# esphome-tcp-uart

Dieses Repository fügt zwei Komponenten hinzu, die eine [UART](https://esphome.io/components/uart.html) mit einem TCP-Socket verbinden.

- `tcp_uart` hat keine Pins. Eine andere Komponente nutzt die Id als `uart_id`.
- `uart_tcp` kopiert Bytes zwischen einer Hardware-UART und einem TCP-Socket.

Die Bytes werden unverändert kopiert. Spricht die Gegenseite Modbus-TCP, kommt `modbus_tcp` auf die `tcp_uart`-Id, und der Modbus-Hub zeigt darauf. Eine [externe Komponente laden](https://esphome.io/components/external_components.html) und [Modbus](https://esphome.io/components/modbus.html) einrichten beschreibt ESPHome.

Für ESPHome 2026.8 oder neuer.

[English](README.md)

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

Diese Komponente ermöglicht es, eine TCP-Verbindung als UART zu verwenden. Es gibt keine Pins. Jede Komponente mit einer `uart_id` kann die Id nutzen. Ein Eintrag öffnet einen Socket.

> [!NOTE]
> Es gibt keine `baud_rate`. Die Verbindung ist ein TCP-Socket, keine serielle Leitung.

### tcp_uart client

Das Gerät verbindet sich mit einem Host. `host` ist Pflicht. `role` ist standardmäßig `client`.

```yaml
# Example configuration entry
tcp_uart:
  - id: remote_serial
    host: 192.0.2.20
    port: 5000

modbus:
  - id: modbus_bus
    uart_id: remote_serial
```

Modbus-TCP (MBAP, Port 502) ist dieser rohe Socket nicht. `modbus_tcp` nimmt den Kopf ab. Der Hub nutzt diese Id:

```yaml
tcp_uart:
  - id: meter_tcp
    host: 192.0.2.10
    port: 502

modbus_tcp:
  - id: meter_link
    tcp_uart_id: meter_tcp

modbus:
  - id: meter_bus
    uart_id: meter_link
    role: client
```

#### Konfigurationsvariablen

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): Die Id für die Code-Erzeugung.
- **host** (**Pflicht**, string): Der Host, zu dem verbunden wird. Eine IPv4-Adresse. Auf dem ESP32 kann auch ein Hostname verwendet werden.
- **port** (**Pflicht**, int): Der TCP-Port, zu dem verbunden wird.
- **protocol** (*Optional*, string): `raw` oder `modbus`. Standard ist `raw`.
- **reconnect_interval** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Wartezeit, bevor nach einem Fehler oder Abbruch erneut verbunden wird. Standard ist `5s`.
- **timeout** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Socket schließen und erneut verbinden, wenn so lange keine Bytes kamen. Standard ist `0s`. Dann bleibt der Socket offen.
- **connected** (*Optional*): Ein [binärer Sensor](https://esphome.io/components/binary_sensor.html), der meldet, ob die TCP-Verbindung steht.
- **disconnects** (*Optional*): Ein [Sensor](https://esphome.io/components/sensor.html), der zählt, wie oft die TCP-Verbindung seit dem Start abbrach.
- **address** (*Optional*): Ein [Textsensor](https://esphome.io/components/text_sensor.html), der `ip:port` der Gegenseite meldet, zum Beispiel `192.0.2.20:5000`.

### tcp_uart server

Das Gerät lauscht. `host` nicht setzen. `role: server` ist Pflicht. Gleichzeitig ist nur ein Client verbunden.

```yaml
# Example configuration entry
tcp_uart:
  - id: local_serial
    role: server
    port: 5000
```

#### Konfigurationsvariablen

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): Die Id für die Code-Erzeugung.
- **role** (**Pflicht**, string): `server` setzen.
- **port** (**Pflicht**, int): Der TCP-Port, auf dem gelauscht wird.
- **protocol** (*Optional*, string): `raw` oder `modbus`. Standard ist `raw`.
- **reconnect_interval** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Wartezeit, bevor nach einem Fehler erneut gelauscht wird. Standard ist `5s`.
- **timeout** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Einen Client trennen, der so lange stumm war. Standard ist `0s`. Dann bleibt die Verbindung offen.
- **allowed_hosts** (*Optional*, Liste): IP-Adressen, die verbinden dürfen. Ohne diese Option darf jede Adresse verbinden.
- **connected** (*Optional*): Ein [binärer Sensor](https://esphome.io/components/binary_sensor.html), der meldet, ob die TCP-Verbindung steht.
- **disconnects** (*Optional*): Ein [Sensor](https://esphome.io/components/sensor.html), der zählt, wie oft die TCP-Verbindung seit dem Start abbrach.
- **address** (*Optional*): Ein [Textsensor](https://esphome.io/components/text_sensor.html), der `ip:port` des verbundenen Clients meldet. Leer, bis ein Client verbunden ist. Der Port ist der Port, auf dem dieses Gerät lauscht.

## uart_tcp

Diese Komponente überträgt Bytes zwischen einer Hardware-[UART](https://esphome.io/components/uart.html) und einem TCP-Socket. Die Daten werden in beide Richtungen unverändert kopiert. Ein Eintrag öffnet einen Socket.

> [!NOTE]
> Baudrate, Datenbits, Parität und Stoppbits stehen an der UART. Sie sind keine Optionen dieser Komponente. `rx_buffer_size` beschreibt die [UART](https://esphome.io/components/uart.html).

### uart_tcp client

Das Gerät verbindet sich mit einem Host. `role: client` ist Pflicht, weil die Komponente sonst lauscht. `host` ist Pflicht.

```yaml
# Example configuration entry
uart:
  - id: uart_bus
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 9600

uart_tcp:
  - id: uart_tcp_1
    uart_id: uart_bus
    role: client
    host: 192.0.2.20
    port: 5000
```

#### Konfigurationsvariablen

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): Die Id für die Code-Erzeugung.
- **uart_id** (**Pflicht**, [ID](https://esphome.io/guides/configuration-types#id)): Die UART, die verwendet wird.
- **role** (**Pflicht**, string): `client` setzen.
- **host** (**Pflicht**, string): Der Host, zu dem verbunden wird. Eine IPv4-Adresse. Auf dem ESP32 kann auch ein Hostname verwendet werden.
- **port** (**Pflicht**, int): Der TCP-Port, zu dem verbunden wird.
- **protocol** (*Optional*, string): `raw` oder `modbus`. Standard ist `raw`.
- **reconnect_interval** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Wartezeit, bevor nach einem Fehler oder Abbruch erneut verbunden wird. Standard ist `5s`.
- **timeout** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Socket schließen und erneut verbinden, wenn so lange keine Bytes kamen. Standard ist `0s`. Dann bleibt der Socket offen.
- **response_timeout** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Wie lange bei `protocol: modbus` auf die Antwort der UART gewartet wird. Standard ist `300ms`.
- **connected** (*Optional*): Ein [binärer Sensor](https://esphome.io/components/binary_sensor.html), der meldet, ob die TCP-Verbindung steht.
- **disconnects** (*Optional*): Ein [Sensor](https://esphome.io/components/sensor.html), der zählt, wie oft die TCP-Verbindung seit dem Start abbrach.
- **address** (*Optional*): Ein [Textsensor](https://esphome.io/components/text_sensor.html), der `ip:port` der Gegenseite meldet, zum Beispiel `192.0.2.20:5000`.

### uart_tcp server

Das Gerät lauscht. `host` nicht setzen. `role` ist standardmäßig `server`. Gleichzeitig ist nur ein Client verbunden.

```yaml
# Example configuration entry
uart:
  - id: uart_bus
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 9600

uart_tcp:
  - id: uart_tcp_1
    uart_id: uart_bus
    port: 5000
```

#### Konfigurationsvariablen

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): Die Id für die Code-Erzeugung.
- **uart_id** (**Pflicht**, [ID](https://esphome.io/guides/configuration-types#id)): Die UART, die verwendet wird.
- **port** (**Pflicht**, int): Der TCP-Port, auf dem gelauscht wird.
- **protocol** (*Optional*, string): `raw` oder `modbus`. Standard ist `raw`.
- **reconnect_interval** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Wartezeit, bevor nach einem Fehler erneut gelauscht wird. Standard ist `5s`.
- **timeout** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Einen Client trennen, der so lange stumm war. Standard ist `0s`. Dann bleibt die Verbindung offen.
- **allowed_hosts** (*Optional*, Liste): IP-Adressen, die verbinden dürfen. Ohne diese Option darf jede Adresse verbinden.
- **response_timeout** (*Optional*, [Zeit](https://esphome.io/guides/configuration-types#time)): Wie lange bei `protocol: modbus` auf die Antwort der UART gewartet wird. Standard ist `300ms`.
- **tap_port** (*Optional*, int): Ein zweiter Port. Verbindungen dort empfangen beide Richtungen und können nicht senden.
- **connected** (*Optional*): Ein [binärer Sensor](https://esphome.io/components/binary_sensor.html), der meldet, ob die TCP-Verbindung steht.
- **disconnects** (*Optional*): Ein [Sensor](https://esphome.io/components/sensor.html), der zählt, wie oft die TCP-Verbindung seit dem Start abbrach.
- **address** (*Optional*): Ein [Textsensor](https://esphome.io/components/text_sensor.html), der `ip:port` des verbundenen Clients meldet. Leer, bis ein Client verbunden ist.

### Pins teilen

Mit `protocol: modbus` und `role: server` die Id als `uart_id` eines lokalen Modbus-Hubs verwenden. Die Komponente nutzt die Pins. Der lokale Controller und ein TCP-Client senden je ein Telegramm. Es wird immer nur eines gesendet, das lokale zuerst, und die Antwort geht nur an den Absender zurück.

```yaml
# Example configuration entry
uart_tcp:
  - id: gate
    uart_id: uart_bus
    port: 502
    protocol: modbus

modbus:
  - id: local_bus
    uart_id: gate
```

## Beide Rollen

Zum Verbinden und zum Lauschen zwei Einträge anlegen. Jeder Eintrag hat eine eigene Id und einen eigenen Port.

```yaml
# Example configuration entry
tcp_uart:
  - id: remote_serial
    host: 192.0.2.20
    port: 5000
  - id: local_serial
    role: server
    port: 5001
```

`uart_tcp` funktioniert genauso: zwei Einträge unter `uart_tcp:`, jeder mit eigener `uart_id`.
