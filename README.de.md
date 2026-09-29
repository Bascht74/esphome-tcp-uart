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
  - [Pins teilen](#pins-teilen)
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
| `stall_timeout` | 0s | Nur Client. Nach dieser Stille neu wählen. `0s` lässt den Socket stehen. |
| `connected` | — | Optional. An, solange diese TCP-Verbindung steht. |
| `disconnects` | — | Optional. Wie oft die TCP-Verbindung seit dem Start abbrach. |

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
| `idle_timeout` | 0s | Nur Server. Trennt eine Gegenstelle nach dieser Stille. `0s` lässt sie stehen. |
| `allowed_hosts` | — | Nur Server. IP-Adressen, die verbinden dürfen. Leer lässt alle zu. |
| `connected` | — | Optional. An, solange diese TCP-Verbindung steht. |
| `disconnects` | — | Optional. Wie oft die TCP-Verbindung seit dem Start abbrach. |

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
| `stall_timeout` | 0s | Nur Client. Nach dieser Stille neu wählen. `0s` lässt den Socket stehen. |
| `connected` | — | Optional. An, solange diese TCP-Verbindung steht. |
| `disconnects` | — | Optional. Wie oft die TCP-Verbindung seit dem Start abbrach. |
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
| `idle_timeout` | 0s | Nur Server. Trennt eine Gegenstelle nach dieser Stille. `0s` lässt sie stehen. |
| `allowed_hosts` | — | Nur Server. IP-Adressen, die verbinden dürfen. Leer lässt alle zu. |
| `connected` | — | Optional. An, solange diese TCP-Verbindung steht. |
| `disconnects` | — | Optional. Wie oft die TCP-Verbindung seit dem Start abbrach. |
| `response_timeout` | 300ms | Nur bei `protocol: modbus`. |
| `tap_port` | — | Zweiter Port. Verbindungen dort hören beide Richtungen und können nicht senden. |

`rx_buffer_size` an der Hardware-`uart:` ist der Puffer der Pins. Die [UART](https://esphome.io/components/uart.html) beschreibt ihn. Diese Komponenten haben keinen zweiten. Modbus-Telegramme sind kurz, der Standard reicht.

### Pins teilen

Nur `uart_tcp` mit `protocol: modbus` und lauschender Rolle. Die Id ist die `uart_id` des lokalen Modbus-Hubs. Die Komponente besitzt die Pins. Der lokale Controller und ein TCP-Client geben ihr je ein Telegramm. Sie sendet immer nur eines, das lokale zuerst, und gibt die Antwort nur an den Absender zurück.

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
