# esphome-tcp-uart

Two ways to put a UART on TCP, for ESPHome 2026.8 or newer. License: MIT.

[Deutsche Fassung](README.de.md)

| You want | Component |
|---|---|
| No pins. A component uses this id as `uart_id`. | `tcp_uart` |
| Pins stay a real UART. Bytes are copied to TCP. | `uart_tcp` |

Bytes are unchanged unless `protocol` is `modbus`. Loading an [external component](https://esphome.io/components/external_components.html), setting up a [UART](https://esphome.io/components/uart.html), and attaching the [Modbus hub](https://esphome.io/components/modbus.html) are documented by ESPHome.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [tcp_uart, uart_tcp]
```

Pick Client or Server. Do not mix the two in one entry.

## Client

This device opens the TCP connection. `host` is required.

### No pins

`role` defaults to `client` and can be left out. Pass the id to a component as `uart_id`. The socket is not a pin and has no line level. `baud_rate` is stored so the component can read it. It is not sent.

```yaml
tcp_uart:
  - id: remote_serial
    host: 192.0.2.20
    port: 5000
```

| Key | Default | Meaning |
|---|---|---|
| `host` | — | Required. |
| `port` | — | Required. Port on that host. |
| `protocol` | `raw` | `modbus` if the peer speaks Modbus TCP. |
| `baud_rate` | 9600 | Not on the wire. |
| `reconnect_interval` | 5s | Pause after a failed dial or a dropped link. |

### Pins

`role: client` is required here, because this component listens unless told otherwise. Pins and baud stay on the [`uart:`](https://esphome.io/components/uart.html) entry.

```yaml
uart_tcp:
  - uart_id: bus
    role: client
    host: 192.0.2.20
    port: 5000
```

| Key | Default | Meaning |
|---|---|---|
| `uart_id` | — | Required. The hardware UART. |
| `role` | — | Required. Set `client`. |
| `host` | — | Required. |
| `port` | — | Required. Port on that host. |
| `protocol` | `raw` | `modbus` if the peer speaks Modbus TCP. |
| `reconnect_interval` | 5s | Pause after a failed dial or a dropped link. |
| `response_timeout` | 300ms | Only for `protocol: modbus`. |

## Server

Another device opens the TCP connection. Do not set `host`. One client at a time.

### No pins

```yaml
tcp_uart:
  - id: local_serial
    role: server
    port: 5000
```

Pass `local_serial` as `uart_id`. `baud_rate` is stored and is not sent.

| Key | Default | Meaning |
|---|---|---|
| `role` | — | Required. Set `server`. |
| `port` | — | Required. Port on this device. |
| `protocol` | `raw` | `modbus` if the peer speaks Modbus TCP. |
| `baud_rate` | 9600 | Not on the wire. |
| `reconnect_interval` | 5s | Pause after a failed listen or a dropped link. |

### Pins

`role` defaults to `server` and can be left out. Pins and baud stay on the [`uart:`](https://esphome.io/components/uart.html) entry.

```yaml
uart_tcp:
  - uart_id: bus
    port: 5000
```

| Key | Default | Meaning |
|---|---|---|
| `uart_id` | — | Required. The hardware UART. |
| `port` | — | Required. Port on this device. |
| `protocol` | `raw` | `modbus` if the peer speaks Modbus TCP. |
| `reconnect_interval` | 5s | Pause after a failed listen or a dropped link. |
| `response_timeout` | 300ms | Only for `protocol: modbus`. |

## Tests

[script/ci](script/ci) checks every file in `tests/` with ESPHome 2026.9.0. A file in `tests/invalid/` must fail, and the log must contain the sentence in the matching `.expect` file. [tests/compile.yaml](tests/compile.yaml) is compiled for ESP32.

```bash
pip install "esphome==2026.9.0"
./script/ci
esphome compile tests/compile.yaml
```
