# esphome-tcp-uart

Zwei Wege, eine UART auf TCP zu legen. ESPHome 2026.8 oder neuer. Lizenz: MIT.

[English](README.md)

| Komponente | Richtung |
|---|---|
| `tcp_uart` | Eine Komponente mit `uart_id` spricht mit einem TCP-Socket. Keine Pins. |
| `uart_tcp` | Die Pins sind eine echte UART. Das andere Ende ist ein TCP-Socket. |

Die Bytes bleiben unverändert, außer `protocol` ist `modbus`. Wie man eine [externe Komponente lädt](https://esphome.io/components/external_components.html), wie eine [UART](https://esphome.io/components/uart.html) eingerichtet wird und wie der [Modbus-Hub](https://esphome.io/components/modbus.html) `uart_id` benutzt, steht in der ESPHome-Doku.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [tcp_uart]
```

## tcp_uart

Ersetzt eine UART. Die Gegenseite muss bereits TCP sprechen. Keine Pins, keine Pegel. Ein Eintrag ist Client oder Server.

```yaml
tcp_uart:
  - id: remote_serial
    role: client
    host: 192.0.2.20
    port: 5000
```

`remote_serial` wird als `uart_id` übergeben. `role: server` nutzt dieselben Schlüssel und verbietet `host`. Gleichzeitig nur ein TCP-Client.

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `role` | `client` | `client` wählt `host`. `server` lauscht. |
| `host` | — | Pflicht für Client, verboten für Server. |
| `port` | — | Pflicht. Zielport oder lokaler Lauschport. |
| `protocol` | `raw` | `raw` kopiert Bytes. `modbus` verpackt sie für eine Modbus-TCP-Gegenstelle. |
| `baud_rate` | 9600 | Wird nur gespeichert, damit eine Komponente sie lesen kann. Geht nicht auf die Leitung und taktet keinen Pin. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl, Abbruch oder fehlgeschlagenem Lauschen. |

## uart_tcp

Kopiert Bytes zwischen einer Hardware-UART und einem TCP-Socket. Pins und Baudrate stehen am [`uart:`](https://esphome.io/components/uart.html)-Eintrag, auf den diese Komponente zeigt. `role: server` nimmt einen Client an. Ein Client ist derselbe Eintrag plus `host`.

```yaml
uart_tcp:
  - uart_id: bus
    role: server
    port: 5000
```

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `uart_id` | — | Die Hardware-UART. |
| `port` | — | Pflicht, TCP-Port. |
| `role` | `server` | `server` lauscht. `client` wählt `host`. |
| `protocol` | `raw` | `raw` kopiert Bytes. `modbus` verpackt sie für eine Modbus-TCP-Gegenstelle. |
| `host` | — | Pflicht für Client, verboten für Server. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl, Abbruch oder fehlgeschlagenem Lauschen. |
| `response_timeout` | 300ms | Nur bei `protocol: modbus`. |

## Baudrate

Der Socket hat keine Baudrate. `tcp_uart` taktet keine Bits. `uart_tcp` nutzt die Baudrate der Hardware-UART für die Pins.

## Tests

[script/ci](script/ci) prüft jede Datei in `tests/` mit ESPHome 2026.9.0. Eine Datei in `tests/invalid/` muss fehlschlagen, und das Protokoll muss den Satz aus der passenden `.expect`-Datei enthalten. [tests/compile.yaml](tests/compile.yaml) wird für den ESP32 kompiliert.

```bash
pip install "esphome==2026.9.0"
./script/ci
esphome compile tests/compile.yaml
```
