# esphome-modbus-tcp-uart

Two TCP bridges for ESPHome 2026.8 or newer. License: MIT.

[Deutsche Fassung](README.de.md)

| Component | What it does |
|---|---|
| `tcp_uart` | A UART with no pins. `protocol: raw` or `protocol: modbus`, client or server. |
| `uart_tcp` | Hardware UART pins to TCP. `raw` copies bytes. `modbus` converts RTU and MBAP. |

Neither component is a sensor platform. A sensor still uses `platform: modbus_controller` from ESPHome. `tcp_uart` only replaces the UART the `modbus:` hub reads.

## tcp_uart

Replaces a UART. It has no pins. One entry is either a client or a server. `protocol` selects the bytes, for both roles:

| `protocol` | Socket | What ESPHome reads and writes |
|---|---|---|
| `raw` | unchanged bytes | the same bytes |
| `modbus` | Modbus TCP (MBAP) | Modbus RTU |

```yaml
external_components:
  - source: github://Bascht74/esphome-modbus-tcp-uart
    components: [tcp_uart]

tcp_uart:
  - id: meter
    role: client
    host: 192.0.2.10
    port: 502
    protocol: modbus

modbus:
  - id: tcp_bus
    uart_id: meter
    role: client
    send_wait_time: 200ms

modbus_controller:
  - id: device_1
    modbus_id: tcp_bus
    address: 1
    update_interval: 1s
```

Several controllers may share one `modbus_id`. The hub keeps a single in-flight request. `send_wait_time` stays on `modbus`.

`role: server` listens. There is no second set of keys. The same ones apply, except `host` is forbidden:

| Key | Default | On a server |
|---|---|---|
| `port` | — | Required. The local listen port. |
| `protocol` | `raw` | `raw` copies bytes. `modbus` speaks MBAP and hands RTU to the hub. |
| `reconnect_interval` | 5s | Retry after a failed bind, and after the client drops. |
| `baud_rate` | 9600 | Not sent. With `protocol: modbus` the hub uses it only as a timer. |

One TCP client at a time. With `protocol: modbus` the stock `modbus:` hub is `role: server`, and register maps live on `modbus_server`, not here. The transaction id from the request is echoed. A hardware RTU bus stays on a real UART.

```yaml
tcp_uart:
  - id: modbus_link
    role: server
    port: 502
    protocol: modbus

modbus:
  - id: server_bus
    uart_id: modbus_link
    role: server

modbus_server:
  - modbus_id: server_bus
    address: 1
```

## uart_tcp

Copies bytes between a real UART and one TCP socket. Baud, data bits, parity, and stop bits belong on that `uart:` entry. They clock the pins. `port` is the TCP port. One TCP client at a time in `role: server`.

`protocol: raw` copies bytes unchanged. A Modbus RTU device then appears on TCP as RTU, without an MBAP header.

`protocol: modbus` speaks Modbus TCP on the socket and Modbus RTU on the pins. `role: server` accepts a TCP master and queries the bus. `role: client` dials a TCP slave and forwards RTU requests from a master on the pins.

```yaml
external_components:
  - source: github://Bascht74/esphome-modbus-tcp-uart
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
    port: 502
    protocol: modbus
    response_timeout: 300ms
```

| Key | Default | Meaning |
|---|---|---|
| `uart_id` | — | Hardware UART. Baud and framing are set there. |
| `port` | — | Required TCP port. |
| `role` | `server` | `server` listens. `client` dials `host`. |
| `protocol` | `raw` | `raw` copies bytes. `modbus` converts MBAP and RTU. |
| `host` | — | Required for a client, forbidden for a server. |
| `reconnect_interval` | 5s | Delay after a failed dial, a dead link, or a failed listen. |
| `response_timeout` | 300ms | Used only for `protocol: modbus`. |

## Where baud rate matters

`tcp_uart` stores 9600 8N1 and never paces bits. The stock hub reads that number in `Modbus::setup()`:

- inter-frame gap = 3.5 character times, about 4 ms at 9600
- estimated transmit time = frame length × bits per character / baud

Both are timers inside the hub. They do not change the TCP stream. A real bus sets its own `baud_rate` on its own `uart:` entry. `tcp_uart` stores `baud_rate` for the same kind of check and does not use it as a clock. `uart_tcp` uses the baud of its hardware UART both to clock the pins and, in `protocol: modbus`, as the 3.5-character gap between RTU frames.

## Tests

GitHub Actions installs ESPHome 2026.9.0 and runs [script/ci](script/ci). That validates every file in `tests/` with `esphome config`. Each file in `tests/invalid/` must be rejected, and the output must contain the sentence in the matching `.expect` file. A failure for some other reason does not count. A second job compiles [tests/compile.yaml](tests/compile.yaml) for ESP32 (ESP-IDF), which builds `tcp_uart` in both protocols and `uart_tcp`.

```bash
pip install "esphome==2026.9.0"
./script/ci
esphome compile tests/compile.yaml
```

## Compatibility

Both components implement only the UART byte methods. They do not call `modbus_controller` internals. The helper-shim removal in 2026.10 does not apply. They break if `UARTComponent` gains a new pure virtual. That set is the same in 2026.9.0 and current `dev`.

`skip_updates`, `force_new_range`, and `command_throttle` still parse on the stock controller in 2026.9 and no longer change polling. Removal is scheduled for 2027.2 and 2027.3.
