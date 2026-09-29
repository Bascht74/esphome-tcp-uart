#include "tcp_uart.h"

#include "esphome/core/log.h"

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/tcp.h>

#ifdef USE_ESP32
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#else
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace esphome {
namespace tcp_uart {

static const char *const TAG = "tcp_uart";

float TcpUart::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void TcpUart::set_link_up_(bool up) {
  if (this->connected_ == up) {
    return;
  }
  this->connected_ = up;
  if (up) {
    this->last_io_ms_ = millis();
  }
  this->publish_link_();
}

void TcpUart::publish_link_() {
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(this->connected_);
  }
}

void TcpUart::publish_address_(const std::string &ip) {
  if (this->address_sensor_ == nullptr || ip.empty()) {
    return;
  }
  std::string next = ip + ":" + std::to_string(this->port_);
  if (next == this->address_) {
    return;
  }
  this->address_ = std::move(next);
  this->address_sensor_->publish_state(this->address_);
}

void TcpUart::clear_address_() {
  if (this->address_sensor_ == nullptr || this->address_.empty()) {
    return;
  }
  this->address_.clear();
  this->address_sensor_->publish_state("");
}

void TcpUart::publish_peer_(const struct sockaddr *addr) {
  if (addr == nullptr || addr->sa_family != AF_INET) {
    return;
  }
  char buf[INET_ADDRSTRLEN];
  auto *in = reinterpret_cast<const struct sockaddr_in *>(addr);
  if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf)) == nullptr) {
    return;
  }
  this->publish_address_(buf);
}

void TcpUart::setup() {
  this->publish_link_();
  if (this->drop_sensor_ != nullptr) {
    this->drop_sensor_->publish_state(0);
  }
}

void TcpUart::dump_config() {
  ESP_LOGCONFIG(TAG, "TCP UART:");
  ESP_LOGCONFIG(TAG, "  Role: %s", this->server_ ? "server" : "client");
  if (this->server_) {
    ESP_LOGCONFIG(TAG, "  Listen: %u", this->port_);
  } else {
    ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  }
  ESP_LOGCONFIG(TAG, "  Protocol: %s", this->modbus_ ? "modbus" : "raw");
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
  this->set_link_up_(false);
  this->rx_.clear();
  this->tx_.clear();
  this->tcp_buf_.clear();
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

#ifdef USE_ESP32
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
  if (socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&literal), sizeof(literal), this->host_, this->port_) !=
      0) {
    this->resolved_ip_ = this->host_;
    this->have_addr_.store(true);
    return;
  }
#ifdef USE_ESP32
  ip_addr_t cached;
  err_t err = dns_gethostbyname(this->host_.c_str(), &cached, &TcpUart::dns_found_, this);
  if (err == ERR_OK && IP_IS_V4(&cached)) {
    this->resolved_addr_.store(ip4_addr_get_u32(ip_2_ip4(&cached)));
    this->have_addr_.store(true);
    return;
  }
  if (err == ERR_INPROGRESS) {
    this->resolving_.store(true);
    return;
  }
#else
  struct addrinfo hints {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  if (getaddrinfo(this->host_.c_str(), nullptr, &hints, &res) == 0 && res != nullptr) {
    char buf[INET_ADDRSTRLEN];
    auto *in = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf)) != nullptr) {
      this->resolved_ip_ = buf;
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
  if (!this->resolved_ip_.empty()) {
    this->publish_address_(this->resolved_ip_);
    return true;
  }
  if (!this->have_addr_.load()) {
    return false;
  }
  struct in_addr addr {};
  addr.s_addr = this->resolved_addr_.load();
  char buf[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == nullptr) {
    return false;
  }
  this->resolved_ip_ = buf;
  this->publish_address_(this->resolved_ip_);
  return true;
}

void TcpUart::try_connect_() {
  if (this->sock_ != nullptr || millis() < this->next_connect_ms_) {
    return;
  }
  if (this->resolve_failed_.exchange(false)) {
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  this->try_resolve_();
  if (!this->ip_ready_()) {
    return;
  }
  struct sockaddr_storage dest;
  socklen_t dest_len =
      socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest), this->resolved_ip_, this->port_);
  if (dest_len == 0) {
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  this->sock_ = socket::socket(dest.ss_family, SOCK_STREAM, IPPROTO_TCP);
  if (this->sock_ == nullptr) {
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  this->apply_socket_options_(this->sock_.get());
  int rc = this->sock_->connect(reinterpret_cast<struct sockaddr *>(&dest), dest_len);
  if (rc == 0 || errno == EINPROGRESS) {
    this->connecting_ = rc != 0;
    this->set_link_up_(rc == 0);
    return;
  }
  this->sock_.reset();
  this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
}

void TcpUart::try_listen_() {
  if (this->listen_ != nullptr || millis() < this->next_connect_ms_) {
    return;
  }
  this->listen_ = socket::socket_ip_loop_monitored(SOCK_STREAM, IPPROTO_TCP);
  if (this->listen_ == nullptr) {
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  int yes = 1;
  this->listen_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  this->listen_->setblocking(false);
  struct sockaddr_storage local;
  socklen_t local_len = socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&local), sizeof(local), this->port_);
  if (local_len == 0 || this->listen_->bind(reinterpret_cast<struct sockaddr *>(&local), local_len) != 0 ||
      this->listen_->listen(1) != 0) {
    ESP_LOGW(TAG, "Listen on %u failed", this->port_);
    this->listen_.reset();
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
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
  auto client = this->listen_->accept(reinterpret_cast<struct sockaddr *>(&peer), &peer_len);
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
    socklen_t len = sizeof(err);
    if (this->sock_->getsockopt(SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
      this->close_sock_();
      this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
      return;
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
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  if (count > 0 && this->modbus_) {
    this->note_io_();
    this->tcp_buf_.insert(this->tcp_buf_.end(), tmp, tmp + count);
    this->extract_frames_();
    return;
  }
  for (ssize_t i = 0; i < count && this->rx_.size() < 1024; i++) {
    this->rx_.push_back(tmp[i]);
  }
  if (count > 0) {
    this->note_io_();
  }
}

void TcpUart::send_bytes_(const uint8_t *data, size_t len) {
  if (!this->connected_ || this->sock_ == nullptr || len == 0) {
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
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
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

void TcpUart::note_io_() { this->last_io_ms_ = millis(); }

void TcpUart::check_idle_() {
  if (!this->connected_ || this->connecting_) {
    return;
  }
  uint32_t limit = this->server_ ? this->idle_timeout_ms_ : this->stall_timeout_ms_;
  if (limit == 0 || this->last_io_ms_ == 0) {
    return;
  }
  if (millis() - this->last_io_ms_ < limit) {
    return;
  }
  ESP_LOGW(TAG, "Link idle, closing");
  this->close_sock_();
  this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
}

void TcpUart::add_allowed(const std::string &host) {
  struct sockaddr_storage addr;
  if (socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr), host, 0) == 0) {
    ESP_LOGW(TAG, "Ignored allowed host %s", host.c_str());
    return;
  }
  if (addr.ss_family != AF_INET) {
    return;
  }
  this->allowed_.push_back(reinterpret_cast<struct sockaddr_in *>(&addr)->sin_addr.s_addr);
}

bool TcpUart::peer_allowed_(const struct sockaddr *addr) {
  if (this->allowed_.empty()) {
    return true;
  }
  if (addr == nullptr || addr->sa_family != AF_INET) {
    return false;
  }
  uint32_t ip = reinterpret_cast<const struct sockaddr_in *>(addr)->sin_addr.s_addr;
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
    this->try_listen_();
    this->accept_client_();
  } else {
    this->try_connect_();
  }
  this->read_socket_();
  this->check_idle_();
}

void TcpUart::push_rx_(uint8_t byte) {
  if (this->rx_.size() < 512) {
    this->rx_.push_back(byte);
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
  while (this->tcp_buf_.size() >= 7) {
    if (this->server_ && this->response_pending_) {
      return;
    }
    uint16_t txn = (this->tcp_buf_[0] << 8) | this->tcp_buf_[1];
    uint16_t proto = (this->tcp_buf_[2] << 8) | this->tcp_buf_[3];
    uint16_t length = (this->tcp_buf_[4] << 8) | this->tcp_buf_[5];
    if (proto != 0 || length < 2 || length > 254) {
      this->tcp_buf_.erase(this->tcp_buf_.begin());
      continue;
    }
    if (this->tcp_buf_.size() < 6u + length) {
      return;
    }
    if (!this->server_ && txn != this->txn_) {
      ESP_LOGW(TAG, "Dropped transaction %u, expected %u", txn, this->txn_);
      this->tcp_buf_.erase(this->tcp_buf_.begin(), this->tcp_buf_.begin() + 6 + length);
      continue;
    }
    uint8_t unit = this->tcp_buf_[6];
    const uint8_t *pdu = this->tcp_buf_.data() + 7;
    size_t pdu_len = length - 1;
    std::vector<uint8_t> rtu;
    rtu.reserve(pdu_len + 3);
    rtu.push_back(unit);
    rtu.insert(rtu.end(), pdu, pdu + pdu_len);
    uint16_t crc = crc16(rtu.data(), rtu.size());
    rtu.push_back(crc & 0xFF);
    rtu.push_back(crc >> 8);
    for (uint8_t byte : rtu) {
      this->push_rx_(byte);
    }
    if (this->server_) {
      this->last_request_txn_ = txn;
      this->response_pending_ = true;
    }
    this->tcp_buf_.erase(this->tcp_buf_.begin(), this->tcp_buf_.begin() + 6 + length);
  }
}

void TcpUart::send_rtu_frame_() {
  if (this->tx_.size() < 4 || !this->connected_) {
    this->tx_.clear();
    return;
  }
  const uint8_t *pdu = this->tx_.data() + 1;
  size_t pdu_len = this->tx_.size() - 3;
  uint8_t unit = this->tx_[0];
  uint16_t txn = this->last_request_txn_;
  if (!this->server_) {
    this->txn_ = this->txn_ == 0xFFFF ? 1 : static_cast<uint16_t>(this->txn_ + 1);
    txn = this->txn_;
  }
  uint16_t length = pdu_len + 1;
  std::vector<uint8_t> frame;
  frame.reserve(7 + pdu_len);
  frame.push_back(txn >> 8);
  frame.push_back(txn & 0xFF);
  frame.push_back(0);
  frame.push_back(0);
  frame.push_back(length >> 8);
  frame.push_back(length & 0xFF);
  frame.push_back(unit);
  frame.insert(frame.end(), pdu, pdu + pdu_len);
  this->send_bytes_(frame.data(), frame.size());
  this->tx_.clear();
  this->response_pending_ = false;
}

void TcpUart::write_array(const uint8_t *data, size_t len) {
  if (this->modbus_) {
    this->tx_.insert(this->tx_.end(), data, data + len);
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
    this->rx_.pop_front();
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

}  // namespace tcp_uart
}  // namespace esphome
