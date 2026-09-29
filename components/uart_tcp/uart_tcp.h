#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32
#include "lwip/ip_addr.h"
#endif

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace uart_tcp {

// Copies bytes between a hardware UART and one controlling TCP socket.
// protocol raw: unchanged bytes. protocol modbus: MBAP on TCP, RTU on the pins.
// With protocol modbus and role server, this id is also a UART. A local
// modbus_controller writes here. One frame is on the pins at a time, local first.
class UartTcp : public Component, public uart::UARTDevice, public uart::UARTComponent {
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_tap_port(uint16_t port) { this->tap_port_ = port; }
  void set_reconnect_interval(uint32_t ms) { this->reconnect_interval_ms_ = ms; }
  void set_response_timeout(uint32_t ms) { this->response_timeout_ms_ = ms; }
  void set_stall_timeout(uint32_t ms) { this->stall_timeout_ms_ = ms; }
  void set_idle_timeout(uint32_t ms) { this->idle_timeout_ms_ = ms; }
  void set_server(bool server) { this->server_ = server; }
  void set_modbus(bool modbus) { this->modbus_ = modbus; }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }
  void set_drop_sensor(sensor::Sensor *sensor) { this->drop_sensor_ = sensor; }
  void add_allowed(const std::string &host);

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override;

  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override;
  uart::UARTFlushResult flush() override;
  bool is_connected() override { return true; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

 protected:
  void check_logger_conflict() override {}
  void close_sock_();
  void close_listen_();
  void try_resolve_();
  bool ip_ready_();
  void try_connect_();
  void try_listen_();
  void accept_client_();
  void read_socket_();
  void apply_socket_options_(socket::Socket *sock);
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
  void note_drop_();
  void note_io_();
  void check_idle_();
  bool peer_allowed_(const struct sockaddr *addr);
  void send_tap_(const uint8_t *data, size_t len);
  void try_listen_tap_();
  void accept_tap_();
  void drain_taps_();
  void close_taps_();
  bool local_frame_ready_();
  void start_local_();
  void push_local_(const uint8_t *data, size_t len);
  uint32_t frame_gap_us_() const;
#ifdef USE_ESP32
  static void dns_found_(const char *name, const ip_addr_t *addr, void *arg);
#endif

  std::string host_;
  uint16_t port_{0};
  uint16_t tap_port_{0};
  bool server_{false};
  bool modbus_{false};
  std::unique_ptr<socket::Socket> sock_;
  std::unique_ptr<socket::ListenSocket> listen_;
  std::unique_ptr<socket::ListenSocket> listen_tap_;
  bool connecting_{false};
  bool connected_{false};
  uint32_t next_connect_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  uint32_t response_timeout_ms_{300};
  uint32_t stall_timeout_ms_{0};
  uint32_t idle_timeout_ms_{0};
  uint32_t last_io_ms_{0};
  uint32_t drops_{0};
  uint16_t txn_{0};
  uint16_t pending_txn_{0};
  uint8_t pending_unit_{0};
  uint8_t pending_function_{0};
  bool wait_uart_{false};
  bool wait_local_{false};
  bool wait_tcp_{false};
  uint32_t wait_started_ms_{0};
  uint32_t last_uart_us_{0};
  uint32_t last_local_us_{0};
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  sensor::Sensor *drop_sensor_{nullptr};

  std::atomic<bool> resolving_{false};
  std::atomic<bool> resolve_failed_{false};
  std::atomic<bool> have_addr_{false};
  std::atomic<uint32_t> resolved_addr_{0};
  std::string resolved_ip_;

  std::vector<uint8_t> tcp_buf_;
  std::vector<uint8_t> uart_buf_;
  std::vector<uint8_t> local_tx_;
  std::deque<uint8_t> local_rx_;
  std::vector<std::unique_ptr<socket::Socket>> taps_;
  std::vector<uint32_t> allowed_;
};

}  // namespace uart_tcp
}  // namespace esphome
