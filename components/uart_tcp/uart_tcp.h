#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include "lwip/ip_addr.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace uart_tcp {

// Copies bytes between a hardware UART and a TCP socket.
// protocol raw: unchanged bytes. protocol modbus: MBAP on TCP, RTU on the pins.
class UartTcp : public Component, public uart::UARTDevice {
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_reconnect_interval(uint32_t ms) { this->reconnect_interval_ms_ = ms; }
  void set_response_timeout(uint32_t ms) { this->response_timeout_ms_ = ms; }
  void set_server(bool server) { this->server_ = server; }
  void set_modbus(bool modbus) { this->modbus_ = modbus; }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override;

 protected:
  void close_sock_();
  void close_listen_();
  void try_resolve_();
  void try_connect_();
  void try_listen_();
  void accept_client_();
  void read_socket_();
  void apply_socket_options_(int fd);
  void send_all_(const uint8_t *data, size_t len);
  void pump_raw_();
  void pump_modbus_();
  void write_rtu_(const uint8_t *pdu, size_t pdu_len, uint8_t unit);
  void send_mbap_(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len);
  void send_gateway_fail_(uint16_t txn, uint8_t unit, uint8_t function);
  bool take_mbap_(std::vector<uint8_t> *pdu, uint8_t *unit, uint16_t *txn);
  bool take_rtu_(std::vector<uint8_t> *pdu, uint8_t *unit);
  void pull_uart_();
  void set_link_up_(bool up);
  void publish_link_();
  uint32_t frame_gap_us_() const;
  static void dns_found_(const char *name, const ip_addr_t *addr, void *arg);

  std::string host_;
  uint16_t port_{0};
  bool server_{false};
  bool modbus_{false};
  int sock_{-1};
  int listen_{-1};
  bool connecting_{false};
  bool connected_{false};
  uint32_t next_connect_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  uint32_t response_timeout_ms_{300};
  uint16_t txn_{0};
  uint16_t pending_txn_{0};
  uint8_t pending_unit_{0};
  uint8_t pending_function_{0};
  bool wait_uart_{false};
  bool wait_tcp_{false};
  uint32_t wait_started_ms_{0};
  uint32_t last_uart_us_{0};
  binary_sensor::BinarySensor *connected_sensor_{nullptr};

  std::atomic<bool> resolving_{false};
  std::atomic<bool> resolve_failed_{false};
  std::atomic<bool> have_addr_{false};
  std::atomic<uint32_t> resolved_addr_{0};

  std::vector<uint8_t> tcp_buf_;
  std::vector<uint8_t> uart_buf_;
};

}  // namespace uart_tcp
}  // namespace esphome
