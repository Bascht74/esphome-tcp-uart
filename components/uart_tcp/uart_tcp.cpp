#include "uart_tcp.h"

#include "esphome/core/hal.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/tcp.h>

#ifdef USE_ESP32
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#else
#include <netdb.h>
#endif

namespace esphome {
namespace uart_tcp {

static const char *const TAG = "uart_tcp";

uint32_t loop_time() { return App.get_loop_component_start_time(); }

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

float UartTcp::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void UartTcp::set_link_up_(bool up) {
  if (this->connected_ == up) {
    return;
  }
  this->connected_ = up;
  if (up) {
    this->last_io_ms_ = loop_time();
  }
  this->publish_link_();
}

void UartTcp::publish_link_() {
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(this->connected_);
  }
}

void UartTcp::publish_address_(const char *ip) {
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

void UartTcp::clear_address_() {
  if (this->address_sensor_ == nullptr || this->address_[0] == '\0') {
    return;
  }
  this->address_[0] = '\0';
  this->address_sensor_->publish_state("");
}

void UartTcp::publish_peer_(const struct sockaddr *addr) {
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

void UartTcp::setup() {
  this->tcp_buf_.reserve(300);
  if (this->parent_ != nullptr) {
    this->set_baud_rate(this->parent_->get_baud_rate());
  }
  this->set_data_bits(8);
  this->set_stop_bits(1);
  this->publish_link_();
  if (this->drop_sensor_ != nullptr) {
    this->drop_sensor_->publish_state(0);
  }
}

void UartTcp::dump_config() {
  ESP_LOGCONFIG(TAG, "UART TCP bridge:");
  ESP_LOGCONFIG(TAG, "  Role: %s", this->server_ ? LOG_STR_LITERAL("server") : LOG_STR_LITERAL("client"));
  ESP_LOGCONFIG(TAG, "  Protocol: %s", this->modbus_ ? LOG_STR_LITERAL("modbus") : LOG_STR_LITERAL("raw"));
  if (this->server_) {
    ESP_LOGCONFIG(TAG, "  Listen: %u", this->port_);
  } else {
    ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  }
  ESP_LOGCONFIG(TAG, "  UART baud: %" PRIu32, this->parent_->get_baud_rate());
}

void UartTcp::on_shutdown() {
  this->close_sock_();
  this->close_listen_();
  this->close_taps_();
}

void UartTcp::close_sock_() {
  bool was = this->connected_;
  if (this->sock_ != nullptr) {
    this->sock_->shutdown(SHUT_RDWR);
    this->sock_->close();
    this->sock_.reset();
  }
  this->connecting_ = false;
  this->set_link_up_(false);
  this->tcp_buf_.clear();
  this->uart_buf_.clear();
  this->wait_uart_ = false;
  this->wait_local_ = false;
  this->wait_tcp_ = false;
  if (this->server_) {
    this->clear_address_();
  }
  if (was) {
    this->note_drop_();
  }
}

void UartTcp::close_listen_() { this->listen_.reset(); }

void UartTcp::apply_socket_options_(socket::Socket *sock) {
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
void UartTcp::dns_found_(const char *name, const ip_addr_t *addr, void *arg) {
  auto *self = static_cast<UartTcp *>(arg);
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

void UartTcp::try_resolve_() {
  if (this->have_addr_.load() || this->resolving_.load()) {
    return;
  }
  struct sockaddr_storage literal;
  if (socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&literal), sizeof(literal), this->host_.c_str(),
                           this->port_) != 0) {
    snprintf(this->resolved_ip_, sizeof(this->resolved_ip_), "%s", this->host_.c_str());
    this->have_addr_.store(true);
    return;
  }
#ifdef USE_ESP32
  ip_addr_t cached;
  err_t err = dns_gethostbyname(this->host_.c_str(), &cached, &UartTcp::dns_found_, this);
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
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  if (getaddrinfo(this->host_.c_str(), nullptr, &hints, &res) == 0 && res != nullptr) {
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

bool UartTcp::ip_ready_() {
  if (this->resolved_ip_[0] != '\0') {
    this->publish_address_(this->resolved_ip_);
    return true;
  }
  if (!this->have_addr_.load()) {
    return false;
  }
  struct in_addr addr{};
  addr.s_addr = this->resolved_addr_.load();
  char buf[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == nullptr) {
    return false;
  }
  snprintf(this->resolved_ip_, sizeof(this->resolved_ip_), "%s", buf);
  this->publish_address_(this->resolved_ip_);
  return true;
}

void UartTcp::try_connect_() {
  if (this->sock_ != nullptr || loop_time() < this->next_connect_ms_) {
    return;
  }
  if (this->resolve_failed_.exchange(false)) {
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
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
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
    return;
  }
  this->sock_ = socket::socket(dest.ss_family, SOCK_STREAM, IPPROTO_TCP);
  if (this->sock_ == nullptr) {
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
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
  this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
}

void UartTcp::try_listen_() {
  if (this->listen_ != nullptr || loop_time() < this->next_connect_ms_) {
    return;
  }
  this->listen_ = socket::socket_ip_loop_monitored(SOCK_STREAM, IPPROTO_TCP);
  if (this->listen_ == nullptr) {
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
    return;
  }
  int yes = 1;
  this->listen_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  this->listen_->setblocking(false);
  struct sockaddr_storage local;
  socklen_t local_len =
      socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&local), sizeof(local), this->port_);
  if (local_len == 0 || this->listen_->bind(reinterpret_cast<struct sockaddr *>(&local), local_len) != 0 ||
      this->listen_->listen(1) != 0) {
    ESP_LOGW(TAG, "Listen on %u failed", this->port_);
    this->listen_.reset();
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
    return;
  }
  ESP_LOGI(TAG, "Listening on %u", this->port_);
}

void UartTcp::accept_client_() {
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
  this->tcp_buf_.clear();
  this->publish_peer_(reinterpret_cast<struct sockaddr *>(&peer));
  ESP_LOGI(TAG, "Client connected");
}

void UartTcp::read_socket_() {
  if (this->sock_ == nullptr) {
    return;
  }
  if (this->connecting_) {
    int err = 0;
    socklen_t len = sizeof(err);
    if (this->sock_->getsockopt(SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
      this->close_sock_();
      this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
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
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
    return;
  }
  if (count > 0) {
    this->note_io_();
    this->tcp_buf_.insert(this->tcp_buf_.end(), tmp, tmp + count);
  }
}

void UartTcp::send_all_(const uint8_t *data, size_t len) {
  if (!this->connected_ || this->sock_ == nullptr || len == 0) {
    return;
  }
  size_t sent_total = 0;
  while (sent_total < len) {
    ssize_t sent = this->sock_->write(data + sent_total, len - sent_total);
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }
    if (sent <= 0) {
      ESP_LOGW(TAG, "Send failed");
      this->close_sock_();
      this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
      return;
    }
    sent_total += static_cast<size_t>(sent);
  }
  this->note_io_();
}

void UartTcp::pull_uart_() {
  uint8_t tmp[128];
  while (this->parent_->available() > 0 && this->uart_buf_.size() < 512) {
    size_t want = std::min(this->parent_->available(), sizeof(tmp));
    if (!this->parent_->read_array(tmp, want)) {
      break;
    }
    this->uart_buf_.insert(this->uart_buf_.end(), tmp, tmp + want);
    this->last_uart_us_ = micros();
  }
}

uint32_t UartTcp::frame_gap_us_() const {
  uint32_t baud = std::max<uint32_t>(1u, this->parent_->get_baud_rate());
  uint8_t data_bits = this->parent_->get_data_bits();
  uint8_t stop_bits = this->parent_->get_stop_bits();
  if (data_bits == 0) {
    data_bits = 8;
  }
  if (stop_bits == 0) {
    stop_bits = 1;
  }
  uint8_t bits = static_cast<uint8_t>(1 + data_bits + stop_bits);
  if (this->parent_->get_parity() != uart::UART_CONFIG_PARITY_NONE) {
    bits++;
  }
  return std::max<uint32_t>(2000u, static_cast<uint32_t>(3.5f * bits * 1000000.0f / baud) + 1);
}

void UartTcp::pump_raw_() {
  if (!this->tcp_buf_.empty()) {
    this->send_tap_(this->tcp_buf_.data(), this->tcp_buf_.size());
    this->parent_->write_array(this->tcp_buf_.data(), this->tcp_buf_.size());
    this->tcp_buf_.clear();
  }
  this->pull_uart_();
  if (!this->uart_buf_.empty()) {
    this->send_tap_(this->uart_buf_.data(), this->uart_buf_.size());
    this->send_all_(this->uart_buf_.data(), this->uart_buf_.size());
    this->uart_buf_.clear();
  }
}

bool UartTcp::take_mbap_(std::vector<uint8_t> *pdu, uint8_t *unit, uint16_t *txn) {
  if (this->tcp_buf_.size() < 7) {
    return false;
  }
  uint16_t proto = (this->tcp_buf_[2] << 8) | this->tcp_buf_[3];
  uint16_t length = (this->tcp_buf_[4] << 8) | this->tcp_buf_[5];
  if (proto != 0 || length < 2 || length > 254) {
    this->tcp_buf_.erase(this->tcp_buf_.begin());
    return false;
  }
  if (this->tcp_buf_.size() < 6u + length) {
    return false;
  }
  *txn = (this->tcp_buf_[0] << 8) | this->tcp_buf_[1];
  *unit = this->tcp_buf_[6];
  pdu->assign(this->tcp_buf_.begin() + 7, this->tcp_buf_.begin() + 6 + length);
  this->tcp_buf_.erase(this->tcp_buf_.begin(), this->tcp_buf_.begin() + 6 + length);
  return true;
}

bool UartTcp::take_rtu_(std::vector<uint8_t> *pdu, uint8_t *unit) {
  if (this->uart_buf_.size() < 4) {
    return false;
  }
  if (micros() - this->last_uart_us_ < this->frame_gap_us_()) {
    return false;
  }
  uint16_t got = this->uart_buf_[this->uart_buf_.size() - 2] | (this->uart_buf_[this->uart_buf_.size() - 1] << 8);
  uint16_t expect = crc16(this->uart_buf_.data(), this->uart_buf_.size() - 2);
  if (got != expect) {
    ESP_LOGW(TAG, "RTU CRC mismatch");
    this->uart_buf_.clear();
    return false;
  }
  *unit = this->uart_buf_[0];
  pdu->assign(this->uart_buf_.begin() + 1, this->uart_buf_.end() - 2);
  this->uart_buf_.clear();
  return true;
}

void UartTcp::write_rtu_(const uint8_t *pdu, size_t pdu_len, uint8_t unit) {
  std::vector<uint8_t> frame;
  frame.reserve(pdu_len + 3);
  frame.push_back(unit);
  frame.insert(frame.end(), pdu, pdu + pdu_len);
  uint16_t crc = crc16(frame.data(), frame.size());
  frame.push_back(crc & 0xFF);
  frame.push_back(crc >> 8);
  this->send_tap_(frame.data(), frame.size());
  this->parent_->write_array(frame.data(), frame.size());
  this->parent_->flush();
}

void UartTcp::send_mbap_(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
  uint8_t header[7];
  uint16_t length = pdu_len + 1;
  header[0] = txn >> 8;
  header[1] = txn & 0xFF;
  header[2] = 0;
  header[3] = 0;
  header[4] = length >> 8;
  header[5] = length & 0xFF;
  header[6] = unit;
  this->send_all_(header, 7);
  this->send_all_(pdu, pdu_len);
}

void UartTcp::send_gateway_fail_(uint16_t txn, uint8_t unit, uint8_t function) {
  uint8_t pdu[2] = {static_cast<uint8_t>(function | 0x80), 0x0B};
  this->send_mbap_(txn, unit, pdu, 2);
}

void UartTcp::pump_modbus_() {
  if (this->wait_uart_ || this->wait_local_) {
    this->pull_uart_();
    std::vector<uint8_t> pdu;
    uint8_t unit = 0;
    if (this->take_rtu_(&pdu, &unit)) {
      std::vector<uint8_t> frame;
      frame.reserve(pdu.size() + 3);
      frame.push_back(unit);
      frame.insert(frame.end(), pdu.begin(), pdu.end());
      uint16_t crc = crc16(frame.data(), frame.size());
      frame.push_back(crc & 0xFF);
      frame.push_back(crc >> 8);
      this->send_tap_(frame.data(), frame.size());
      if (this->wait_local_) {
        this->push_local_(frame.data(), frame.size());
        this->wait_local_ = false;
      } else {
        this->send_mbap_(this->pending_txn_, unit, pdu.data(), pdu.size());
        this->wait_uart_ = false;
      }
    } else if (loop_time() - this->wait_started_ms_ > this->response_timeout_ms_) {
      ESP_LOGW(TAG, "RTU response timeout");
      this->uart_buf_.clear();
      if (this->wait_uart_) {
        this->send_gateway_fail_(this->pending_txn_, this->pending_unit_, this->pending_function_);
      }
      this->wait_uart_ = false;
      this->wait_local_ = false;
    }
    return;
  }
  if (this->wait_tcp_) {
    std::vector<uint8_t> pdu;
    uint8_t unit = 0;
    uint16_t txn = 0;
    if (this->take_mbap_(&pdu, &unit, &txn)) {
      if (txn == this->pending_txn_) {
        this->write_rtu_(pdu.data(), pdu.size(), unit);
      }
      this->wait_tcp_ = false;
    } else if (loop_time() - this->wait_started_ms_ > this->response_timeout_ms_) {
      ESP_LOGW(TAG, "TCP response timeout");
      this->tcp_buf_.clear();
      this->wait_tcp_ = false;
    }
    return;
  }
  if (this->server_) {
    if (this->local_frame_ready_()) {
      this->start_local_();
      return;
    }
    if (!this->connected_) {
      return;
    }
    std::vector<uint8_t> pdu;
    uint8_t unit = 0;
    uint16_t txn = 0;
    if (!this->take_mbap_(&pdu, &unit, &txn) || pdu.empty()) {
      return;
    }
    this->pending_txn_ = txn;
    this->pending_unit_ = unit;
    this->pending_function_ = pdu[0];
    this->uart_buf_.clear();
    this->write_rtu_(pdu.data(), pdu.size(), unit);
    this->wait_uart_ = true;
    this->wait_started_ms_ = loop_time();
    this->last_uart_us_ = micros();
    return;
  }
  this->pull_uart_();
  std::vector<uint8_t> pdu;
  uint8_t unit = 0;
  if (!this->take_rtu_(&pdu, &unit) || pdu.empty()) {
    return;
  }
  this->txn_ = this->txn_ == 0xFFFF ? 1 : static_cast<uint16_t>(this->txn_ + 1);
  this->pending_txn_ = this->txn_;
  this->pending_function_ = pdu[0];
  this->send_mbap_(this->pending_txn_, unit, pdu.data(), pdu.size());
  this->wait_tcp_ = true;
  this->wait_started_ms_ = loop_time();
}

void UartTcp::write_array(const uint8_t *data, size_t len) {
  if (!this->server_ || !this->modbus_ || len == 0) {
    return;
  }
  this->local_tx_.insert(this->local_tx_.end(), data, data + len);
  if (this->local_tx_.size() > 300) {
    this->local_tx_.clear();
  }
  this->last_local_us_ = micros();
}

bool UartTcp::peek_byte(uint8_t *data) {
  if (this->local_rx_.empty()) {
    return false;
  }
  *data = this->local_rx_.front();
  return true;
}

bool UartTcp::read_array(uint8_t *data, size_t len) {
  if (this->local_rx_.size() < len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    data[i] = this->local_rx_.front();
    this->local_rx_.pop();
  }
  return true;
}

size_t UartTcp::available() { return this->local_rx_.size(); }

uart::UARTFlushResult UartTcp::flush() {
  if (this->local_tx_.size() >= 4) {
    this->last_local_us_ = micros() - this->frame_gap_us_() - 1;
  }
  return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
}

void UartTcp::note_drop_() {
  this->drops_++;
  if (this->drop_sensor_ != nullptr) {
    this->drop_sensor_->publish_state(this->drops_);
  }
}

void UartTcp::note_io_() { this->last_io_ms_ = loop_time(); }

void UartTcp::check_idle_() {
  if (!this->connected_ || this->connecting_) {
    return;
  }
  uint32_t limit = this->server_ ? this->idle_timeout_ms_ : this->stall_timeout_ms_;
  if (limit == 0 || this->last_io_ms_ == 0) {
    return;
  }
  if (loop_time() - this->last_io_ms_ < limit) {
    return;
  }
  ESP_LOGW(TAG, "Link idle, closing");
  this->close_sock_();
  this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
}

void UartTcp::add_allowed(const char *host) {
  struct sockaddr_storage addr;
  if (socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr), host, 0) == 0) {
    ESP_LOGW(TAG, "Ignored allowed host %s", host);
    return;
  }
  if (addr.ss_family != AF_INET) {
    return;
  }
  this->allowed_.push_back(reinterpret_cast<struct sockaddr_in *>(&addr)->sin_addr.s_addr);
}

bool UartTcp::peer_allowed_(const struct sockaddr *addr) {
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

void UartTcp::send_tap_(const uint8_t *data, size_t len) {
  if (len == 0) {
    return;
  }
  for (size_t i = 0; i < this->taps_.size();) {
    ssize_t sent = this->taps_[i]->write(data, len);
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      i++;
      continue;
    }
    if (sent <= 0) {
      this->taps_[i]->close();
      this->taps_.erase(this->taps_.begin() + i);
      continue;
    }
    i++;
  }
}

void UartTcp::try_listen_tap_() {
  if (this->tap_port_ == 0 || this->listen_tap_ != nullptr || loop_time() < this->next_connect_ms_) {
    return;
  }
  this->listen_tap_ = socket::socket_ip_loop_monitored(SOCK_STREAM, IPPROTO_TCP);
  if (this->listen_tap_ == nullptr) {
    return;
  }
  int yes = 1;
  this->listen_tap_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  this->listen_tap_->setblocking(false);
  struct sockaddr_storage local;
  socklen_t local_len =
      socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&local), sizeof(local), this->tap_port_);
  if (local_len == 0 || this->listen_tap_->bind(reinterpret_cast<struct sockaddr *>(&local), local_len) != 0 ||
      this->listen_tap_->listen(2) != 0) {
    ESP_LOGW(TAG, "Listen on tap %u failed", this->tap_port_);
    this->listen_tap_.reset();
    return;
  }
  ESP_LOGI(TAG, "Tap listening on %u", this->tap_port_);
}

void UartTcp::accept_tap_() {
  if (this->listen_tap_ == nullptr || this->taps_.size() >= 2) {
    return;
  }
  struct sockaddr_storage peer;
  socklen_t peer_len = sizeof(peer);
  auto client = this->listen_tap_->accept(reinterpret_cast<struct sockaddr *>(&peer), &peer_len);
  if (client == nullptr) {
    return;
  }
  if (!this->peer_allowed_(reinterpret_cast<struct sockaddr *>(&peer))) {
    client->close();
    return;
  }
  client->setblocking(false);
  this->taps_.push_back(std::move(client));
  ESP_LOGI(TAG, "Tap connected");
}

void UartTcp::drain_taps_() {
  uint8_t tmp[64];
  for (size_t i = 0; i < this->taps_.size();) {
    ssize_t count = this->taps_[i]->read(tmp, sizeof(tmp));
    if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
      this->taps_[i]->close();
      this->taps_.erase(this->taps_.begin() + i);
      continue;
    }
    i++;
  }
}

void UartTcp::close_taps_() {
  for (auto &tap : this->taps_) {
    if (tap != nullptr) {
      tap->close();
    }
  }
  this->taps_.clear();
  this->listen_tap_.reset();
}

bool UartTcp::local_frame_ready_() {
  return this->local_tx_.size() >= 4 && micros() - this->last_local_us_ >= this->frame_gap_us_();
}

void UartTcp::start_local_() {
  uint16_t got = this->local_tx_[this->local_tx_.size() - 2] | (this->local_tx_[this->local_tx_.size() - 1] << 8);
  uint16_t expect = crc16(this->local_tx_.data(), this->local_tx_.size() - 2);
  if (got != expect) {
    ESP_LOGW(TAG, "Local RTU CRC mismatch");
    this->local_tx_.clear();
    return;
  }
  this->pending_function_ = this->local_tx_[1];
  this->send_tap_(this->local_tx_.data(), this->local_tx_.size());
  this->parent_->write_array(this->local_tx_.data(), this->local_tx_.size());
  this->parent_->flush();
  this->local_tx_.clear();
  this->uart_buf_.clear();
  this->wait_local_ = true;
  this->wait_started_ms_ = loop_time();
  this->last_uart_us_ = micros();
}

void UartTcp::push_local_(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (this->local_rx_.size() >= 512) {
      this->local_rx_.pop();
    }
    this->local_rx_.push(data[i]);
  }
}

void UartTcp::loop() {
  if (this->server_) {
    this->try_listen_();
    this->accept_client_();
    this->try_listen_tap_();
    this->accept_tap_();
    this->drain_taps_();
  } else {
    this->try_connect_();
  }
  this->read_socket_();
  this->check_idle_();
  if (this->modbus_ && (this->connected_ || this->server_)) {
    this->pump_modbus_();
  } else if (this->connected_) {
    this->pump_raw_();
  }
}

}  // namespace uart_tcp
}  // namespace esphome
