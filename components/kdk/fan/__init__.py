import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan
from esphome.const import (
    CONF_ID,
)

from .. import (
    KDK_CLIENT_SCHEMA,
    kdk_ns,
    register_kdk_connection_client,
)

CODEOWNERS = ["TzeWey"]
DEPENDENCIES = ["kdk"]

KdkFan = kdk_ns.class_("KdkFan", fan.Fan)


CONFIG_SCHEMA = cv.All(
    fan.FAN_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(KdkFan),
        }
    )
    .extend(KDK_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await fan.register_fan(var, config)
    await register_kdk_connection_client(var, config)
