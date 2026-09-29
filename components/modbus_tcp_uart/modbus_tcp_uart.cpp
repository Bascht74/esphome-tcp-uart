#include "modbus_tcp_uart.h"

#include "esphome/core/log.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

namespace esphome {
namespace modbus_tcp_uart {

static const char *const TAG = "modbus_tcp_uart";

static uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

float ModbusTcpUart::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void ModbusTcpUart::setup() { this->tcp_buf_.reserve(300); }

void ModbusTcpUart::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus TCP as UART:");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
}

void ModbusTcpUart::close_sock_() {
  if (this->sock_ >= 0) {
    ::close(this->sock_);
  }
  this->sock_ = -1;
  this->connecting_ = false;
  this->connected_ = false;
  this->tcp_buf_.clear();
  this->tx_.clear();
}

void ModbusTcpUart::try_connect_() {
  if (this->sock_ >= 0 || millis() < this->next_connect_ms_) {
    return;
  }
  struct addrinfo hints {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  const std::string port = std::to_string(this->port_);
  if (getaddrinfo(this->host_.c_str(), port.c_str(), &hints, &res) != 0 || res == nullptr) {
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd < 0) {
    freeaddrinfo(res);
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  int rc = ::connect(fd, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);
  if (rc == 0 || errno == EINPROGRESS) {
    this->sock_ = fd;
    this->connecting_ = rc != 0;
    this->connected_ = rc == 0;
    return;
  }
  ::close(fd);
  this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
}

void ModbusTcpUart::push_rx_(uint8_t byte) {
  if (this->rx_.size() < 512) {
    this->rx_.push_back(byte);
  }
}

void ModbusTcpUart::read_socket_() {
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
  int n = ::recv(this->sock_, tmp, sizeof(tmp), 0);
  if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
    ESP_LOGW(TAG, "Connection lost");
    this->close_sock_();
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
    return;
  }
  if (n > 0) {
    this->tcp_buf_.insert(this->tcp_buf_.end(), tmp, tmp + n);
  }
  while (this->tcp_buf_.size() >= 7) {
    uint16_t txn = (this->tcp_buf_[0] << 8) | this->tcp_buf_[1];
    uint16_t proto = (this->tcp_buf_[2] << 8) | this->tcp_buf_[3];
    uint16_t length = (this->tcp_buf_[4] << 8) | this->tcp_buf_[5];
    if (proto != 0 || length < 2 || length > 254) {
      this->tcp_buf_.erase(this->tcp_buf_.begin());
      continue;
    }
    if (this->tcp_buf_.size() < 6u + length) {
      break;
    }
    uint8_t unit = this->tcp_buf_[6];
    const uint8_t *pdu = this->tcp_buf_.data() + 7;
    size_t pdu_len = length - 1;
    if (txn == this->txn_) {
      std::vector<uint8_t> rtu;
      rtu.reserve(pdu_len + 3);
      rtu.push_back(unit);
      rtu.insert(rtu.end(), pdu, pdu + pdu_len);
      uint16_t crc = crc16(rtu.data(), rtu.size());
      rtu.push_back(crc & 0xFF);
      rtu.push_back(crc >> 8);
      for (uint8_t b : rtu) {
        this->push_rx_(b);
      }
    } else {
      ESP_LOGW(TAG, "Dropped transaction %u, expected %u", txn, this->txn_);
    }
    this->tcp_buf_.erase(this->tcp_buf_.begin(), this->tcp_buf_.begin() + 6 + length);
  }
}

void ModbusTcpUart::send_rtu_frame_() {
  if (this->tx_.size() < 4 || !this->connected_) {
    this->tx_.clear();
    return;
  }
  // Hub frame is address, PDU, CRC. The CRC is not sent on TCP.
  const uint8_t *pdu = this->tx_.data() + 1;
  size_t pdu_len = this->tx_.size() - 3;
  uint8_t unit = this->tx_[0];
  this->txn_ = this->txn_ == 0xFFFF ? 1 : this->txn_ + 1;
  uint8_t hdr[7];
  hdr[0] = this->txn_ >> 8;
  hdr[1] = this->txn_ & 0xFF;
  hdr[2] = 0;
  hdr[3] = 0;
  uint16_t length = pdu_len + 1;
  hdr[4] = length >> 8;
  hdr[5] = length & 0xFF;
  hdr[6] = unit;
  if (::send(this->sock_, hdr, 7, 0) != 7 || ::send(this->sock_, pdu, pdu_len, 0) != (int) pdu_len) {
    ESP_LOGW(TAG, "Send failed");
    this->close_sock_();
    this->next_connect_ms_ = millis() + this->reconnect_interval_ms_;
  }
  this->tx_.clear();
}

void ModbusTcpUart::loop() {
  this->try_connect_();
  this->read_socket_();
}

void ModbusTcpUart::write_array(const uint8_t *data, size_t len) {
  this->tx_.insert(this->tx_.end(), data, data + len);
}

bool ModbusTcpUart::peek_byte(uint8_t *data) {
  if (this->rx_.empty()) {
    return false;
  }
  *data = this->rx_.front();
  return true;
}

bool ModbusTcpUart::read_array(uint8_t *data, size_t len) {
  if (this->rx_.size() < len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    data[i] = this->rx_.front();
    this->rx_.pop_front();
  }
  return true;
}

size_t ModbusTcpUart::available() { return this->rx_.size(); }

uart::UARTFlushResult ModbusTcpUart::flush() {
  this->send_rtu_frame_();
  return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
}

}  // namespace modbus_tcp_uart
}  // namespace esphome
