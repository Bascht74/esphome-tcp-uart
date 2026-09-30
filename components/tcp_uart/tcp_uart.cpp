#include "tcp_uart.h"
#include "connect_poll.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/tcp.h>

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#else
#include <netdb.h>
#endif

namespace esphome {
namespace tcp_uart {

static const char *const TAG = "tcp_uart";

static uint32_t loop_time() { return App.get_loop_component_start_time(); }

static void append_buf(uint8_t *buf, size_t *len, size_t cap, const uint8_t *src,
                size_t n) {
  if (*len >= cap || n == 0) {
    return;
  }
  if (n > cap - *len) {
    n = cap - *len;
  }
  std::memcpy(buf + *len, src, n);
  *len += n;
}

static bool format_ipv4(uint32_t raw, char *dest, size_t dest_len) {
#if defined(USE_HOST) || defined(USE_ZEPHYR)
  struct in_addr addr{};
  addr.s_addr = raw;
  return inet_ntop(AF_INET, &addr, dest, dest_len) != nullptr;
#else
  ip4_addr_t addr;
  ip4_addr_set_u32(&addr, raw);
  return ip4addr_ntoa_r(&addr, dest, static_cast<int>(dest_len)) != nullptr;
#endif
}

static void consume_buf(uint8_t *buf, size_t *len, size_t n) {
  if (n >= *len) {
    *len = 0;
    return;
  }
  std::memmove(buf, buf + n, *len - n);
  *len -= n;
}

float TcpUart::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void TcpUart::set_link_up_(bool up) {
  if (this->connected_ == up) {
    return;
  }
  this->connected_ = up;
  if (up) {
    this->last_io_ms_ = loop_time();
  } else {
    this->offline_drop_logged_ = false;
  }
  this->publish_link_();
}

void TcpUart::publish_link_() {
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(this->connected_);
  }
}

void TcpUart::publish_address_(const char *ip) {
  if (this->address_sensor_ == nullptr || ip == nullptr || ip[0] == '\0') {
    return;
  }
  char next[sizeof(this->address_)];
  snprintf(next, sizeof(next), "%s:%u", ip, this->port_);
  if (strcmp(next, this->address_) == 0) {
    return;
  }
  snprintf(this->address_, sizeof(this->address_), "%s", next);
  this->address_sensor_->publish_state(this->address_);
}

void TcpUart::clear_address_() {
  if (this->address_sensor_ == nullptr || this->address_[0] == '\0') {
    return;
  }
  this->address_[0] = '\0';
  this->address_sensor_->publish_state("");
}

void TcpUart::publish_peer_(const struct sockaddr *addr) {
  if (addr == nullptr || addr->sa_family != AF_INET) {
    return;
  }
  char buf[INET_ADDRSTRLEN];
  auto *in = reinterpret_cast<const struct sockaddr_in *>(addr);
  if (!format_ipv4(in->sin_addr.s_addr, buf, sizeof(buf))) {
    return;
  }
  this->publish_address_(buf);
}

void TcpUart::setup() {
  this->last_attempt_ms_ = loop_time() - this->reconnect_interval_ms_;
  this->publish_link_();
  if (this->drop_sensor_ != nullptr) {
    this->drop_sensor_->publish_state(0);
  }
}

void TcpUart::dump_config() {
  if (this->server_) {
    ESP_LOGCONFIG(TAG,
                  "TCP UART:\n"
                  "  Role: %s\n"
                  "  Listen: %u\n"
                  "  Protocol: %s\n"
                  "  Reconnect Interval: %" PRIu32 "ms",
                  LOG_STR_LITERAL("server"), this->port_,
                  this->modbus_ ? LOG_STR_LITERAL("modbus") : LOG_STR_LITERAL("raw"), this->reconnect_interval_ms_);
    return;
  }
  ESP_LOGCONFIG(TAG,
                "TCP UART:\n"
                "  Role: %s\n"
                "  Host: %s:%u\n"
                "  Protocol: %s\n"
                "  Reconnect Interval: %" PRIu32 "ms",
                LOG_STR_LITERAL("client"), this->host_.c_str(), this->port_,
                this->modbus_ ? LOG_STR_LITERAL("modbus") : LOG_STR_LITERAL("raw"), this->reconnect_interval_ms_);
}

void TcpUart::on_shutdown() {
  this->close_sock_();
  this->close_listen_();
}

void TcpUart::close_sock_() {
  bool was = this->connected_;
  if (this->sock_ != nullptr) {
    this->sock_->shutdown(SHUT_RDWR);
    this->sock_->close();
    this->sock_.reset();
  }
  this->connecting_ = false;
  this->rx_pending_ = false;
  this->set_link_up_(false);
  if (!this->server_) {
    this->forget_addr_();
  }
  this->rx_.clear();
  this->tx_len_ = 0;
  this->tcp_len_ = 0;
  this->response_pending_ = false;
  if (this->server_) {
    this->clear_address_();
  }
  if (was) {
    this->note_drop_();
  }
}

void TcpUart::close_listen_() { this->listen_.reset(); }

void TcpUart::apply_socket_options_(socket::Socket *sock) {
  int yes = 1;
  sock->setblocking(false);
  sock->setsockopt(IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
  sock->setsockopt(SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
#ifdef TCP_KEEPIDLE
  int idle = 30;
  int interval = 10;
  int count = 3;
  sock->setsockopt(IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
  sock->setsockopt(IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
  sock->setsockopt(IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
}

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
void TcpUart::dns_found_(const char *name, const ip_addr_t *addr, void *arg) {
  auto *self = static_cast<TcpUart *>(arg);
  if (addr != nullptr && IP_IS_V4(addr)) {
    self->resolved_addr_.store(ip4_addr_get_u32(ip_2_ip4(addr)));
    self->have_addr_.store(true);
  } else {
    self->resolve_failed_.store(true);
    ESP_LOGW(TAG, "DNS failed for %s", name);
  }
  self->resolving_.store(false);
}
#endif

void TcpUart::try_resolve_() {
  if (this->have_addr_.load() || this->resolving_.load()) {
    return;
  }
  struct sockaddr_storage literal;
  if (socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&literal),
                           sizeof(literal), this->host_.c_str(),
                           this->port_) != 0) {
    snprintf(this->resolved_ip_, sizeof(this->resolved_ip_), "%s",
             this->host_.c_str());
    this->have_addr_.store(true);
    return;
  }
#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
  ip_addr_t cached;
  err_t err;
  {
    LwIPLock lock;
    this->resolving_.store(true);
    err = dns_gethostbyname(this->host_.c_str(), &cached, &TcpUart::dns_found_, this);
    if (err != ERR_INPROGRESS) {
      this->resolving_.store(false);
    }
  }
  if (err == ERR_OK && IP_IS_V4(&cached)) {
    this->resolved_addr_.store(ip4_addr_get_u32(ip_2_ip4(&cached)));
    this->have_addr_.store(true);
    return;
  }
  if (err == ERR_INPROGRESS) {
    return;
  }
#else
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  if (getaddrinfo(this->host_.c_str(), nullptr, &hints, &res) == 0 &&
      res != nullptr) {
    char buf[INET_ADDRSTRLEN];
    auto *in = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf)) != nullptr) {
      snprintf(this->resolved_ip_, sizeof(this->resolved_ip_), "%s", buf);
      this->have_addr_.store(true);
    }
    freeaddrinfo(res);
    if (this->have_addr_.load()) {
      return;
    }
  }
#endif
  this->resolve_failed_.store(true);
}

bool TcpUart::ip_ready_() {
  if (this->resolved_ip_[0] != '\0') {
    this->publish_address_(this->resolved_ip_);
    return true;
  }
  if (!this->have_addr_.load()) {
    return false;
  }
  char buf[16];
  if (!format_ipv4(this->resolved_addr_.load(), buf, sizeof(buf))) {
    return false;
  }
  snprintf(this->resolved_ip_, sizeof(this->resolved_ip_), "%s", buf);
  this->publish_address_(this->resolved_ip_);
  return true;
}

void TcpUart::try_connect_() {
  if (this->sock_ != nullptr || this->in_backoff_()) {
    return;
  }
  if (this->resolve_failed_.load() != 0) {
    this->resolve_failed_.store(false);
    this->forget_addr_();
    this->note_attempt_();
    return;
  }
  this->try_resolve_();
  if (!this->ip_ready_()) {
    return;
  }
  struct sockaddr_storage dest;
  socklen_t dest_len =
      socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&dest),
                           sizeof(dest), this->resolved_ip_, this->port_);
  if (dest_len == 0) {
    this->note_attempt_();
    return;
  }
  this->sock_ = socket::socket_loop_monitored(dest.ss_family, SOCK_STREAM, IPPROTO_TCP);
  if (this->sock_ == nullptr) {
    this->note_attempt_();
    return;
  }
  this->apply_socket_options_(this->sock_.get());
  int rc = this->sock_->connect(reinterpret_cast<struct sockaddr *>(&dest),
                                dest_len);
  if (rc == 0 || errno == EINPROGRESS) {
    this->connecting_ = rc != 0;
    this->set_link_up_(rc == 0);
    return;
  }
  this->sock_.reset();
  this->note_attempt_();
}

void TcpUart::try_listen_() {
  if (this->listen_ != nullptr || this->in_backoff_()) {
    return;
  }
  this->listen_ = socket::socket_ip_loop_monitored(SOCK_STREAM, IPPROTO_TCP);
  if (this->listen_ == nullptr) {
    this->note_attempt_();
    return;
  }
  int yes = 1;
  this->listen_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  this->listen_->setblocking(false);
  struct sockaddr_storage local;
  socklen_t local_len = socket::set_sockaddr_any(
      reinterpret_cast<struct sockaddr *>(&local), sizeof(local), this->port_);
  if (local_len == 0 ||
      this->listen_->bind(reinterpret_cast<struct sockaddr *>(&local),
                          local_len) != 0 ||
      this->listen_->listen(1) != 0) {
    ESP_LOGW(TAG, "Listen on %u failed", this->port_);
    this->listen_.reset();
    this->note_attempt_();
    return;
  }
  ESP_LOGI(TAG, "Listening on %u", this->port_);
}

void TcpUart::accept_client_() {
  if (this->listen_ == nullptr || this->sock_ != nullptr) {
    return;
  }
  struct sockaddr_storage peer;
  socklen_t peer_len = sizeof(peer);
  auto client = this->listen_->accept(
      reinterpret_cast<struct sockaddr *>(&peer), &peer_len);
  if (client == nullptr) {
    return;
  }
  if (!this->peer_allowed_(reinterpret_cast<struct sockaddr *>(&peer))) {
    client->close();
    return;
  }
  this->apply_socket_options_(client.get());
  this->sock_ = std::move(client);
  this->set_link_up_(true);
  this->publish_peer_(reinterpret_cast<struct sockaddr *>(&peer));
  ESP_LOGI(TAG, "Client connected");
}

void TcpUart::read_socket_() {
  if (this->sock_ == nullptr) {
    return;
  }
  if (this->connecting_) {
    int err = 0;
    switch (tcp_link::poll_connect_result(*this->sock_, err)) {
      case tcp_link::ConnectWait::PENDING:
        return;
      case tcp_link::ConnectWait::FAILED:
        ESP_LOGW(TAG, "Connection failed: %d", err);
        this->close_sock_();
        this->note_attempt_();
        return;
      default:
        break;
    }
    this->connecting_ = false;
    this->set_link_up_(true);
    ESP_LOGI(TAG, "Connected to %s:%u", this->host_.c_str(), this->port_);
  }
  uint8_t tmp[128];
  ssize_t count = this->sock_->read(tmp, sizeof(tmp));
  if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
    ESP_LOGW(TAG, "Connection lost");
    this->close_sock_();
    this->note_attempt_();
    return;
  }
  if (count < 0) {
    this->rx_pending_ = false;
    return;
  }
  this->rx_pending_ = static_cast<size_t>(count) == sizeof(tmp);
  if (count > 0 && this->modbus_) {
    this->note_io_();
    append_buf(this->tcp_buf_, &this->tcp_len_, sizeof(this->tcp_buf_), tmp,
               static_cast<size_t>(count));
    this->extract_frames_();
    return;
  }
  for (ssize_t i = 0; i < count && this->rx_.size() < 1024; i++) {
    this->rx_.push(tmp[i]);
  }
  if (count > 0) {
    this->note_io_();
  }
}

void TcpUart::send_bytes_(const uint8_t *data, size_t len) {
  if (!this->connected_ || this->sock_ == nullptr || len == 0) {
    if (len > 0 && !this->offline_drop_logged_) {
      ESP_LOGW(TAG, "Not connected, dropped %u bytes", static_cast<unsigned>(len));
      this->offline_drop_logged_ = true;
    }
    return;
  }
  size_t sent_total = 0;
  while (sent_total < len) {
    ssize_t sent = this->sock_->write(data + sent_total, len - sent_total);
    if (sent > 0) {
      sent_total += static_cast<size_t>(sent);
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    ESP_LOGW(TAG, "Send failed");
    this->close_sock_();
    this->note_attempt_();
    return;
  }
  this->note_io_();
}

void TcpUart::note_drop_() {
  this->drops_++;
  if (this->drop_sensor_ != nullptr) {
    this->drop_sensor_->publish_state(this->drops_);
  }
}

void TcpUart::note_io_() { this->last_io_ms_ = loop_time(); }

void TcpUart::check_idle_() {
  if (!this->connected_ || this->connecting_) {
    return;
  }
  uint32_t limit =
      this->server_ ? this->idle_timeout_ms_ : this->stall_timeout_ms_;
  if (limit == 0 || this->last_io_ms_ == 0) {
    return;
  }
  if (loop_time() - this->last_io_ms_ < limit) {
    return;
  }
  ESP_LOGW(TAG, "Link idle, closing");
  this->close_sock_();
  this->note_attempt_();
}

void TcpUart::add_allowed(const char *host) {
  struct sockaddr_storage addr;
  if (socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&addr),
                           sizeof(addr), host, 0) == 0) {
    ESP_LOGW(TAG, "Ignored allowed host %s", host);
    return;
  }
  if (addr.ss_family != AF_INET) {
    return;
  }
  this->allowed_.push_back(
      reinterpret_cast<struct sockaddr_in *>(&addr)->sin_addr.s_addr);
}

bool TcpUart::peer_allowed_(const struct sockaddr *addr) {
  if (this->allowed_.empty()) {
    return true;
  }
  if (addr == nullptr || addr->sa_family != AF_INET) {
    return false;
  }
  uint32_t ip =
      reinterpret_cast<const struct sockaddr_in *>(addr)->sin_addr.s_addr;
  for (uint32_t allowed : this->allowed_) {
    if (allowed == ip) {
      return true;
    }
  }
  ESP_LOGW(TAG, "Rejected TCP peer");
  return false;
}

void TcpUart::loop() {
  if (this->server_) {
    if (this->listen_ == nullptr && !this->in_backoff_()) {
      this->try_listen_();
    }
    if (this->listen_ != nullptr && this->sock_ == nullptr && this->listen_->ready()) {
      this->accept_client_();
    }
  } else if (this->sock_ == nullptr && !this->in_backoff_()) {
    this->try_connect_();
  }
  if (this->sock_ != nullptr && (this->connecting_ || this->rx_pending_ || this->sock_->ready())) {
    this->read_socket_();
  }
  this->check_idle_();
}

void TcpUart::push_rx_(uint8_t byte) {
  if (this->rx_.size() < 512) {
    this->rx_.push(byte);
  }
}

static uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

void TcpUart::extract_frames_() {
  while (this->tcp_len_ >= 7) {
    if (this->server_ && this->response_pending_) {
      return;
    }
    uint16_t txn = (this->tcp_buf_[0] << 8) | this->tcp_buf_[1];
    uint16_t proto = (this->tcp_buf_[2] << 8) | this->tcp_buf_[3];
    uint16_t length = (this->tcp_buf_[4] << 8) | this->tcp_buf_[5];
    if (proto != 0 || length < 2 || length > 254) {
      consume_buf(this->tcp_buf_, &this->tcp_len_, 1);
      continue;
    }
    if (this->tcp_len_ < 6u + length) {
      return;
    }
    if (!this->server_ && txn != this->txn_) {
      ESP_LOGW(TAG, "Dropped transaction %u, expected %u", txn, this->txn_);
      consume_buf(this->tcp_buf_, &this->tcp_len_, 6u + length);
      continue;
    }
    uint8_t unit = this->tcp_buf_[6];
    const uint8_t *pdu = this->tcp_buf_ + 7;
    size_t pdu_len = length - 1;
    uint8_t rtu[256];
    rtu[0] = unit;
    std::memcpy(rtu + 1, pdu, pdu_len);
    size_t rtu_len = 1 + pdu_len;
    uint16_t crc = crc16(rtu, rtu_len);
    rtu[rtu_len++] = crc & 0xFF;
    rtu[rtu_len++] = crc >> 8;
    for (size_t i = 0; i < rtu_len; i++) {
      this->push_rx_(rtu[i]);
    }
    if (this->server_) {
      this->last_request_txn_ = txn;
      this->response_pending_ = true;
    }
    consume_buf(this->tcp_buf_, &this->tcp_len_, 6u + length);
  }
}

void TcpUart::send_rtu_frame_() {
  if (this->tx_len_ < 4 || !this->connected_) {
    this->tx_len_ = 0;
    return;
  }
  const uint8_t *pdu = this->tx_ + 1;
  size_t pdu_len = this->tx_len_ - 3;
  uint8_t unit = this->tx_[0];
  uint16_t txn = this->last_request_txn_;
  if (!this->server_) {
    this->txn_ =
        this->txn_ == 0xFFFF ? 1 : static_cast<uint16_t>(this->txn_ + 1);
    txn = this->txn_;
  }
  uint16_t length = pdu_len + 1;
  uint8_t frame[260];
  frame[0] = txn >> 8;
  frame[1] = txn & 0xFF;
  frame[2] = 0;
  frame[3] = 0;
  frame[4] = length >> 8;
  frame[5] = length & 0xFF;
  frame[6] = unit;
  std::memcpy(frame + 7, pdu, pdu_len);
  this->send_bytes_(frame, 7 + pdu_len);
  this->tx_len_ = 0;
  this->response_pending_ = false;
}

void TcpUart::write_array(const uint8_t *data, size_t len) {
  if (this->modbus_) {
    // The Modbus component writes one whole RTU frame and does not call
    // flush() unless a flow-control pin is set. Send it here once the CRC matches.
    size_t before = this->tx_len_;
    append_buf(this->tx_, &this->tx_len_, sizeof(this->tx_), data, len);
    if (before == 0 && this->tx_len_ >= 4) {
      uint16_t crc = crc16(this->tx_, this->tx_len_ - 2);
      uint16_t got = this->tx_[this->tx_len_ - 2] |
                     (static_cast<uint16_t>(this->tx_[this->tx_len_ - 1]) << 8);
      if (crc == got) {
        this->send_rtu_frame_();
      }
    }
    return;
  }
  this->send_bytes_(data, len);
}

bool TcpUart::peek_byte(uint8_t *data) {
  if (this->rx_.empty()) {
    return false;
  }
  *data = this->rx_.front();
  return true;
}

bool TcpUart::read_array(uint8_t *data, size_t len) {
  if (this->rx_.size() < len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    data[i] = this->rx_.front();
    this->rx_.pop();
  }
  return true;
}

size_t TcpUart::available() { return this->rx_.size(); }

uart::UARTFlushResult TcpUart::flush() {
  if (this->modbus_) {
    this->send_rtu_frame_();
  }
  return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
}

} // namespace tcp_uart
} // namespace esphome
