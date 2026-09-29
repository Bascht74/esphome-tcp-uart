# esphome-modbus-tcp-uart

Drei TCP-Brücken für ESPHome 2026.8 oder neuer. Lizenz: MIT.

[English](README.md)

| Komponente | Was sie tut |
|---|---|
| `modbus_tcp_uart` | Dasselbe wie `tcp_uart` mit `protocol: modbus`. Bleibt, damit vorhandene YAML weiter lädt. |
| `tcp_uart` | Eine UART ohne Pins. `protocol: raw` oder `protocol: modbus`, Client oder Server. |
| `uart_tcp` | Hardware-UART auf TCP. `raw` kopiert Bytes. `modbus` wandelt RTU und MBAP. |

Keine der beiden ist eine Sensor-Plattform. Ein Sensor bleibt `platform: modbus_controller` aus ESPHome. Diese Komponenten ersetzen nur die UART, die der `modbus:`-Hub liest.

## modbus_tcp_uart

Ein Eintrag ist entweder Client oder Server. Beides gleichzeitig sind zwei Einträge.

```yaml
external_components:
  - source: github://Bascht74/esphome-modbus-tcp-uart
    components: [modbus_tcp_uart]

modbus_tcp_uart:
  - id: tcp_link
    host: 192.0.2.10
    port: 502

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

Mehrere Controller dürfen sich eine `modbus_id` teilen. Der Hub hält eine Anfrage in der Luft und gibt die Antwort an den Auftraggeber zurück. Diese Komponente hat keine eigene Geräteliste.

Server, gleichzeitig ein TCP-Client. Dahinter spricht der normale `modbus_server` RTU. Ein echter RTU-Bus bleibt an seiner Hardware-UART.

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

| Schlüssel | Standard | Bedeutung |
|---|---|---|
| `role` | `client` | `client` wählt `host`. `server` lauscht. |
| `host` | — | IPv4 oder Hostname. Pflicht für Client, verboten für Server. |
| `port` | 502 | Zielport oder lokaler Lauschport. |
| `reconnect_interval` | 5s | Pause nach Fehlwahl, Abbruch oder fehlgeschlagenem Lauschen. |

`send_wait_time` bleibt unter `modbus`.

## tcp_uart

Ersetzt eine UART. Sie hat keine Pins. `protocol` wählt die Bytes auf dem Socket, beim Client und beim Server:

| `protocol` | Socket | Was ESPHome liest und schreibt |
|---|---|---|
| `raw` | unveränderte Bytes | dieselben Bytes |
| `modbus` | Modbus-TCP (MBAP) | Modbus-RTU |

`modbus_tcp_uart` ist diese Komponente mit `protocol: modbus` und Port 502. Neue YAML kann beide Namen verwenden.

```yaml
tcp_uart:
  - id: remote_serial
    role: client
    host: 192.0.2.20
    port: 5000
    protocol: raw
  - id: meter
    role: client
    host: 192.0.2.10
    port: 502
    protocol: modbus
  - id: modbus_server
    role: server
    port: 502
    protocol: modbus
```

`port` ist Pflicht. `baud_rate` ist standardmäßig 9600 und wird nicht gesendet. Bei `protocol: modbus` nutzt der Hub sie nur als Timer. Die eigenen UART-Pins werden nicht weitergereicht. Das macht `uart_tcp`.

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

`modbus_tcp_uart` speichert 9600 8N1 und taktet keine Bits. Der Hub liest die Zahl in `Modbus::setup()`:

- Pause zwischen Frames = 3,5 Zeichenzeiten, bei 9600 etwa 4 ms
- geschätzte Sendezeit = Framelänge × Bits je Zeichen / Baud

Beides sind Timer im Hub. Sie ändern den TCP-Strom nicht. Ein echter Bus setzt `baud_rate` an seinem eigenen `uart:`-Eintrag. `tcp_uart` speichert `baud_rate` für dieselbe Art von Prüfung und nutzt sie nicht als Takt. `uart_tcp` nutzt die Baudrate seiner Hardware-UART sowohl zum Takten der Pins als auch, bei `protocol: modbus`, als Pause von 3,5 Zeichen zwischen RTU-Frames.

## Kompatibilität

Beide Komponenten implementieren nur die UART-Byte-Methoden. Sie rufen keine Interna von `modbus_controller` auf. Das Entfernen der Helper-Shims in 2026.10 trifft sie nicht. Sie brechen, wenn `UARTComponent` eine neue pure virtual Methode bekommt. Dieser Satz ist in 2026.9.0 und im aktuellen `dev` gleich.

`skip_updates`, `force_new_range` und `command_throttle` akzeptiert der Controller in 2026.9 noch. Sie ändern das Pollen nicht mehr. Entfernt werden sie 2027.2 und 2027.3.
