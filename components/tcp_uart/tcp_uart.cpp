#include "tcp_uart.h"

#include "esphome/core/log.h"

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
namespace tcp_uart {

static const char *const TAG = "tcp_uart";

float TcpUart::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void TcpUart::setup() {}

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
  if (this->sock_ >= 0) {
    ::shutdown(this->sock_, SHUT_RDWR);
    ::close(this->sock_);
  }
  this->sock_ = -1;
  this->connecting_ = false;
  this->connected_ = false;
  this->rx_.clear();
  this->tx_.clear();
  this->tcp_buf_.clear();
  this->response_pending_ = false;
}

void TcpUart::close_listen_() {
  if (this->listen_ >= 0) {
    ::close(this->listen_);
  }
  this->listen_ = -1;
}

void TcpUart::apply_socket_options_(int fd) {
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

void TcpUart::try_resolve_() {
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
  this->resolve_failed_.store(true);
}

void TcpUart::try_connect_() {
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
    this->connected_ = rc == 0;
    return;
  }
  ::close(fd);
  this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
}

void TcpUart::try_listen_() {
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

void TcpUart::accept_client_() {
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
  this->connected_ = true;
  ESP_LOGI(TAG, "Client connected");
}

void TcpUart::read_socket_() {
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
    this->connected_ = true;
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
  if (count > 0 && this->modbus_) {
    this->tcp_buf_.insert(this->tcp_buf_.end(), tmp, tmp + count);
    this->extract_frames_();
    return;
  }
  for (int i = 0; i < count && this->rx_.size() < 1024; i++) {
    this->rx_.push_back(tmp[i]);
  }
}

void TcpUart::send_bytes_(const uint8_t *data, size_t len) {
  if (!this->connected_ || len == 0) {
    return;
  }
  size_t sent_total = 0;
  while (sent_total < len) {
    int sent = ::send(this->sock_, data + sent_total, len - sent_total, 0);
    if (sent > 0) {
      sent_total += static_cast<size_t>(sent);
      continue;
    }
    ESP_LOGW(TAG, "Send failed");
    this->close_sock_();
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
}

void TcpUart::loop() {
  if (this->server_) {
    this->try_listen_();
    this->accept_client_();
  } else {
    this->try_connect_();
  }
  this->read_socket_();
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
