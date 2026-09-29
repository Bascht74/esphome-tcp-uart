# esphome-modbus-tcp-uart

Zwei TCP-Leitungen, die für ESPHome wie eine UART aussehen. ESPHome 2026.8 oder neuer. Lizenz: MIT.

[English](README.md)

| Komponente | Bytes auf dem Socket |
|---|---|
| `modbus_tcp_uart` | Modbus-TCP (MBAP). Auf der UART-Seite RTU. |
| `tcp_uart` | Dieselben Bytes, nichts dazwischen. Ersetzt eine UART. Leitet keine Pins weiter. |

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

Ersetzt eine UART für jede Komponente, die `uart_id` nutzt und nur Bytes liest und schreibt. Die Gegenseite muss bereits rohes TCP sprechen. Das ist kein Modbus-TCP, kein RS-232-Pegel und keine Brücke von den Pins: Bytes der eigenen UART-Stiftleiste werden nicht weitergereicht.

Baud, Parität und Stoppbits der echten Schnittstelle stellt die Gegenseite ein. `baud_rate` liegt hier nur, damit eine Komponente den Wert prüfen kann. Sie wird nicht gesendet und taktet keine Bits.

`port` ist Pflicht.

```yaml
external_components:
  - source: github://Bascht74/esphome-modbus-tcp-uart
    components: [tcp_uart]

tcp_uart:
  - id: remote_serial
    role: client
    host: 192.0.2.20
    port: 5000
    baud_rate: 9600
```

Ein Gateway, das eine Hardware-`uart:` auf TCP legt, ist diese Komponente nicht. Dafür brauchte es eine echte `baud_rate` an den Pins und einen eigenen TCP-`port`.

## Wo die Baudrate wirkt

`modbus_tcp_uart` speichert 9600 8N1 und taktet keine Bits. Der Hub liest die Zahl in `Modbus::setup()`:

- Pause zwischen Frames = 3,5 Zeichenzeiten, bei 9600 etwa 4 ms
- geschätzte Sendezeit = Framelänge × Bits je Zeichen / Baud

Beides sind Timer im Hub. Sie ändern den TCP-Strom nicht. Ein echter Bus setzt `baud_rate` an seinem eigenen `uart:`-Eintrag. `tcp_uart` speichert `baud_rate` für dieselbe Art von Prüfung und nutzt sie nicht als Takt.

## Kompatibilität

Beide Komponenten implementieren nur die UART-Byte-Methoden. Sie rufen keine Interna von `modbus_controller` auf. Das Entfernen der Helper-Shims in 2026.10 trifft sie nicht. Sie brechen, wenn `UARTComponent` eine neue pure virtual Methode bekommt. Dieser Satz ist in 2026.9.0 und im aktuellen `dev` gleich.

`skip_updates`, `force_new_range` und `command_throttle` akzeptiert der Controller in 2026.9 noch. Sie ändern das Pollen nicht mehr. Entfernt werden sie 2027.2 und 2027.3.
