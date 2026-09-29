import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.tcp_uart import CONF_HOST, CONF_ROLE, TcpUart, _validate
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_PORT

DEPENDENCIES = ["network"]
AUTO_LOAD = ["tcp_uart"]
MULTI_CONF = True

CONF_RECONNECT_INTERVAL = "reconnect_interval"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TcpUart),
            cv.Optional(CONF_ROLE, default="client"): cv.one_of("client", "server", lower=True),
            cv.Optional(CONF_HOST): cv.string,
            cv.Optional(CONF_PORT, default=502): cv.port,
            cv.Optional(CONF_BAUD_RATE, default=9600): cv.int_range(min=1),
            cv.Optional(CONF_RECONNECT_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_server(config[CONF_ROLE] == "server"))
    cg.add(var.set_modbus(True))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_reconnect_interval(config[CONF_RECONNECT_INTERVAL]))
    if CONF_HOST in config:
        cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
    cg.add(var.set_data_bits(8))
    cg.add(var.set_stop_bits(1))
    cg.add(var.set_rx_buffer_size(1024))
