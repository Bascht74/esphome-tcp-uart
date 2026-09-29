# esphome-modbus-tcp-uart

Ein Modbus-TCP-Socket, der sich für den normalen ESPHome-`modbus_controller` wie eine UART verhält. ESPHome 2026.8 oder neuer. Lizenz: MIT.

[English](README.md)

`creepystefan/esphome_modbus_tcp` kopiert den alten Controller. Hier bleibt der Controller aus ESPHome. Die Komponente packt nur RTU-Frames in MBAP und zurück.

## Einbinden

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

Pro Gerät eine Komponente und ein `modbus`-Hub. Ein echter RTU-Bus bleibt an der Hardware-UART.

## Schlüssel

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `host` | — | IPv4 oder Hostname |
| `port` | 502 | TCP-Port |
| `reconnect_interval` | 5s | Pause nach Fehler oder Abbruch |

Wartezeit, Poll-Intervall und Registerbereiche liegen am nativen Hub und Controller: `send_wait_time` unter `modbus`, `update_interval` unter `modbus_controller`. `skip_updates` und `force_new_range` gibt es dort seit 2026.9 nicht mehr.
