import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_RECEIVE_TIMEOUT,
)

CODEOWNERS = ["TzeWey"]
DEPENDENCIES = ["uart"]

MULTI_CONF = True

CONF_KDK_CONN_ID = "kdk_conn_id"
CONF_KDK_CONN_POLL_INTERVAL = "poll_interval"

kdk_ns = cg.esphome_ns.namespace("kdk")
KdkConnectionManager = kdk_ns.class_("KdkConnectionManager", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(KdkConnectionManager),
            cv.Optional(CONF_RECEIVE_TIMEOUT, default="500ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_KDK_CONN_POLL_INTERVAL, default="15s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.polling_component_schema("100ms"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

KDK_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_KDK_CONN_ID): cv.use_id(KdkConnectionManager),
    }
)


async def register_kdk_connection_client(var, config):
    parent = await cg.get_variable(config[CONF_KDK_CONN_ID])
    cg.add(parent.register_client(var))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_receive_timeout(config[CONF_RECEIVE_TIMEOUT]))
    cg.add(var.set_poll_interval(config[CONF_KDK_CONN_POLL_INTERVAL]))
