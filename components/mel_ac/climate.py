import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome.const import (
    CONF_ID,
    CONF_MAX_REFRESH_RATE,
    CONF_STARTUP_DELAY,
    CONF_VISUAL,
    CONF_MIN_TEMPERATURE,
    CONF_MAX_TEMPERATURE,
    CONF_TEMPERATURE_STEP,
    CONF_TARGET_TEMPERATURE,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_SWING_MODES,
    CONF_SUPPORTED_FAN_MODES,
)
from esphome.components.climate import (
    CONF_CURRENT_TEMPERATURE,
    CLIMATE_MODES,
    CLIMATE_FAN_MODES,
    CLIMATE_SWING_MODES,
)

CODEOWNERS = ["TzeWey"]
DEPENDENCIES = ["climate", "uart"]

PROTOCOL_MIN_TEMPERATURE = 16.0
PROTOCOL_MAX_TEMPERATURE = 31.0
PROTOCOL_TEMPERATURE_STEP_MIN = 0.5
PROTOCOL_TEMPERATURE_STEP_MAX = 1.0

SUPPORTED_MODES = {x: CLIMATE_MODES[x] for x in ("OFF", "HEAT_COOL", "COOL", "HEAT", "DRY", "FAN_ONLY", "AUTO")}
SUPPORTED_FAN_MODES = {x: CLIMATE_FAN_MODES[x] for x in ("AUTO", "LOW", "MEDIUM", "HIGH", "FOCUS", "QUIET")}
SUPPORTED_SWING_MODES = {x: CLIMATE_SWING_MODES[x] for x in ("OFF", "BOTH", "VERTICAL", "HORIZONTAL")}

mel_ac_ns = cg.esphome_ns.namespace("mel").namespace("ac")
MelAirConditioner = mel_ac_ns.class_(
    "MelAirConditioner", climate.Climate, cg.PollingComponent, uart.UARTDevice)


def validate_visual_temperature_step(value):
    if (value == PROTOCOL_TEMPERATURE_STEP_MIN) or (value == PROTOCOL_TEMPERATURE_STEP_MAX):
        return
    raise cv.Invalid(
        f"visual temperature_step must be {PROTOCOL_TEMPERATURE_STEP_MIN} or {PROTOCOL_TEMPERATURE_STEP_MAX}")


def validate_visual(config):
    if CONF_VISUAL not in config:
        config[CONF_VISUAL] = {
            CONF_MIN_TEMPERATURE: PROTOCOL_MIN_TEMPERATURE,
            CONF_MAX_TEMPERATURE: PROTOCOL_MAX_TEMPERATURE,
            CONF_TEMPERATURE_STEP: {
                CONF_TARGET_TEMPERATURE: PROTOCOL_TEMPERATURE_STEP_MIN,
                CONF_CURRENT_TEMPERATURE: PROTOCOL_TEMPERATURE_STEP_MIN,
            },
        }
        return config

    visual_config = config[CONF_VISUAL]
    if CONF_MIN_TEMPERATURE in visual_config:
        min_temp = visual_config[CONF_MIN_TEMPERATURE]
        if min_temp < PROTOCOL_MIN_TEMPERATURE:
            raise cv.Invalid(
                f"visual min_temperature {min_temp} must be greater than {PROTOCOL_MIN_TEMPERATURE}"
            )
    else:
        config[CONF_VISUAL][CONF_MIN_TEMPERATURE] = PROTOCOL_MIN_TEMPERATURE

    if CONF_MAX_TEMPERATURE in visual_config:
        max_temp = visual_config[CONF_MAX_TEMPERATURE]
        if max_temp > PROTOCOL_MAX_TEMPERATURE:
            raise cv.Invalid(
                f"visual max_temperature {max_temp} must be less than {PROTOCOL_MAX_TEMPERATURE}"
            )
    else:
        config[CONF_VISUAL][CONF_MAX_TEMPERATURE] = PROTOCOL_MAX_TEMPERATURE

    if CONF_TEMPERATURE_STEP in visual_config:
        temp_step = config[CONF_VISUAL][CONF_TEMPERATURE_STEP]
        if isinstance(temp_step, dict):
            validate_visual_temperature_step(temp_step[CONF_TARGET_TEMPERATURE])
            validate_visual_temperature_step(temp_step[CONF_CURRENT_TEMPERATURE])
        else:
            validate_visual_temperature_step(temp_step)
    else:
        config[CONF_VISUAL][CONF_TEMPERATURE_STEP] = {
            CONF_TARGET_TEMPERATURE: PROTOCOL_TEMPERATURE_STEP_MIN,
            CONF_CURRENT_TEMPERATURE: PROTOCOL_TEMPERATURE_STEP_MIN,
        }

    return config


CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MelAirConditioner),
            cv.Optional(CONF_MAX_REFRESH_RATE, default="1s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_STARTUP_DELAY, default="0s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SUPPORTED_MODES, default=[]
                        ): cv.ensure_list(cv.enum(SUPPORTED_MODES, upper=True)),
            cv.Optional(CONF_SUPPORTED_FAN_MODES, default=[]
                        ): cv.ensure_list(cv.enum(SUPPORTED_FAN_MODES, upper=True)),
            cv.Optional(CONF_SUPPORTED_SWING_MODES, default=["VERTICAL"]
                        ): cv.ensure_list(cv.enum(SUPPORTED_SWING_MODES, upper=True)),
        }
    )
    .extend(cv.polling_component_schema("100ms"))
    .extend(uart.UART_DEVICE_SCHEMA),
    validate_visual,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)

    cg.add(var.set_poll_refresh_rate(config[CONF_MAX_REFRESH_RATE]))
    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY]))

    cg.add(var.set_supported_modes(config[CONF_SUPPORTED_MODES]))
    cg.add(var.set_supported_fan_modes(config[CONF_SUPPORTED_FAN_MODES]))
    cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))
