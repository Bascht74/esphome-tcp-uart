import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_PORT

DEPENDENCIES = ["network", "uart"]
MULTI_CONF = True

modbus_tcp_uart_ns = cg.esphome_ns.namespace("modbus_tcp_uart")
ModbusTcpUart = modbus_tcp_uart_ns.class_("ModbusTcpUart", uart.UARTComponent, cg.Component)

CONF_RECONNECT_INTERVAL = "reconnect_interval"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ModbusTcpUart),
        cv.Required("host"): cv.string,
        cv.Optional(CONF_PORT, default=502): cv.port,
        cv.Optional(CONF_RECONNECT_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_host(config["host"]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    cg.add(var.set_baud_rate(9600))
    cg.add(var.set_rx_buffer_size(512))
