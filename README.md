# esphome-modbus-tcp-uart

A Modbus TCP socket that looks like a UART to the stock ESPHome `modbus` hub. ESPHome 2026.8 or newer. License: MIT.

[Deutsche Fassung](README.de.md)

The component does not copy `modbus_controller`. It wraps RTU frames in MBAP and unwraps the replies. Client and server both use that path.

## Client

```yaml
external_components:
  - source: github://Bascht74/esphome-modbus-tcp-uart
    components: [modbus_tcp_uart]

modbus_tcp_uart:
  - id: tcp_link
    host: 192.0.2.10
    port: 502
    reconnect_interval: 5s

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

Several `modbus_controller` entries may share one `modbus_id`. The hub sends one request at a time and returns the reply to the device that queued it.

`send_wait_time` stays on `modbus`. That is the stock hub timer. Sensors use `platform: modbus_controller`.

## Server

One TCP client at a time. The stock `modbus_server` speaks RTU on this UART.

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

## Keys

| Key | Default | Meaning |
|---|---|---|
| `role` | `client` | `client` dials `host`. `server` listens. |
| `host` | — | IPv4 or hostname. Required for a client, forbidden for a server. |
| `port` | 502 | Remote port, or the local listen port. |
| `reconnect_interval` | 5s | Delay after a failed dial, a dead link, or a failed listen. |

A numeric host never blocks. A hostname is resolved through the lwIP DNS callback. The socket is closed in `on_shutdown`. TCP keepalive probes after 30 s idle.

## What this does not copy

`skip_updates`, `force_new_range`, and `command_throttle` still parse on the stock controller in 2026.9. They no longer change polling: `skip_updates` is ignored, `force_new_range` is rewritten to `reuse_previous_range`, and `command_throttle` points at `turnaround_time` on `modbus`. Removal is scheduled for 2027.2 and 2027.3, not 2026.9.

## Compatibility

The C++ side implements the UART byte methods only (`write_array`, `peek_byte`, `read_array`, `available`, `flush`, `load_settings`, `check_logger_conflict`). It does not call `modbus_controller` internals, so the helper-shim removal in 2026.10 does not apply. It breaks if `UARTComponent` gains a new pure virtual. That set is the same in 2026.9.0 and current `dev`.
