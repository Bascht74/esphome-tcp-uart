#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart_component.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace esphome {
namespace modbus_tcp_uart {

// Presents one Modbus-TCP socket as a UART so the stock modbus client hub
// and modbus_controller stay unchanged. RTU frames in, MBAP frames out.
class ModbusTcpUart : public uart::UARTComponent, public Component {
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_reconnect_interval(uint32_t ms) { this->reconnect_interval_ms_ = ms; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override;
  uart::UARTFlushResult flush() override;
  bool is_connected() override { return this->sock_ >= 0 && this->connected_; }
  void load_settings(bool dump_config) override {}

 protected:
  void close_sock_();
  void try_connect_();
  void read_socket_();
  void send_rtu_frame_();
  void push_rx_(uint8_t byte);

  std::string host_;
  uint16_t port_{502};
  int sock_{-1};
  bool connecting_{false};
  bool connected_{false};
  uint32_t next_connect_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  uint16_t txn_{1};

  std::vector<uint8_t> tx_;
  std::deque<uint8_t> rx_;
  std::vector<uint8_t> tcp_buf_;
};

}  // namespace modbus_tcp_uart
}  // namespace esphome
