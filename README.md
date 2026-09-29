# esphome-tcp-uart

ESPHome only offers a UART on pins. These components connect that UART to a TCP socket, in either direction. A component that already takes `uart_id` keeps working when the other end is on the network. A device wired to the pins can be reached from the network.

For ESPHome 2026.8 or newer.

[Deutsche Fassung](README.de.md)

| You want | Component |
|---|---|
| No pins. A component uses this id as `uart_id`. | [`tcp_uart`](#tcp_uart) |
| Pins stay a real UART. Bytes are copied to TCP. | [`uart_tcp`](#uart_tcp) |

Bytes are unchanged unless `protocol` is `modbus`. Loading an [external component](https://esphome.io/components/external_components.html), setting up a [UART](https://esphome.io/components/uart.html), and attaching the [Modbus hub](https://esphome.io/components/modbus.html) are documented by ESPHome.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [tcp_uart, uart_tcp]
```

## Contents

- [`tcp_uart`](#tcp_uart)
  - [Client](#tcp_uart-client)
  - [Server](#tcp_uart-server)
- [`uart_tcp`](#uart_tcp)
  - [Client](#uart_tcp-client)
  - [Server](#uart_tcp-server)
- [Both roles](#both-roles)

## tcp_uart

Replaces a UART. No pins. Pass the id to another component as `uart_id`. One entry is one socket: it dials or it listens.

### `tcp_uart` client

This device opens the connection. `host` is required. `role` defaults to `client` and can be left out.

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
| `reconnect_interval` | 5s | Pause after a failed dial or a dropped link. |

### `tcp_uart` server

Another device opens the connection. Do not set `host`. `role: server` is required. One client at a time.

```yaml
tcp_uart:
  - id: local_serial
    role: server
    port: 5000
```

| Key | Default | Meaning |
|---|---|---|
| `role` | — | Required. Set `server`. |
| `port` | — | Required. Port on this device. |
| `protocol` | `raw` | `modbus` if the peer speaks Modbus TCP. |
| `reconnect_interval` | 5s | Pause after a failed listen or a dropped link. |

## uart_tcp

Copies bytes between one hardware UART and one TCP socket. The pins are a [UART](https://esphome.io/components/uart.html). One entry is one socket: it dials or it listens.

### `uart_tcp` client

This device opens the connection. `role: client` is required, because this component listens unless told otherwise. `host` is required.

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

### `uart_tcp` server

Another device opens the connection. Do not set `host`. `role` defaults to `server` and can be left out. One client at a time.

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

## Both roles

Use two entries to dial and to listen. Each one has its own id and its own port.

```yaml
tcp_uart:
  - id: remote_serial
    host: 192.0.2.20
    port: 5000
  - id: local_serial
    role: server
    port: 5001
```

`uart_tcp` is the same: two entries under `uart_tcp:`, each with its own `uart_id`.
