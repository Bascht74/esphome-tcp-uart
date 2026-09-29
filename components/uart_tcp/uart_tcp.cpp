#include "uart_tcp.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include "lwip/dns.h"
#include "lwip/ip4_addr.h"

namespace esphome {
namespace uart_tcp {

static const char *const TAG = "uart_tcp";

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
  this->publish_link_();
}

void UartTcp::publish_link_() {
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(this->connected_);
  }
}

void UartTcp::setup() {
  this->tcp_buf_.reserve(300);
  this->publish_link_();
}

void UartTcp::dump_config() {
  ESP_LOGCONFIG(TAG, "UART TCP bridge:");
  ESP_LOGCONFIG(TAG, "  Role: %s", this->server_ ? "server" : "client");
  ESP_LOGCONFIG(TAG, "  Protocol: %s", this->modbus_ ? "modbus" : "raw");
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
}

void UartTcp::close_sock_() {
  if (this->sock_ >= 0) {
    ::shutdown(this->sock_, SHUT_RDWR);
    ::close(this->sock_);
  }
  this->sock_ = -1;
  this->connecting_ = false;
  this->set_link_up_(false);
  this->tcp_buf_.clear();
  this->uart_buf_.clear();
  this->wait_uart_ = false;
  this->wait_tcp_ = false;
}

void UartTcp::close_listen_() {
  if (this->listen_ >= 0) {
    ::close(this->listen_);
  }
  this->listen_ = -1;
}

void UartTcp::apply_socket_options_(int fd) {
  int yes = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
#ifdef TCP_KEEPIDLE
  int idle = 30;
  int interval = 10;
  int count = 3;
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
}

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

void UartTcp::try_resolve_() {
  if (this->have_addr_.load() || this->resolving_.load()) {
    return;
  }
  ip4_addr_t literal;
  if (ip4addr_aton(this->host_.c_str(), &literal)) {
    this->resolved_addr_.store(ip4_addr_get_u32(&literal));
    this->have_addr_.store(true);
    return;
  }
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
  this->resolve_failed_.store(true);
}

void UartTcp::try_connect_() {
  if (this->sock_ >= 0 || millis() < this->next_connect_ms_) {
    return;
  }
  if (this->resolve_failed_.exchange(false)) {
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  this->try_resolve_();
  if (!this->have_addr_.load()) {
    return;
  }
  int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  this->apply_socket_options_(fd);
  struct sockaddr_in dest {};
  dest.sin_family = AF_INET;
  dest.sin_port = htons(this->port_);
  dest.sin_addr.s_addr = this->resolved_addr_.load();
  int rc = ::connect(fd, reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest));
  if (rc == 0 || errno == EINPROGRESS) {
    this->sock_ = fd;
    this->connecting_ = rc != 0;
    this->set_link_up_(rc == 0);
    return;
  }
  ::close(fd);
  this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
}

void UartTcp::try_listen_() {
  if (this->listen_ >= 0 || millis() < this->next_connect_ms_) {
    return;
  }
  int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  int yes = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  struct sockaddr_in local {};
  local.sin_family = AF_INET;
  local.sin_port = htons(this->port_);
  local.sin_addr.s_addr = INADDR_ANY;
  if (bind(fd, reinterpret_cast<struct sockaddr *>(&local), sizeof(local)) != 0 || listen(fd, 1) != 0) {
    ESP_LOGW(TAG, "Listen on %u failed", this->port_);
    ::close(fd);
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  this->listen_ = fd;
  ESP_LOGI(TAG, "Listening on %u", this->port_);
}

void UartTcp::accept_client_() {
  if (this->listen_ < 0 || this->sock_ >= 0) {
    return;
  }
  int fd = ::accept(this->listen_, nullptr, nullptr);
  if (fd < 0) {
    return;
  }
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  this->apply_socket_options_(fd);
  this->sock_ = fd;
  this->set_link_up_(true);
  this->tcp_buf_.clear();
  ESP_LOGI(TAG, "Client connected");
}

void UartTcp::read_socket_() {
  if (this->sock_ < 0) {
    return;
  }
  if (this->connecting_) {
    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(this->sock_, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
      this->close_sock_();
      this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
      return;
    }
    this->connecting_ = false;
    this->set_link_up_(true);
    ESP_LOGI(TAG, "Connected to %s:%u", this->host_.c_str(), this->port_);
  }
  uint8_t tmp[128];
  int count = ::recv(this->sock_, tmp, sizeof(tmp), 0);
  if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
    ESP_LOGW(TAG, "Connection lost");
    this->close_sock_();
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  if (count > 0) {
    this->tcp_buf_.insert(this->tcp_buf_.end(), tmp, tmp + count);
  }
}

void UartTcp::send_all_(const uint8_t *data, size_t len) {
  if (!this->connected_ || len == 0) {
    return;
  }
  size_t sent_total = 0;
  while (sent_total < len) {
    int sent = ::send(this->sock_, data + sent_total, len - sent_total, 0);
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }
    if (sent <= 0) {
      ESP_LOGW(TAG, "Send failed");
      this->close_sock_();
      this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
      return;
    }
    sent_total += static_cast<size_t>(sent);
  }
}

void UartTcp::pull_uart_() {
  uint8_t tmp[128];
  while (this->available() > 0 && this->uart_buf_.size() < 512) {
    size_t want = std::min(this->available(), sizeof(tmp));
    if (!this->read_array(tmp, want)) {
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
    this->write_array(this->tcp_buf_.data(), this->tcp_buf_.size());
    this->tcp_buf_.clear();
  }
  this->pull_uart_();
  if (!this->uart_buf_.empty()) {
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
  this->write_array(frame.data(), frame.size());
  this->flush();
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
  if (this->wait_uart_) {
    this->pull_uart_();
    std::vector<uint8_t> pdu;
    uint8_t unit = 0;
    if (this->take_rtu_(&pdu, &unit)) {
      this->send_mbap_(this->pending_txn_, unit, pdu.data(), pdu.size());
      this->wait_uart_ = false;
    } else if (millis() - this->wait_started_ms_ > this->response_timeout_ms_) {
      ESP_LOGW(TAG, "RTU response timeout");
      this->uart_buf_.clear();
      this->send_gateway_fail_(this->pending_txn_, this->pending_unit_, this->pending_function_);
      this->wait_uart_ = false;
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
    } else if (millis() - this->wait_started_ms_ > this->response_timeout_ms_) {
      ESP_LOGW(TAG, "TCP response timeout");
      this->tcp_buf_.clear();
      this->wait_tcp_ = false;
    }
    return;
  }
  if (this->server_) {
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
    this->wait_started_ms_ = millis();
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
  this->wait_started_ms_ = millis();
}

void UartTcp::loop() {
  if (this->server_) {
    this->try_listen_();
    this->accept_client_();
  } else {
    this->try_connect_();
  }
  this->read_socket_();
  if (!this->connected_) {
    return;
  }
  if (this->modbus_) {
    this->pump_modbus_();
  } else {
    this->pump_raw_();
  }
}

}  // namespace uart_tcp
}  // namespace esphome
