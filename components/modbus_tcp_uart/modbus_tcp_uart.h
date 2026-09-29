#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart_component.h"

#include "lwip/ip_addr.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace esphome {
namespace modbus_tcp_uart {

// One Modbus TCP socket presented as a UART. The stock modbus client and
// server hubs stay unchanged: RTU in, MBAP out, and the reverse for a server.
class ModbusTcpUart : public uart::UARTComponent, public Component {
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_reconnect_interval(uint32_t ms) { this->reconnect_interval_ms_ = ms; }
  void set_server(bool server) { this->server_ = server; }

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
  bool is_connected() override { return this->sock_ >= 0 && this->connected_; }

  void load_settings(bool dump_config) override {}

 protected:
  void check_logger_conflict() override {}

  void close_sock_();
  void close_listen_();
  void try_resolve_();
  void try_connect_();
  void try_listen_();
  void accept_client_();
  void read_socket_();
  void extract_frames_();
  void send_rtu_frame_();
  void push_rx_(uint8_t byte);
  void apply_socket_options_(int fd);
  static void dns_found_(const char *name, const ip_addr_t *addr, void *arg);

  std::string host_;
  uint16_t port_{502};
  bool server_{false};
  int sock_{-1};
  int listen_{-1};
  bool connecting_{false};
  bool connected_{false};
  uint32_t next_connect_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  uint16_t txn_{0};
  uint16_t last_request_txn_{0};
  bool response_pending_{false};

  std::atomic<bool> resolving_{false};
  std::atomic<bool> resolve_failed_{false};
  std::atomic<bool> have_addr_{false};
  std::atomic<uint32_t> resolved_addr_{0};

  std::vector<uint8_t> tx_;
  std::deque<uint8_t> rx_;
  std::vector<uint8_t> tcp_buf_;
};

}  // namespace modbus_tcp_uart
}  // namespace esphome
