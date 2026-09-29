# esphome-tcp-uart

Zwei Wege, eine UART auf TCP zu legen. ESPHome 2026.8 oder neuer. Lizenz: MIT.

[English](README.md)

| Du willst | Komponente |
|---|---|
| Keine Pins. Eine Komponente nutzt diese Id als `uart_id`. | `tcp_uart` |
| Die Pins bleiben eine echte UART. Die Bytes gehen auf TCP. | `uart_tcp` |

Die Bytes bleiben unverändert, außer `protocol` ist `modbus`. Eine [externe Komponente laden](https://esphome.io/components/external_components.html), eine [UART einrichten](https://esphome.io/components/uart.html) und den [Modbus-Hub](https://esphome.io/components/modbus.html) anhängen beschreibt ESPHome.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [tcp_uart, uart_tcp]
```

Wähle Client oder Server. Beides in einem Eintrag geht nicht.

## Client

Dieses Gerät baut die TCP-Verbindung auf. `host` ist Pflicht.

### Ohne Pins

`role` ist standardmäßig `client` und kann wegbleiben. Die Id wird einer Komponente als `uart_id` gegeben. Der Socket ist kein Pin und hat keinen Pegel. `baud_rate` wird nur gespeichert, damit die Komponente sie lesen kann. Sie wird nicht gesendet.

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
| `baud_rate` | 9600 | Nicht auf der Leitung. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl oder Abbruch. |

### Mit Pins

`role: client` ist hier Pflicht, weil diese Komponente sonst lauscht. Pins und Baudrate bleiben am [`uart:`](https://esphome.io/components/uart.html)-Eintrag.

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
| `response_timeout` | 300ms | Nur bei `protocol: modbus`. |

## Server

Ein anderes Gerät baut die TCP-Verbindung auf. `host` nicht setzen. Gleichzeitig nur ein Client.

### Ohne Pins

```yaml
tcp_uart:
  - id: local_serial
    role: server
    port: 5000
```

`local_serial` wird als `uart_id` übergeben. `baud_rate` wird gespeichert und nicht gesendet.

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `role` | — | Pflicht. Setze `server`. |
| `port` | — | Pflicht. Port auf diesem Gerät. |
| `protocol` | `raw` | `modbus`, wenn die Gegenstelle Modbus-TCP spricht. |
| `baud_rate` | 9600 | Nicht auf der Leitung. |
| `reconnect_interval` | 5s | Pause nach fehlgeschlagenem Lauschen oder Abbruch. |

### Mit Pins

`role` ist standardmäßig `server` und kann wegbleiben. Pins und Baudrate bleiben am [`uart:`](https://esphome.io/components/uart.html)-Eintrag.

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
| `response_timeout` | 300ms | Nur bei `protocol: modbus`. |

## Tests

[script/ci](script/ci) prüft jede Datei in `tests/` mit ESPHome 2026.9.0. Eine Datei in `tests/invalid/` muss fehlschlagen, und das Protokoll muss den Satz aus der passenden `.expect`-Datei enthalten. [tests/compile.yaml](tests/compile.yaml) wird für den ESP32 kompiliert.

```bash
pip install "esphome==2026.9.0"
./script/ci
esphome compile tests/compile.yaml
```
