#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

#ifdef USE_ESP32
#include "lwip/ip_addr.h"
#endif

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace esphome {
namespace tcp_uart {

// TCP socket presented as a UART.
// protocol raw copies bytes. protocol modbus is MBAP on the socket and RTU
// toward ESPHome.
class TcpUart : public uart::UARTComponent, public Component {
public:
  void set_host(const char *host) { this->host_ = StringRef(host); }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_reconnect_interval(uint32_t ms) {
    this->reconnect_interval_ms_ = ms;
  }
  void set_stall_timeout(uint32_t ms) { this->stall_timeout_ms_ = ms; }
  void set_idle_timeout(uint32_t ms) { this->idle_timeout_ms_ = ms; }
  void set_server(bool server) { this->server_ = server; }
  void set_modbus(bool modbus) { this->modbus_ = modbus; }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) {
    this->connected_sensor_ = sensor;
  }
  void set_drop_sensor(sensor::Sensor *sensor) { this->drop_sensor_ = sensor; }
  void set_address_sensor(text_sensor::TextSensor *sensor) {
    this->address_sensor_ = sensor;
  }
  void add_allowed(const char *host);

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
  bool is_connected() override {
    return this->sock_ != nullptr && this->connected_;
  }
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
  void extract_frames_();
  void send_rtu_frame_();
  void push_rx_(uint8_t byte);
  void send_bytes_(const uint8_t *data, size_t len);
  void apply_socket_options_(socket::Socket *sock);
  void set_link_up_(bool up);
  void publish_link_();
  void publish_address_(const char *ip);
  void clear_address_();
  void publish_peer_(const struct sockaddr *addr);
  void note_drop_();
  void note_io_();
  void check_idle_();
  bool peer_allowed_(const struct sockaddr *addr);
#ifdef USE_ESP32
  static void dns_found_(const char *name, const ip_addr_t *addr, void *arg);
#endif

  StringRef host_;
  uint16_t port_{0};
  bool server_{false};
  bool modbus_{false};
  std::unique_ptr<socket::Socket> sock_;
  std::unique_ptr<socket::ListenSocket> listen_;
  bool connecting_{false};
  bool connected_{false};
  uint32_t next_connect_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  uint32_t stall_timeout_ms_{0};
  uint32_t idle_timeout_ms_{0};
  uint32_t last_io_ms_{0};
  uint32_t drops_{0};
  uint16_t txn_{0};
  uint16_t last_request_txn_{0};
  bool response_pending_{false};
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  sensor::Sensor *drop_sensor_{nullptr};
  text_sensor::TextSensor *address_sensor_{nullptr};
  char address_[32]{};

  std::atomic<bool> resolving_{false};
  std::atomic<bool> resolve_failed_{false};
  std::atomic<bool> have_addr_{false};
  std::atomic<uint32_t> resolved_addr_{0};
  char resolved_ip_[16]{};

  StaticRingBuffer<uint8_t, 1024> rx_;
  uint8_t tx_[512]{};
  size_t tx_len_{0};
  uint8_t tcp_buf_[512]{};
  size_t tcp_len_{0};
  std::vector<uint32_t> allowed_;
};

} // namespace tcp_uart
} // namespace esphome
