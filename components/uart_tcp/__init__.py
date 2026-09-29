import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_PORT

DEPENDENCIES = ["network", "uart"]
MULTI_CONF = True

uart_tcp_ns = cg.esphome_ns.namespace("uart_tcp")
UartTcp = uart_tcp_ns.class_("UartTcp", cg.Component, uart.UARTDevice)

CONF_HOST = "host"
CONF_ROLE = "role"
CONF_PROTOCOL = "protocol"
CONF_RECONNECT_INTERVAL = "reconnect_interval"
CONF_RESPONSE_TIMEOUT = "response_timeout"


def _validate(config):
    if config[CONF_ROLE] == "server" and CONF_HOST in config:
        raise cv.Invalid("host is only used when role is client", path=[CONF_HOST])
    if config[CONF_ROLE] == "client" and CONF_HOST not in config:
        raise cv.Invalid("host is required when role is client", path=[CONF_HOST])
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UartTcp),
            cv.Required(CONF_PORT): cv.port,
            cv.Optional(CONF_ROLE, default="server"): cv.one_of("client", "server", lower=True),
            cv.Optional(CONF_PROTOCOL, default="raw"): cv.one_of("raw", "modbus", lower=True),
            cv.Optional(CONF_HOST): cv.string,
            cv.Optional(CONF_RECONNECT_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_RESPONSE_TIMEOUT, default="300ms"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_server(config[CONF_ROLE] == "server"))
    cg.add(var.set_modbus(config[CONF_PROTOCOL] == "modbus"))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))
    if CONF_HOST in config:
        cg.add(var.set_host(config[CONF_HOST]))
