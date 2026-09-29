import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, socket, text_sensor, uart
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    CONF_PORT,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
)

DEPENDENCIES = ["network", "socket"]
AUTO_LOAD = ["uart", "binary_sensor", "sensor", "text_sensor", "socket"]
MULTI_CONF = True

tcp_uart_ns = cg.esphome_ns.namespace("tcp_uart")
TcpUart = tcp_uart_ns.class_("TcpUart", uart.UARTComponent, cg.Component)

CONF_HOST = "host"
CONF_ROLE = "role"
CONF_PROTOCOL = "protocol"
CONF_RECONNECT_INTERVAL = "reconnect_interval"
CONF_CONNECTED = "connected"
CONF_ADDRESS = "address"
CONF_STALL_TIMEOUT = "stall_timeout"
CONF_IDLE_TIMEOUT = "idle_timeout"
CONF_DISCONNECTS = "disconnects"
CONF_ALLOWED_HOSTS = "allowed_hosts"


def _validate(config):
    if config[CONF_ROLE] == "server" and CONF_HOST in config:
        raise cv.Invalid("host is only used when role is client", path=[CONF_HOST])
    if config[CONF_ROLE] == "client" and CONF_HOST not in config:
        raise cv.Invalid("host is required when role is client", path=[CONF_HOST])
    if config[CONF_ROLE] == "server" and config[CONF_STALL_TIMEOUT].total_milliseconds != 0:
        raise cv.Invalid("stall_timeout is only used when role is client", path=[CONF_STALL_TIMEOUT])
    if config[CONF_ROLE] == "client" and config[CONF_IDLE_TIMEOUT].total_milliseconds != 0:
        raise cv.Invalid("idle_timeout is only used when role is server", path=[CONF_IDLE_TIMEOUT])
    if config[CONF_ROLE] == "client" and CONF_ALLOWED_HOSTS in config:
        raise cv.Invalid("allowed_hosts is only used when role is server", path=[CONF_ALLOWED_HOSTS])
    if config[CONF_ROLE] == "server":
        socket.consume_sockets(1, "tcp_uart", socket.SocketType.TCP_LISTEN)(config)
        socket.consume_sockets(1, "tcp_uart")(config)
    else:
        socket.consume_sockets(1, "tcp_uart")(config)
    return config


ITEM_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TcpUart),
            cv.Optional(CONF_ROLE, default="client"): cv.one_of("client", "server", lower=True),
            cv.Optional(CONF_PROTOCOL, default="raw"): cv.one_of("raw", "modbus", lower=True),
            cv.Optional(CONF_HOST): cv.string,
            cv.Required(CONF_PORT): cv.port,
            cv.Optional(CONF_BAUD_RATE, default=9600): cv.int_range(min=1),
            cv.Optional(CONF_RECONNECT_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_STALL_TIMEOUT, default="0s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_IDLE_TIMEOUT, default="0s"): cv.positive_time_period_milliseconds,
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
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


CONFIG_SCHEMA = ITEM_SCHEMA


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_server(config[CONF_ROLE] == "server"))
    cg.add(var.set_modbus(config[CONF_PROTOCOL] == "modbus"))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    cg.add(var.set_stall_timeout(config[CONF_STALL_TIMEOUT]))
    cg.add(var.set_idle_timeout(config[CONF_IDLE_TIMEOUT]))
    if CONF_HOST in config:
        cg.add(var.set_host(config[CONF_HOST]))
    for host in config.get(CONF_ALLOWED_HOSTS, []):
        cg.add(var.add_allowed(host))
    if CONF_CONNECTED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_CONNECTED])
        cg.add(var.set_connected_sensor(sens))
    if CONF_DISCONNECTS in config:
        drops = await sensor.new_sensor(config[CONF_DISCONNECTS])
        cg.add(var.set_drop_sensor(drops))
    if CONF_ADDRESS in config:
        address = await text_sensor.new_text_sensor(config[CONF_ADDRESS])
        cg.add(var.set_address_sensor(address))
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(8))
    cg.add(var.set_stop_bits(1))
    cg.add(var.set_rx_buffer_size(1024))
