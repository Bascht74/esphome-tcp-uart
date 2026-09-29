# esphome-modbus-tcp-uart

Zwei TCP-Brücken für ESPHome 2026.8 oder neuer. Lizenz: MIT.

[English](README.md)

| Komponente | Was sie tut |
|---|---|
| `tcp_uart` | Eine UART ohne Pins. `protocol: raw` oder `protocol: modbus`, Client oder Server. |
| `uart_tcp` | Hardware-UART auf TCP. `raw` kopiert Bytes. `modbus` wandelt RTU und MBAP. |

Keine der beiden ist eine Sensor-Plattform. Ein Sensor bleibt `platform: modbus_controller` aus ESPHome. `tcp_uart` ersetzt nur die UART, die der `modbus:`-Hub liest.

## tcp_uart

Ersetzt eine UART. Sie hat keine Pins. Ein Eintrag ist entweder Client oder Server. `protocol` gilt für beide Rollen:

| `protocol` | Socket | Was ESPHome liest und schreibt |
|---|---|---|
| `raw` | unveränderte Bytes | dieselben Bytes |
| `modbus` | Modbus-TCP (MBAP) | Modbus-RTU |

```yaml
external_components:
  - source: github://Bascht74/esphome-modbus-tcp-uart
    components: [tcp_uart]

tcp_uart:
  - id: meter
    role: client
    host: 192.0.2.10
    port: 502
    protocol: modbus

modbus:
  - id: tcp_bus
    uart_id: meter
    role: client
    send_wait_time: 200ms

modbus_controller:
  - id: device_1
    modbus_id: tcp_bus
    address: 1
    update_interval: 1s
```

Mehrere Controller dürfen sich eine `modbus_id` teilen. Der Hub hält eine Anfrage in der Luft. `send_wait_time` bleibt unter `modbus`.

`role: server` lauscht. Es gibt keinen zweiten Satz Schlüssel. Dieselben gelten, nur `host` ist verboten:

| Schlüssel | Standard | Am Server |
|---|---|---|
| `port` | — | Pflicht. Der lokale Lauschport. |
| `protocol` | `raw` | `raw` kopiert Bytes. `modbus` spricht MBAP und gibt RTU an den Hub. |
| `reconnect_interval` | 5s | Neuer Versuch nach fehlgeschlagenem Binden und nach einem Abbruch. |
| `baud_rate` | 9600 | Wird nicht gesendet. Bei `protocol: modbus` nutzt der Hub sie nur als Timer. |

Gleichzeitig nur ein TCP-Client. Bei `protocol: modbus` ist der `modbus:`-Hub `role: server`, die Register liegen auf `modbus_server`, nicht hier. Die Transaktionsnummer der Anfrage wird zurückgegeben. Ein echter RTU-Bus bleibt an seiner Hardware-UART.

```yaml
tcp_uart:
  - id: modbus_link
    role: server
    port: 502
    protocol: modbus

modbus:
  - id: server_bus
    uart_id: modbus_link
    role: server

modbus_server:
  - modbus_id: server_bus
    address: 1
```

## uart_tcp

Kopiert Bytes zwischen einer echten UART und einem TCP-Socket. Baud, Datenbits, Parität und Stoppbits stehen an diesem `uart:`-Eintrag. Sie takten die Pins. `port` ist der TCP-Port. Bei `role: server` ist gleichzeitig nur ein TCP-Client verbunden.

`protocol: raw` kopiert Bytes unverändert. Ein Modbus-RTU-Gerät liegt dann auf TCP als RTU, ohne MBAP-Kopf.

`protocol: modbus` spricht auf dem Socket Modbus-TCP und an den Pins Modbus-RTU. `role: server` nimmt einen TCP-Master an und fragt den Bus. `role: client` wählt einen TCP-Slave und reicht RTU-Anfragen eines Masters an den Pins weiter.

```yaml
external_components:
  - source: github://Bascht74/esphome-modbus-tcp-uart
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
    port: 502
    protocol: modbus
    response_timeout: 300ms
```

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `uart_id` | — | Hardware-UART. Baud und Rahmen stehen dort. |
| `port` | — | Pflicht, TCP-Port. |
| `role` | `server` | `server` lauscht. `client` wählt `host`. |
| `protocol` | `raw` | `raw` kopiert Bytes. `modbus` wandelt MBAP und RTU. |
| `host` | — | Pflicht für Client, verboten für Server. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl, Abbruch oder fehlgeschlagenem Lauschen. |
| `response_timeout` | 300ms | Nur bei `protocol: modbus`. |

## Wo die Baudrate wirkt

`tcp_uart` speichert 9600 8N1 und taktet keine Bits. Der Hub liest die Zahl in `Modbus::setup()`:

- Pause zwischen Frames = 3,5 Zeichenzeiten, bei 9600 etwa 4 ms
- geschätzte Sendezeit = Framelänge × Bits je Zeichen / Baud

Beides sind Timer im Hub. Sie ändern den TCP-Strom nicht. Ein echter Bus setzt `baud_rate` an seinem eigenen `uart:`-Eintrag. `tcp_uart` speichert `baud_rate` für dieselbe Art von Prüfung und nutzt sie nicht als Takt. `uart_tcp` nutzt die Baudrate seiner Hardware-UART sowohl zum Takten der Pins als auch, bei `protocol: modbus`, als Pause von 3,5 Zeichen zwischen RTU-Frames.

## Tests

GitHub Actions installiert ESPHome 2026.9.0 und führt [script/ci](script/ci) aus. Das prüft jede Datei in `tests/` mit `esphome config`. Dateien in `tests/invalid/` müssen abgelehnt werden. Ein zweiter Job kompiliert [tests/compile.yaml](tests/compile.yaml) für den ESP32 (ESP-IDF). Dabei werden `tcp_uart` in beiden Protokollen und `uart_tcp` gebaut.

```bash
pip install "esphome==2026.9.0"
./script/ci
esphome compile tests/compile.yaml
```

## Kompatibilität

Beide Komponenten implementieren nur die UART-Byte-Methoden. Sie rufen keine Interna von `modbus_controller` auf. Das Entfernen der Helper-Shims in 2026.10 trifft sie nicht. Sie brechen, wenn `UARTComponent` eine neue pure virtual Methode bekommt. Dieser Satz ist in 2026.9.0 und im aktuellen `dev` gleich.

`skip_updates`, `force_new_range` und `command_throttle` akzeptiert der Controller in 2026.9 noch. Sie ändern das Pollen nicht mehr. Entfernt werden sie 2027.2 und 2027.3.
