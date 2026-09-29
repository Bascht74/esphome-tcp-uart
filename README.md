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
  - [Sharing the pins](#sharing-the-pins)
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
| `stall_timeout` | 0s | Client only. Close and dial again after this long with no bytes. `0s` leaves the socket up. |
| `connected` | — | Optional. On while this TCP connection is up. |
| `disconnects` | — | Optional. How often the TCP connection dropped since boot. |

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
| `idle_timeout` | 0s | Server only. Close a peer that has been silent this long. `0s` leaves it up. |
| `allowed_hosts` | — | Server only. IP addresses that may connect. Empty allows any. |
| `connected` | — | Optional. On while this TCP connection is up. |
| `disconnects` | — | Optional. How often the TCP connection dropped since boot. |

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
| `stall_timeout` | 0s | Client only. Close and dial again after this long with no bytes. `0s` leaves the socket up. |
| `connected` | — | Optional. On while this TCP connection is up. |
| `disconnects` | — | Optional. How often the TCP connection dropped since boot. |
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
| `idle_timeout` | 0s | Server only. Close a peer that has been silent this long. `0s` leaves it up. |
| `allowed_hosts` | — | Server only. IP addresses that may connect. Empty allows any. |
| `connected` | — | Optional. On while this TCP connection is up. |
| `disconnects` | — | Optional. How often the TCP connection dropped since boot. |
| `response_timeout` | 300ms | Only for `protocol: modbus`. |
| `tap_port` | — | Second port. Connections there hear both directions and cannot send. |

`rx_buffer_size` on the hardware `uart:` is the pin buffer. The [UART](https://esphome.io/components/uart.html) page documents it. These components do not add another knob. Modbus frames are short, so the default is enough.

### Sharing the pins

Only `uart_tcp` with `protocol: modbus` and a listening role. Use its id as `uart_id` of the local Modbus hub. The component owns the pins. The local controller and one TCP client each hand it a frame. It sends one at a time, the local frame first, and returns the answer only to the sender.

```yaml
uart_tcp:
  - id: gate
    uart_id: bus
    port: 502
    protocol: modbus
    tap_port: 1503

modbus:
  - uart_id: gate
    id: local_bus
```

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
