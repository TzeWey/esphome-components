import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import (
    CONF_TYPE,
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
    CONF_DEFAULT_TRANSITION_LENGTH,
)

from .. import (
    KDK_CLIENT_SCHEMA,
    kdk_ns,
    register_kdk_connection_client,
)

CODEOWNERS = ["TzeWey"]
DEPENDENCIES = ["kdk"]

KdkLight = kdk_ns.class_("KdkLight", light.LightOutput)
KdkLightType = kdk_ns.enum("KdkLightType", is_class=True)

LIGHT_TYPE = {
    "MAIN_LIGHT": KdkLightType.MAIN_LIGHT,
    "NIGHT_LIGHT": KdkLightType.NIGHT_LIGHT,
}

CONFIG_SCHEMA = cv.All(
    light.light_schema(KdkLight, light.LightType.BRIGHTNESS_ONLY).extend(
        {
            cv.Optional(CONF_TYPE, default="MAIN_LIGHT"): cv.enum(LIGHT_TYPE, upper=True),
            cv.Optional(CONF_COLD_WHITE_COLOR_TEMPERATURE, default="6000K"): cv.color_temperature,
            cv.Optional(CONF_WARM_WHITE_COLOR_TEMPERATURE, default="2700K"): cv.color_temperature,
            cv.Optional(CONF_DEFAULT_TRANSITION_LENGTH, default="0s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(KDK_CLIENT_SCHEMA),
    cv.has_none_or_all_keys(
        [CONF_COLD_WHITE_COLOR_TEMPERATURE, CONF_WARM_WHITE_COLOR_TEMPERATURE]
    ),
    light.validate_color_temperature_channels,
)


async def to_code(config):
    var = await light.new_light(config)
    await register_kdk_connection_client(var, config)

    if cold_white_color_temperature := config.get(CONF_COLD_WHITE_COLOR_TEMPERATURE):
        cg.add(var.set_cold_white_temperature(cold_white_color_temperature))
    if warm_white_color_temperature := config.get(CONF_WARM_WHITE_COLOR_TEMPERATURE):
        cg.add(var.set_warm_white_temperature(warm_white_color_temperature))
    if light_type := config.get(CONF_TYPE):
        cg.add(var.set_type(light_type))
