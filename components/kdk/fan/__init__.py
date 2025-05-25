import esphome.config_validation as cv
from esphome.components import fan


from .. import (
    KDK_CLIENT_SCHEMA,
    kdk_ns,
    register_kdk_connection_client,
)

CODEOWNERS = ["TzeWey"]
DEPENDENCIES = ["kdk"]

KdkFan = kdk_ns.class_("KdkFan", fan.Fan)


CONFIG_SCHEMA = cv.All(
    fan.fan_schema(KdkFan)
    .extend(KDK_CLIENT_SCHEMA)
)


async def to_code(config):
    var = await fan.new_fan(config)
    await register_kdk_connection_client(var, config)
