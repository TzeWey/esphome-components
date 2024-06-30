import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)

from .. import (
    KDK_CLIENT_SCHEMA,
    kdk_ns,
    register_kdk_connection_client,
)

CODEOWNERS = ["TzeWey"]
DEPENDENCIES = ["kdk"]

KdkLight = kdk_ns.class_("KdkLight", light.LightOutput)


CONFIG_SCHEMA = cv.All(
    light.LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(KdkLight),
            cv.Optional(CONF_COLD_WHITE_COLOR_TEMPERATURE, default="6000K"): cv.color_temperature,
            cv.Optional(CONF_WARM_WHITE_COLOR_TEMPERATURE, default="2700K"): cv.color_temperature,
        }
    )
    .extend(KDK_CLIENT_SCHEMA),
    cv.has_none_or_all_keys(
        [CONF_COLD_WHITE_COLOR_TEMPERATURE, CONF_WARM_WHITE_COLOR_TEMPERATURE]
    ),
    light.validate_color_temperature_channels,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await register_kdk_connection_client(var, config)

    if cold_white_color_temperature := config.get(CONF_COLD_WHITE_COLOR_TEMPERATURE):
        cg.add(var.set_cold_white_temperature(cold_white_color_temperature))
    if warm_white_color_temperature := config.get(CONF_WARM_WHITE_COLOR_TEMPERATURE):
        cg.add(var.set_warm_white_temperature(warm_white_color_temperature))
