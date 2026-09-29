# esphome-modbus-tcp-uart

Ein Modbus-TCP-Socket, der sich für den normalen ESPHome-`modbus`-Hub wie eine UART verhält. ESPHome 2026.8 oder neuer. Lizenz: MIT.

[English](README.md)

Die Komponente kopiert `modbus_controller` nicht. Sie packt RTU-Frames in MBAP und zurück. Client und Server gehen denselben Weg.

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

Mehrere `modbus_controller` dürfen sich eine `modbus_id` teilen. Der Hub sendet eine Anfrage nach der anderen und gibt die Antwort nur an das Gerät zurück, das sie gestellt hat.

`send_wait_time` bleibt unter `modbus`. Das ist der Timer des Original-Hubs. Sensoren nutzen `platform: modbus_controller`.

## Server

Gleichzeitig nur ein TCP-Client. Der normale `modbus_server` spricht auf dieser UART RTU.

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

## Schlüssel

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `role` | `client` | `client` wählt `host`. `server` lauscht. |
| `host` | — | IPv4 oder Hostname. Pflicht für Client, verboten für Server. |
| `port` | 502 | Zielport oder lokaler Lauschport. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl, Abbruch oder fehlgeschlagenem Lauschen. |

Eine numerische Adresse blockiert nicht. Ein Hostname läuft über den lwIP-DNS-Callback. Der Socket wird in `on_shutdown` geschlossen. TCP-Keepalive prüft nach 30 s Ruhe.

## Was nicht mitkopiert wird

`skip_updates`, `force_new_range` und `command_throttle` akzeptiert der Original-Controller in 2026.9 noch. Sie ändern das Pollen nicht mehr: `skip_updates` wird ignoriert, `force_new_range` wird nach `reuse_previous_range` umgeschrieben, `command_throttle` verweist auf `turnaround_time` unter `modbus`. Entfernt werden sie 2027.2 und 2027.3, nicht 2026.9.

## Kompatibilität

Der C++-Teil implementiert nur die UART-Byte-Methoden (`write_array`, `peek_byte`, `read_array`, `available`, `flush`, `load_settings`, `check_logger_conflict`). Er ruft keine Interna von `modbus_controller` auf, deshalb trifft ihn das Entfernen der Helper-Shims in 2026.10 nicht. Er bricht, wenn `UARTComponent` eine neue pure virtual Methode bekommt. Dieser Satz ist in 2026.9.0 und im aktuellen `dev` gleich.
