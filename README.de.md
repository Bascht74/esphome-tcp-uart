# esphome-tcp-uart

Zwei Wege, eine UART auf TCP zu legen. ESPHome 2026.8 oder neuer. Lizenz: MIT.

[English](README.md)

| Komponente | Richtung |
|---|---|
| `tcp_uart` | Eine Komponente mit `uart_id` spricht mit einem TCP-Socket. Keine Pins. |
| `uart_tcp` | Die Pins sind eine echte UART. Das andere Ende ist ein TCP-Socket. |

Die Bytes sind auf beiden Seiten dieselben. `protocol: modbus` ist optional und steht unten. Die Komponenten sind keine Sensoren. Alles, was schon `uart_id` nimmt, kann sie nutzen.

## tcp_uart

Ersetzt eine UART. Die Gegenseite muss bereits rohes TCP sprechen. Es gibt keine GPIO-Pins und keinen RS-232-Pegel. Ein Eintrag ist Client oder Server, nicht beides.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [tcp_uart]

tcp_uart:
  - id: remote_serial
    role: client
    host: 192.0.2.20
    port: 5000
```

`role: server` nutzt dieselben Schlüssel. `host` ist dann verboten. Gleichzeitig nur ein TCP-Client.

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `role` | `client` | `client` wählt `host`. `server` lauscht. |
| `host` | — | Pflicht für Client, verboten für Server. |
| `port` | — | Pflicht. Zielport oder lokaler Lauschport. |
| `protocol` | `raw` | `raw` kopiert Bytes. `modbus` steht im Abschnitt unten. |
| `baud_rate` | 9600 | Wird nur gespeichert. Sie geht nicht auf die Leitung und taktet keinen Pin. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl, Abbruch oder fehlgeschlagenem Lauschen. |

## uart_tcp

Kopiert Bytes zwischen einer Hardware-UART und einem TCP-Socket. Baud, Datenbits, Parität und Stoppbits stehen an diesem `uart:`-Eintrag, weil diese Pins getaktet werden. `port` ist der TCP-Port. `role: server` nimmt einen Client an.

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [uart_tcp]

uart:
  - id: bus
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 9600
    data_bits: 8
    parity: NONE
    stop_bits: 1

uart_tcp:
  - uart_id: bus
    role: server
    port: 5000
```

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `uart_id` | — | Hardware-UART. Baud und Rahmen stehen dort. |
| `port` | — | Pflicht, TCP-Port. |
| `role` | `server` | `server` lauscht. `client` wählt `host`. |
| `protocol` | `raw` | `raw` kopiert Bytes. `modbus` steht im Abschnitt unten. |
| `host` | — | Pflicht für Client, verboten für Server. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl, Abbruch oder fehlgeschlagenem Lauschen. |
| `response_timeout` | 300ms | Nur bei `protocol: modbus`. |

Ein Client ist derselbe Eintrag mit `role: client` und einem `host`.

## Baudrate

`tcp_uart` taktet keine Bits. Die `baud_rate` liegt nur da, damit eine Komponente sie zurücklesen kann. `uart_tcp` nutzt die Baudrate der Hardware-UART, um die Pins zu takten. Der Socket selbst hat keine Baudrate.

## protocol: modbus

Optional. Setzen, wenn die Gegenstelle Modbus-TCP spricht. Der Standard bleibt `raw`. Bei `uart_tcp` gilt `response_timeout` nur in diesem Modus.

```yaml
tcp_uart:
  - id: meter
    role: client
    host: 192.0.2.10
    port: 502
    protocol: modbus
```

## Tests

GitHub Actions installiert ESPHome 2026.9.0 und führt [script/ci](script/ci) aus. Das prüft jede Datei in `tests/` mit `esphome config`. Jede Datei in `tests/invalid/` muss abgelehnt werden, und die Ausgabe muss den Satz aus der passenden `.expect`-Datei enthalten. Ein Fehlschlag aus einem anderen Grund zählt nicht. Ein zweiter Job kompiliert [tests/compile.yaml](tests/compile.yaml) für den ESP32.

```bash
pip install "esphome==2026.9.0"
./script/ci
esphome compile tests/compile.yaml
```

## Kompatibilität

Beide Komponenten implementieren nur die UART-Byte-Methoden. Sie brechen, wenn `UARTComponent` eine neue pure virtual Methode bekommt. Dieser Satz ist in 2026.9.0 und im aktuellen `dev` gleich.
