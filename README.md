# esphome-tcp-uart

This repository adds two components that connect a [UART bus](https://esphome.io/components/uart.html) to a TCP socket.

- `tcp_uart` has no pins. Another component uses its id as `uart_id`.
- `uart_tcp` copies bytes between a hardware UART and a TCP socket.

Bytes are copied unchanged. Set `protocol: modbus` when the other end speaks Modbus TCP. Loading an [external component](https://esphome.io/components/external_components.html) and setting up [Modbus](https://esphome.io/components/modbus.html) are documented by ESPHome.

For ESPHome 2026.8 or newer.

[Deutsche Fassung](README.de.md)

```yaml
external_components:
  - source: github://Bascht74/esphome-tcp-uart
    components: [tcp_uart, uart_tcp]
```

## Contents

- [`tcp_uart`](#tcp_uart)
  - [Client](#tcp_uart-client)
  - [Server](#tcp_uart-server)
- [`uart_tcp`](#uart_tcp)
  - [Client](#uart_tcp-client)
  - [Server](#uart_tcp-server)
  - [Sharing the pins](#sharing-the-pins)
- [Both roles](#both-roles)

## tcp_uart

This component allows ESPHome to use a TCP connection as a UART bus. There are no pins. Any component that has a `uart_id` option can use the id. One entry opens one socket.

> [!NOTE]
> There is no `baud_rate` option. The connection is a TCP socket, not a serial line.

### tcp_uart client

The device connects to a remote host. `host` is required. `role` defaults to `client`.

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

#### Configuration variables

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **host** (**Required**, string): The host to connect to. An IPv4 address. On the ESP32, a hostname can be used as well.
- **port** (**Required**, int): The TCP port to connect to.
- **protocol** (*Optional*, string): `raw` or `modbus`. Defaults to `raw`.
- **reconnect_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): The time to wait before connecting again after a failed or closed connection. Defaults to `5s`.
- **timeout** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): Close the socket and connect again after this long with no bytes. Defaults to `0s`, which leaves the socket open.
- **connected** (*Optional*): A [binary sensor](https://esphome.io/components/binary_sensor.html) that reports whether the TCP connection is established.
- **disconnects** (*Optional*): A [sensor](https://esphome.io/components/sensor.html) that counts how often the TCP connection dropped since boot.
- **address** (*Optional*): A [text sensor](https://esphome.io/components/text_sensor.html) that publishes `ip:port` of the other end, for example `192.0.2.20:5000`.

### tcp_uart server

The device listens. Do not set `host`. `role: server` is required. Only one client is connected at a time.

```yaml
# Example configuration entry
tcp_uart:
  - id: local_serial
    role: server
    port: 5000
```

#### Configuration variables

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **role** (**Required**, string): Set `server`.
- **port** (**Required**, int): The TCP port to listen on.
- **protocol** (*Optional*, string): `raw` or `modbus`. Defaults to `raw`.
- **reconnect_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): The time to wait before listening again after a failure. Defaults to `5s`.
- **timeout** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): Close a client that has been silent this long. Defaults to `0s`, which leaves the connection open.
- **allowed_hosts** (*Optional*, list): IP addresses that may connect. If omitted, any address may connect.
- **connected** (*Optional*): A [binary sensor](https://esphome.io/components/binary_sensor.html) that reports whether the TCP connection is established.
- **disconnects** (*Optional*): A [sensor](https://esphome.io/components/sensor.html) that counts how often the TCP connection dropped since boot.
- **address** (*Optional*): A [text sensor](https://esphome.io/components/text_sensor.html) that publishes `ip:port` of the connected client. Empty until a client connects. The port is the port this device listens on.

## uart_tcp

This component allows bytes to be transferred between a hardware [UART bus](https://esphome.io/components/uart.html) and a TCP socket. Data is copied unchanged in both directions. One entry opens one socket.

> [!NOTE]
> Baud rate, data bits, parity and stop bits are set on the UART bus. They are not options of this component. `rx_buffer_size` on that UART is documented on the [UART bus](https://esphome.io/components/uart.html) page.

### uart_tcp client

The device connects to a remote host. `role: client` is required, because this component listens unless told otherwise. `host` is required.

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

#### Configuration variables

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **uart_id** (**Required**, [ID](https://esphome.io/guides/configuration-types#id)): The UART bus to use.
- **role** (**Required**, string): Set `client`.
- **host** (**Required**, string): The host to connect to. An IPv4 address. On the ESP32, a hostname can be used as well.
- **port** (**Required**, int): The TCP port to connect to.
- **protocol** (*Optional*, string): `raw` or `modbus`. Defaults to `raw`.
- **reconnect_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): The time to wait before connecting again after a failed or closed connection. Defaults to `5s`.
- **timeout** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): Close the socket and connect again after this long with no bytes. Defaults to `0s`, which leaves the socket open.
- **response_timeout** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): How long to wait for the UART answer when `protocol` is `modbus`. Defaults to `300ms`.
- **connected** (*Optional*): A [binary sensor](https://esphome.io/components/binary_sensor.html) that reports whether the TCP connection is established.
- **disconnects** (*Optional*): A [sensor](https://esphome.io/components/sensor.html) that counts how often the TCP connection dropped since boot.
- **address** (*Optional*): A [text sensor](https://esphome.io/components/text_sensor.html) that publishes `ip:port` of the other end, for example `192.0.2.20:5000`.

### uart_tcp server

The device listens. Do not set `host`. `role` defaults to `server`. Only one client is connected at a time.

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

#### Configuration variables

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **uart_id** (**Required**, [ID](https://esphome.io/guides/configuration-types#id)): The UART bus to use.
- **port** (**Required**, int): The TCP port to listen on.
- **protocol** (*Optional*, string): `raw` or `modbus`. Defaults to `raw`.
- **reconnect_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): The time to wait before listening again after a failure. Defaults to `5s`.
- **timeout** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): Close a client that has been silent this long. Defaults to `0s`, which leaves the connection open.
- **allowed_hosts** (*Optional*, list): IP addresses that may connect. If omitted, any address may connect.
- **response_timeout** (*Optional*, [Time](https://esphome.io/guides/configuration-types#time)): How long to wait for the UART answer when `protocol` is `modbus`. Defaults to `300ms`.
- **tap_port** (*Optional*, int): A second port. Connections there receive both directions and cannot send.
- **connected** (*Optional*): A [binary sensor](https://esphome.io/components/binary_sensor.html) that reports whether the TCP connection is established.
- **disconnects** (*Optional*): A [sensor](https://esphome.io/components/sensor.html) that counts how often the TCP connection dropped since boot.
- **address** (*Optional*): A [text sensor](https://esphome.io/components/text_sensor.html) that publishes `ip:port` of the connected client. Empty until a client connects.

### Sharing the pins

With `protocol: modbus` and `role: server`, use the id as `uart_id` of a local Modbus hub. The component uses the pins. The local controller and one TCP client each send one frame. One frame is sent at a time, the local frame first, and the answer is returned only to the sender.

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

## Both roles

To connect and to listen, add two entries. Each entry has its own id and its own port.

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

`uart_tcp` works the same way: two entries under `uart_tcp:`, each with its own `uart_id`.
