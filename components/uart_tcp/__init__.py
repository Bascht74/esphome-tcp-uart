import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, socket, text_sensor, uart
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    CONF_PORT,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
)
from esphome.types import ConfigType

DEPENDENCIES = ["network", "socket", "uart"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor", "socket"]
MULTI_CONF = True

uart_tcp_ns = cg.esphome_ns.namespace("uart_tcp")
UartTcp = uart_tcp_ns.class_("UartTcp", cg.Component, uart.UARTDevice, uart.UARTComponent)

CONF_HOST = "host"
CONF_ROLE = "role"
CONF_PROTOCOL = "protocol"
CONF_RECONNECT_INTERVAL = "reconnect_interval"
CONF_RESPONSE_TIMEOUT = "response_timeout"
CONF_CONNECTED = "connected"
CONF_ADDRESS = "address"
CONF_TIMEOUT = "timeout"
CONF_DISCONNECTS = "disconnects"
CONF_ALLOWED_HOSTS = "allowed_hosts"
CONF_TAP_PORT = "tap_port"


def _validate(config: ConfigType) -> ConfigType:
    if config[CONF_ROLE] == "server" and CONF_HOST in config:
        raise cv.Invalid("host is only used when role is client", path=[CONF_HOST])
    if config[CONF_ROLE] == "client" and CONF_HOST not in config:
        raise cv.Invalid("host is required when role is client", path=[CONF_HOST])
    if config[CONF_ROLE] == "client" and CONF_ALLOWED_HOSTS in config:
        raise cv.Invalid("allowed_hosts is only used when role is server", path=[CONF_ALLOWED_HOSTS])
    if config[CONF_ROLE] != "server" and CONF_TAP_PORT in config:
        raise cv.Invalid("tap_port is only used when role is server", path=[CONF_TAP_PORT])
    if config[CONF_ROLE] == "server":
        listens = 2 if CONF_TAP_PORT in config else 1
        clients = 3 if CONF_TAP_PORT in config else 1
        socket.consume_sockets(listens, "uart_tcp", socket.SocketType.TCP_LISTEN)(config)
        socket.consume_sockets(clients, "uart_tcp")(config)
    else:
        socket.consume_sockets(1, "uart_tcp")(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UartTcp),
            cv.Required(CONF_PORT): cv.port,
            cv.Optional(CONF_BAUD_RATE, default=9600): cv.int_range(min=1),
            cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
            cv.Optional(CONF_PARITY, default="NONE"): cv.enum(uart.UART_PARITY_OPTIONS, upper=True),
            cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
            cv.Optional(CONF_ROLE, default="server"): cv.one_of("client", "server", lower=True),
            cv.Optional(CONF_PROTOCOL, default="raw"): cv.one_of("raw", "modbus", lower=True),
            cv.Optional(CONF_HOST): cv.string,
            cv.Optional(CONF_RECONNECT_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_RESPONSE_TIMEOUT, default="300ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TIMEOUT, default="0s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TAP_PORT): cv.port,
            cv.Optional(CONF_ALLOWED_HOSTS): cv.ensure_list(cv.string),
            cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_DISCONNECTS): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_ADDRESS): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:ip-network",
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_server(config[CONF_ROLE] == "server"))
    cg.add(var.set_modbus(config[CONF_PROTOCOL] == "modbus"))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    if (host := config.get(CONF_HOST)) is not None:
        cg.add(var.set_host(host))
    if (tap_port := config.get(CONF_TAP_PORT)) is not None:
        cg.add(var.set_tap_port(tap_port))
    for host in config.get(CONF_ALLOWED_HOSTS, []):
        cg.add(var.add_allowed(host))
    if (connected := config.get(CONF_CONNECTED)) is not None:
        cg.add(var.set_connected_sensor(await binary_sensor.new_binary_sensor(connected)))
    if (disconnects := config.get(CONF_DISCONNECTS)) is not None:
        cg.add(var.set_drop_sensor(await sensor.new_sensor(disconnects)))
    if (address := config.get(CONF_ADDRESS)) is not None:
        cg.add(var.set_address_sensor(await text_sensor.new_text_sensor(address)))
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
    cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
    cg.add(var.set_parity(config[CONF_PARITY]))
    cg.add(var.set_rx_buffer_size(256))
