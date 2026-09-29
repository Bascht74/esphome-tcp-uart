# esphome-tcp-uart

ESPHome bietet eine UART nur an Pins. Diese Komponenten verbinden diese UART mit einem TCP-Socket, in beide Richtungen. Eine Komponente, die schon `uart_id` nimmt, funktioniert weiter, wenn die Gegenseite im Netz liegt. Ein Gerät an den Pins ist vom Netz aus erreichbar.

Für ESPHome 2026.8 oder neuer.

[English](README.md)

| Du willst | Komponente |
|---|---|
| Keine Pins. Eine Komponente nutzt diese Id als `uart_id`. | [`tcp_uart`](#tcp_uart) |
| Die Pins bleiben eine echte UART. Die Bytes gehen auf TCP. | [`uart_tcp`](#uart_tcp) |

Die Bytes bleiben unverändert, außer `protocol` ist `modbus`. Eine [externe Komponente laden](https://esphome.io/components/external_components.html), eine [UART einrichten](https://esphome.io/components/uart.html) und den [Modbus-Hub](https://esphome.io/components/modbus.html) anhängen beschreibt ESPHome.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [tcp_uart, uart_tcp]
```

## Inhalt

- [`tcp_uart`](#tcp_uart)
  - [Client](#tcp_uart-client)
  - [Server](#tcp_uart-server)
- [`uart_tcp`](#uart_tcp)
  - [Client](#uart_tcp-client)
  - [Server](#uart_tcp-server)
- [Beide Rollen](#beide-rollen)

## tcp_uart

Ersetzt eine UART. Keine Pins. Die Id wird einer anderen Komponente als `uart_id` gegeben. Ein Eintrag ist ein Socket: er wählt oder er lauscht.

### `tcp_uart` Client

Dieses Gerät baut die Verbindung auf. `host` ist Pflicht. `role` ist standardmäßig `client` und kann wegbleiben.

```yaml
tcp_uart:
  - id: remote_serial
    host: 192.0.2.20
    port: 5000
```

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `host` | — | Pflicht. |
| `port` | — | Pflicht. Port auf diesem Host. |
| `protocol` | `raw` | `modbus`, wenn die Gegenstelle Modbus-TCP spricht. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl oder Abbruch. |
| `connected` | — | Optional. An, solange diese TCP-Verbindung steht. |

### `tcp_uart` Server

Ein anderes Gerät baut die Verbindung auf. `host` nicht setzen. `role: server` ist Pflicht. Gleichzeitig nur ein Client.

```yaml
tcp_uart:
  - id: local_serial
    role: server
    port: 5000
```

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `role` | — | Pflicht. Setze `server`. |
| `port` | — | Pflicht. Port auf diesem Gerät. |
| `protocol` | `raw` | `modbus`, wenn die Gegenstelle Modbus-TCP spricht. |
| `reconnect_interval` | 5s | Pause nach fehlgeschlagenem Lauschen oder Abbruch. |
| `connected` | — | Optional. An, solange diese TCP-Verbindung steht. |

## uart_tcp

Kopiert Bytes zwischen einer Hardware-UART und einem TCP-Socket. Die Pins sind eine [UART](https://esphome.io/components/uart.html). Ein Eintrag ist ein Socket: er wählt oder er lauscht.

### `uart_tcp` Client

Dieses Gerät baut die Verbindung auf. `role: client` ist Pflicht, weil diese Komponente sonst lauscht. `host` ist Pflicht.

```yaml
uart_tcp:
  - uart_id: bus
    role: client
    host: 192.0.2.20
    port: 5000
```

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `uart_id` | — | Pflicht. Die Hardware-UART. |
| `role` | — | Pflicht. Setze `client`. |
| `host` | — | Pflicht. |
| `port` | — | Pflicht. Port auf diesem Host. |
| `protocol` | `raw` | `modbus`, wenn die Gegenstelle Modbus-TCP spricht. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl oder Abbruch. |
| `connected` | — | Optional. An, solange diese TCP-Verbindung steht. |
| `response_timeout` | 300ms | Nur bei `protocol: modbus`. |

### `uart_tcp` Server

Ein anderes Gerät baut die Verbindung auf. `host` nicht setzen. `role` ist standardmäßig `server` und kann wegbleiben. Gleichzeitig nur ein Client.

```yaml
uart_tcp:
  - uart_id: bus
    port: 5000
```

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `uart_id` | — | Pflicht. Die Hardware-UART. |
| `port` | — | Pflicht. Port auf diesem Gerät. |
| `protocol` | `raw` | `modbus`, wenn die Gegenstelle Modbus-TCP spricht. |
| `reconnect_interval` | 5s | Pause nach fehlgeschlagenem Lauschen oder Abbruch. |
| `connected` | — | Optional. An, solange diese TCP-Verbindung steht. |
| `response_timeout` | 300ms | Nur bei `protocol: modbus`. |

## Beide Rollen

Zwei Einträge, einer wählt, einer lauscht. Jeder hat eine eigene Id und einen eigenen Port.

```yaml
tcp_uart:
  - id: remote_serial
    host: 192.0.2.20
    port: 5000
  - id: local_serial
    role: server
    port: 5001
```

`uart_tcp` funktioniert gleich: zwei Einträge unter `uart_tcp:`, jeder mit eigener `uart_id`.
