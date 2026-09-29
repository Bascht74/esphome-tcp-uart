# esphome-modbus-tcp-uart

A Modbus TCP socket that looks like a UART to the stock ESPHome `modbus_controller`. ESPHome 2026.8 or newer. License: MIT.

[Deutsche Fassung](README.de.md)

`creepystefan/esphome_modbus_tcp` copies the old controller. This component leaves the ESPHome controller as it is. It only wraps RTU frames in MBAP and unwraps the replies.

## Use

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

One component and one `modbus` hub per device. A real RTU bus stays on a hardware UART.

## Keys

| Key | Default | Meaning |
|---|---|---|
| `host` | — | IPv4 address or hostname |
| `port` | 502 | TCP port |
| `reconnect_interval` | 5s | Delay after an error or a dropped connection |

Wait time, poll interval, and register ranges belong to the native hub and controller: `send_wait_time` on `modbus`, `update_interval` on `modbus_controller`. `skip_updates` and `force_new_range` were removed there in 2026.9.
