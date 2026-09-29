# esphome-tcp-uart

Two ways to put a UART on TCP, for ESPHome 2026.8 or newer. License: MIT.

[Deutsche Fassung](README.de.md)

| Component | Direction |
|---|---|
| `tcp_uart` | A component with `uart_id` talks to a TCP socket. No pins. |
| `uart_tcp` | The pins are a real UART. The other end is a TCP socket. |

The bytes are the same on both ends. `protocol: modbus` is optional and is described last. These components are not sensors. Anything that already takes `uart_id` can use them.

## tcp_uart

Replaces a UART. The other side must already speak raw TCP. There are no GPIO pins and no RS-232 levels. One entry is a client or a server, not both.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [tcp_uart]

tcp_uart:
  - id: remote_serial
    role: client
    host: 192.0.2.20
    port: 5000
```

`role: server` uses the same keys. `host` is then forbidden. One TCP client at a time.

| Key | Default | Meaning |
|---|---|---|
| `role` | `client` | `client` dials `host`. `server` listens. |
| `host` | — | Required for a client, forbidden for a server. |
| `port` | — | Required. Remote port, or the local listen port. |
| `protocol` | `raw` | `raw` copies bytes. `modbus` is the section below. |
| `baud_rate` | 9600 | Stored only. It is not sent and it does not clock a pin. |
| `reconnect_interval` | 5s | Pause after a failed dial, a dropped link, or a failed listen. |

## uart_tcp

Copies bytes between a hardware UART and one TCP socket. Baud, data bits, parity, and stop bits belong on that `uart:` entry, because those pins are clocked. `port` is the TCP port. `role: server` accepts one client.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [uart_tcp]

uart:
  - id: bus
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 9600
    data_bits: 8
    parity: NONE
    stop_bits: 1

uart_tcp:
  - uart_id: bus
    role: server
    port: 5000
```

| Key | Default | Meaning |
|---|---|---|
| `uart_id` | — | Hardware UART. Baud and framing are set there. |
| `port` | — | Required TCP port. |
| `role` | `server` | `server` listens. `client` dials `host`. |
| `protocol` | `raw` | `raw` copies bytes. `modbus` is the section below. |
| `host` | — | Required for a client, forbidden for a server. |
| `reconnect_interval` | 5s | Pause after a failed dial, a dropped link, or a failed listen. |
| `response_timeout` | 300ms | Used only with `protocol: modbus`. |

A client is the same entry with `role: client` and a `host`.

## Baud rate

`tcp_uart` never paces bits. Its `baud_rate` is only there so a component can read it back. `uart_tcp` uses the baud of its hardware UART to clock the pins. The socket itself has no baud rate.

## protocol: modbus

Set this only when the TCP peer speaks Modbus TCP. The socket then carries an MBAP header. The UART side stays Modbus RTU, which is what the stock `modbus:` hub already speaks. `send_wait_time` stays on that hub. Register maps stay on `modbus_controller` or `modbus_server`.

```yaml
tcp_uart:
  - id: meter
    role: client
    host: 192.0.2.10
    port: 502
    protocol: modbus

modbus:
  - id: bus
    uart_id: meter
    role: client

modbus_controller:
  - id: device_1
    modbus_id: bus
    address: 1
    update_interval: 1s
```

A listening Modbus TCP socket is `role: server` on `tcp_uart` and `role: server` on `modbus:`. On `uart_tcp`, `protocol: modbus` does the same conversion on the pins: a TCP master is turned into RTU on the wire, or the reverse for `role: client`. If the peer does not answer, `uart_tcp` sends Modbus exception `0x0B` after `response_timeout`.

With `protocol: modbus`, the baud value is also a timer inside the stock hub: the gap between frames is about 3.5 character times. It still does not change the TCP stream. On `uart_tcp` the same baud clocks the pins.

## Tests

GitHub Actions installs ESPHome 2026.9.0 and runs [script/ci](script/ci). That validates every file in `tests/` with `esphome config`. Each file in `tests/invalid/` must be rejected, and the output must contain the sentence in the matching `.expect` file. A failure for some other reason does not count. A second job compiles [tests/compile.yaml](tests/compile.yaml) for ESP32.

```bash
pip install "esphome==2026.9.0"
./script/ci
esphome compile tests/compile.yaml
```

## Compatibility

Both components implement only the UART byte methods. They break if `UARTComponent` gains a new pure virtual. That set is the same in 2026.9.0 and current `dev`.
