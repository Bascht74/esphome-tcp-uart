# esphome-modbus-tcp-uart

Three TCP bridges for ESPHome 2026.8 or newer. License: MIT.

[Deutsche Fassung](README.de.md)

| Component | What it does |
|---|---|
| `modbus_tcp_uart` | Modbus TCP (MBAP) on the socket, Modbus RTU toward ESPHome. No pins. |
| `tcp_uart` | Raw bytes. Replaces a UART for a component with `uart_id`. No pins. |
| `uart_tcp` | Hardware UART pins to TCP. `raw` copies bytes. `modbus` converts RTU and MBAP. |

Neither component is a sensor platform. A sensor still uses `platform: modbus_controller` from ESPHome. These components only replace the UART the `modbus:` hub reads.

## modbus_tcp_uart

One entry is either a client or a server. Use two entries to do both.

```yaml
external_components:
  - source: github://Bascht74/esphome-modbus-tcp-uart
    components: [modbus_tcp_uart]

modbus_tcp_uart:
  - id: tcp_link
    host: 192.0.2.10
    port: 502

modbus:
  - id: tcp_bus
    uart_id: tcp_link
    role: client
    send_wait_time: 200ms

modbus_controller:
  - id: device_1
    modbus_id: tcp_bus
    address: 1
    update_interval: 1s
```

Several controllers may share one `modbus_id`. The hub keeps a single in-flight request and returns the reply to the device that queued it. This component has no device list of its own.

Server, one TCP client at a time. The stock `modbus_server` speaks RTU behind it. A hardware RTU bus stays on a real UART.

```yaml
modbus_tcp_uart:
  - id: tcp_server
    role: server
    port: 502

modbus:
  - id: server_bus
    uart_id: tcp_server
    role: server

modbus_server:
  - modbus_id: server_bus
    address: 1
```

| Key | Default | Meaning |
|---|---|---|
| `role` | `client` | `client` dials `host`. `server` listens. |
| `host` | — | IPv4 or hostname. Required for a client, forbidden for a server. |
| `port` | 502 | Remote port, or the local listen port. |
| `reconnect_interval` | 5s | Delay after a failed dial, a dead link, or a failed listen. |

`send_wait_time` stays on `modbus`.

## tcp_uart

Replaces a UART for any component that takes `uart_id` and only reads and writes bytes. The far end must already speak raw TCP. This is not Modbus TCP, not an RS-232 level, and not a pin bridge: bytes from the ESP's own UART pins are not forwarded.

Baud, parity, and stop bits of the real serial port are set on the far side. `baud_rate` here is stored so a component can check it. It is not sent and it does not clock bits.

`port` is required.

```yaml
external_components:
  - source: github://Bascht74/esphome-modbus-tcp-uart
    components: [tcp_uart]

tcp_uart:
  - id: remote_serial
    role: client
    host: 192.0.2.20
    port: 5000
    baud_rate: 9600
```

A gateway that takes a hardware `uart:` and publishes it on TCP is `uart_tcp` below. `tcp_uart` does not do that, so its `baud_rate` never clocks a pin.

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

`modbus_tcp_uart` stores 9600 8N1 and never paces bits. The stock hub reads that number in `Modbus::setup()`:

- inter-frame gap = 3.5 character times, about 4 ms at 9600
- estimated transmit time = frame length × bits per character / baud

Both are timers inside the hub. They do not change the TCP stream. A real bus sets its own `baud_rate` on its own `uart:` entry. `tcp_uart` stores `baud_rate` for the same kind of check and does not use it as a clock. `uart_tcp` uses the baud of its hardware UART both to clock the pins and, in `protocol: modbus`, as the 3.5-character gap between RTU frames.

## Compatibility

Both components implement only the UART byte methods. They do not call `modbus_controller` internals. The helper-shim removal in 2026.10 does not apply. They break if `UARTComponent` gains a new pure virtual. That set is the same in 2026.9.0 and current `dev`.

`skip_updates`, `force_new_range`, and `command_throttle` still parse on the stock controller in 2026.9 and no longer change polling. Removal is scheduled for 2027.2 and 2027.3.
