# esphome-tcp-uart

Two ways to put a UART on TCP, for ESPHome 2026.8 or newer. License: MIT.

[Deutsche Fassung](README.de.md)

| Component | Direction |
|---|---|
| `tcp_uart` | A component with `uart_id` talks to a TCP socket. No pins. |
| `uart_tcp` | The pins are a real UART. The other end is a TCP socket. |

Bytes are unchanged unless `protocol` is `modbus`. How to [load an external component](https://esphome.io/components/external_components.html), how a [UART](https://esphome.io/components/uart.html) is set up, and how the [Modbus hub](https://esphome.io/components/modbus.html) uses `uart_id` is documented by ESPHome.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [tcp_uart]
```

## tcp_uart

Replaces a UART. The peer must already speak TCP. No pins, no line levels. One entry is a client or a server.

```yaml
tcp_uart:
  - id: remote_serial
    role: client
    host: 192.0.2.20
    port: 5000
```

Pass `remote_serial` as `uart_id`. `role: server` uses the same keys and forbids `host`. One TCP client at a time.

| Key | Default | Meaning |
|---|---|---|
| `role` | `client` | `client` dials `host`. `server` listens. |
| `host` | — | Required for a client, forbidden for a server. |
| `port` | — | Required. Remote port, or the local listen port. |
| `protocol` | `raw` | `raw` copies bytes. `modbus` wraps them for a Modbus TCP peer. |
| `baud_rate` | 9600 | Stored only, so a component can read it. Not sent, and it clocks no pin. |
| `reconnect_interval` | 5s | Pause after a failed dial, a dropped link, or a failed listen. |

## uart_tcp

Copies bytes between one hardware UART and one TCP socket. Pins and baud belong on the [`uart:`](https://esphome.io/components/uart.html) entry this component points at. `role: server` accepts one client. A client is the same entry plus `host`.

```yaml
uart_tcp:
  - uart_id: bus
    role: server
    port: 5000
```

| Key | Default | Meaning |
|---|---|---|
| `uart_id` | — | The hardware UART. |
| `port` | — | Required TCP port. |
| `role` | `server` | `server` listens. `client` dials `host`. |
| `protocol` | `raw` | `raw` copies bytes. `modbus` wraps them for a Modbus TCP peer. |
| `host` | — | Required for a client, forbidden for a server. |
| `reconnect_interval` | 5s | Pause after a failed dial, a dropped link, or a failed listen. |
| `response_timeout` | 300ms | Used only with `protocol: modbus`. |

## Baud rate

The socket has no baud rate. `tcp_uart` does not clock bits. `uart_tcp` uses the baud of its hardware UART for the pins.

## Tests

[script/ci](script/ci) checks every file in `tests/` with ESPHome 2026.9.0. A file in `tests/invalid/` must fail, and the log must contain the sentence in the matching `.expect` file. [tests/compile.yaml](tests/compile.yaml) is compiled for ESP32.

```bash
pip install "esphome==2026.9.0"
./script/ci
esphome compile tests/compile.yaml
```
