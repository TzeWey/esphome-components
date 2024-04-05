#include <cinttypes>
#include "esphome/core/log.h"

#include "mel_ac.h"

namespace esphome {
namespace mel {
namespace ac {

using namespace conn;

static const char *const TAG = "mel.ac";

/*******************************************************************************
 * PROTECTED
 ******************************************************************************/

void MelAirConditioner::request_connect(void) {
  ESP_LOGV(TAG, "REQ> CONNECT");
  uint8_t payload[2] = {MEL_COMMAND_TYPE_CONNECT, 0x1};
  this->conn_.send_command(MEL_COMMAND_FLAGS_CONNECT, payload, sizeof(payload));
}

void MelAirConditioner::request_params(void) {
  ESP_LOGV(TAG, "REQ> GET_PARAMS");
  uint8_t payload[16] = {MEL_COMMAND_TYPE_GET_PARAMS};
  this->conn_.send_command(MEL_COMMAND_FLAGS_GET, payload, sizeof(payload));
}

void MelAirConditioner::request_room_temperature(void) {
  ESP_LOGV(TAG, "REQ> GET_TEMP");
  uint8_t payload[16] = {MEL_COMMAND_TYPE_GET_TEMP};
  this->conn_.send_command(MEL_COMMAND_FLAGS_GET, payload, sizeof(payload));
}

void MelAirConditioner::request_status(void) {
  ESP_LOGV(TAG, "REQ> GET_STATUS");
  uint8_t payload[16] = {MEL_COMMAND_TYPE_GET_STATUS};
  this->conn_.send_command(MEL_COMMAND_FLAGS_GET, payload, sizeof(payload));
}

void MelAirConditioner::process_response(void) {
  if (this->conn_.is_response_timeout()) {
    ESP_LOGV(TAG, "RES> Response Timeout");
    return;
  }

  if (!this->conn_.is_response_pending()) {
    return;
  }
  this->conn_.clear_response_pending();

  const struct MelCommand *res = this->conn_.response();

  // CONNECT
  if (this->conn_.check_response_flags(MEL_COMMAND_FLAGS_CONNECT)) {
    ESP_LOGV(TAG, "RES> CONNECT");
    ESP_LOGI(TAG, "Connected to Air Conditioner");
    this->set_connected(true);
    return;
  }

  // GET
  if (this->conn_.check_response_flags(MEL_COMMAND_FLAGS_GET)) {
    auto &params = this->params();

    switch (res->payload[0]) {
      // GET_PARAMS
      case MEL_COMMAND_TYPE_GET_PARAMS: {
        ESP_LOGV(TAG, "RES> GET_PARAMS:");
        this->clear_poll_flag(MEL_POLL_GET_PARAMS);

        // POWER
        uint8_t power = res->payload[MEL_GET_PARAMS_OFFSET_POWER];
        if (params.set_power(power)) {
          ESP_LOGV(TAG, "RES>   power: 0x%02X [%s]", power, params.get_power_name().c_str());
        } else {
          ESP_LOGW(TAG, "RES>   power: 0x%02X [UNKNOWN]", power);
        }

        // ISEE
        uint8_t isee = ((res->payload[MEL_GET_PARAMS_OFFSET_MODE] & 0x8) != 0);
        if (params.set_isee(isee)) {
          ESP_LOGV(TAG, "RES>   isee: 0x%02X [%s]", isee, params.get_isee_name().c_str());
        } else {
          ESP_LOGW(TAG, "RES>   isee: 0x%02X [UNKNOWN]", isee);
        }

        // MODE
        uint8_t mode = res->payload[MEL_GET_PARAMS_OFFSET_MODE] & 0x7;
        if (params.set_mode(mode)) {
          ESP_LOGV(TAG, "RES>   mode: 0x%02X [%s]", mode, params.get_mode_name().c_str());
        } else {
          ESP_LOGW(TAG, "RES>   mode: 0x%02X [UNKNOWN]", mode);
        }

        // FAN
        uint8_t fan = res->payload[MEL_GET_PARAMS_OFFSET_FAN];
        if (params.set_fan(fan)) {
          ESP_LOGV(TAG, "RES>   fan: 0x%02X [%s]", fan, params.get_fan_name().c_str());
        } else {
          ESP_LOGW(TAG, "RES>   fan: 0x%02X [UNKNOWN]", fan);
        }

        // VANE VERT
        uint8_t vane_vert = res->payload[MEL_GET_PARAMS_OFFSET_VANE_VERT];
        if (params.set_vane_vert(vane_vert)) {
          ESP_LOGV(TAG, "RES>   vane_vert: 0x%02X [%s]", vane_vert, params.get_vane_vert_name().c_str());
        } else {
          ESP_LOGW(TAG, "RES>   vane_vert: 0x%02X [UNKNOWN]", vane_vert);
        }

        // VANE HORZ
        uint8_t vane_horz = res->payload[MEL_GET_PARAMS_OFFSET_VANE_HORZ];
        if (params.set_vane_horz(vane_horz)) {
          ESP_LOGV(TAG, "RES>   vane_horz: 0x%02X [%s]", vane_horz, params.get_vane_horz_name().c_str());
        } else {
          ESP_LOGW(TAG, "RES>   vane_horz: 0x%02X [UNKNOWN]", vane_horz);
        }

        // VANE HORZ FLAG
        params.set_vane_horz_flag((vane_horz & 0x80) != 0);
        ESP_LOGV(TAG, "RES>   vane_horz_flag: %s", TRUEFALSE(params.get_vane_horz_flag()));

        // TEMPERATURE TARGET
        float temperature;
        if (res->payload[MEL_GET_PARAMS_OFFSET_TEMPERATURE_2] != 0x00) {
          temperature = (float) (res->payload[MEL_GET_PARAMS_OFFSET_TEMPERATURE_2] & 0x7F) / 2;
          params.set_temperature_mode(MEL_AC_TEMP_MODE_2);
        } else {
          temperature = (float) (res->payload[MEL_GET_PARAMS_OFFSET_TEMPERATURE_1]) + 10.0;
          params.set_temperature_mode(MEL_AC_TEMP_MODE_1);
        }
        params.set_temperature_target(temperature);
        ESP_LOGV(TAG, "RES>   temperature_mode: %d", params.get_temperature_mode());
        ESP_LOGV(TAG, "RES>   temperature_target: %.1f", params.get_temperature_target());

        break;
      }

      // GET_TEMP - Room Temperature
      case MEL_COMMAND_TYPE_GET_TEMP: {
        ESP_LOGV(TAG, "RES> GET_TEMP:");
        this->clear_poll_flag(MEL_POLL_GET_TEMP);

        float temperature;
        if (res->payload[6] != 0x00) {
          temperature = (float) (res->payload[6] & 0x7F) / 2;
        } else {
          temperature = (float) (res->payload[3]) + 10.0;
        }
        params.set_temperature_current(temperature);
        ESP_LOGV(TAG, "RES>   temperature_current: %.1f", params.get_temperature_current());

        break;
      }

      // GET_STATUS
      case MEL_COMMAND_TYPE_GET_STATUS: {
        ESP_LOGV(TAG, "RES> GET_STATUS:");
        this->clear_poll_flag(MEL_POLL_GET_STATUS);

        uint8_t compressor_operating = res->payload[4];
        params.set_compressor_operating(compressor_operating != 0);
        ESP_LOGV(TAG, "RES>   compressor_operating: 0x%02X [%s]", compressor_operating,
                 TRUEFALSE(params.get_compressor_operating()));

        uint8_t compressor_frequency = res->payload[3];
        params.set_compressor_frequency(compressor_frequency);
        ESP_LOGV(TAG, "RES>   compressor_frequency: 0x%02X", params.get_compressor_frequency());

        break;
      }
    }
  }

  // SET
  if (this->conn_.check_response_flags(MEL_COMMAND_FLAGS_SET)) {
    const uint16_t last_error = res->payload[1] | (res->payload[2] << 8);
    if (last_error) {
      ESP_LOGW(TAG, "RES> SET failed with error 0x%04X", last_error);
    }
  }
}

void MelAirConditioner::do_publish(void) {
  // Publish Climate States
  auto &params = this->params();

  if (!params.is_updated()) {
    return;
  }

  params.clear_updated();

  this->current_temperature = params.get_temperature_current();
  this->target_temperature = params.get_temperature_target();

  switch (params.get_power()) {
    case MelAcParamPower::MEL_AC_PARAM_POWER_ON: {
      auto operating = params.get_compressor_operating();
      switch (params.get_mode()) {
        case MelAcParamMode::MEL_AC_PARAM_MODE_HEAT: {
          this->mode = climate::CLIMATE_MODE_HEAT;
          this->action = operating ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;
        } break;

        case MelAcParamMode::MEL_AC_PARAM_MODE_DRY: {
          this->mode = climate::CLIMATE_MODE_DRY;
          this->action = climate::CLIMATE_ACTION_DRYING;
        } break;

        case MelAcParamMode::MEL_AC_PARAM_MODE_COOL: {
          this->mode = climate::CLIMATE_MODE_COOL;
          this->action = operating ? climate::CLIMATE_ACTION_COOLING : climate::CLIMATE_ACTION_IDLE;
        } break;

        case MelAcParamMode::MEL_AC_PARAM_MODE_FAN: {
          this->mode = climate::CLIMATE_MODE_FAN_ONLY;
          this->action = climate::CLIMATE_ACTION_FAN;
        } break;

        case MelAcParamMode::MEL_AC_PARAM_MODE_AUTO: {
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
          if (operating && (this->current_temperature > this->target_temperature)) {
            this->action = climate::CLIMATE_ACTION_COOLING;
          } else if (this->current_temperature < this->target_temperature) {
            this->action = climate::CLIMATE_ACTION_HEATING;
          } else {
            this->action = climate::CLIMATE_ACTION_IDLE;
          }
        } break;
      }
    } break;

    case MelAcParamPower::MEL_AC_PARAM_POWER_OFF:
    default: {
      this->mode = climate::CLIMATE_MODE_OFF;
      this->action = climate::CLIMATE_ACTION_OFF;
    } break;
  }

  switch (params.get_fan()) {
    case MelAcParamFan::MEL_AC_PARAM_FAN_QUIET: {
      this->fan_mode = climate::CLIMATE_FAN_QUIET;
    } break;

    case MelAcParamFan::MEL_AC_PARAM_FAN_1: {
      this->fan_mode = climate::CLIMATE_FAN_LOW;
    } break;

    case MelAcParamFan::MEL_AC_PARAM_FAN_2: {
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
    } break;

    case MelAcParamFan::MEL_AC_PARAM_FAN_3: {
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
    } break;

    case MelAcParamFan::MEL_AC_PARAM_FAN_4: {
      this->fan_mode = climate::CLIMATE_FAN_FOCUS;
    } break;

    case MelAcParamFan::MEL_AC_PARAM_FAN_AUTO:
    default: {
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
    } break;
  }

  switch (params.get_vane_vert()) {
    case MelAcParamVaneVert::MEL_AC_PARAM_VANE_VERT_SWING: {
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    } break;

    default: {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    } break;
  }

  this->publish_state();
}

void MelAirConditioner::do_connect(void) {
  static uint32_t attempt = 0;
  if (this->get_connected()) {
    return;
  }
  this->request_connect();
  attempt++;
  if ((attempt > 0) && (attempt % 25 == 0)) {
    ESP_LOGW(TAG, "Attempted CONNECT %" PRIu32 " times. Please verify connection or change the baud rate.", attempt);
  }
}

bool MelAirConditioner::do_update(void) {
  auto &params = this->set_params();

  if (!params.is_pending()) {
    return false;
  }
  params.set_pending(false);

  uint16_t control = 0;
  uint8_t payload[16] = {MEL_COMMAND_TYPE_SET_PARAMS};

  if (params.get_power().has_value()) {
    auto power = params.get_power().value();
    payload[MEL_SET_PARAMS_OFFSET_POWER] = (uint8_t) power;
    control |= MEL_COMMAND_SET_POWER;
    ESP_LOGD(TAG, "CONTROL> power: 0x%02X [%s]", power, params.get_power_name().c_str());
  }

  if (params.get_mode().has_value()) {
    auto mode = params.get_mode().value();
    payload[MEL_SET_PARAMS_OFFSET_MODE] = (uint8_t) mode;
    control |= MEL_COMMAND_SET_MODE;
    ESP_LOGD(TAG, "CONTROL> mode: 0x%02X [%s]", mode, params.get_mode_name().c_str());
  }

  if (params.get_temperature().has_value()) {
    auto value = params.get_temperature().value();

    value = value < MEL_AC_TEMP_MIN ? MEL_AC_TEMP_MIN : value;
    value = value > MEL_AC_TEMP_MAX ? MEL_AC_TEMP_MAX : value;

    if (this->params().get_temperature_mode() == MelAcTemperatureMode::MEL_AC_TEMP_MODE_2) {
      payload[MEL_SET_PARAMS_OFFSET_TEMPERATURE_2] = (uint8_t) round(value * 2) | 0x80;
    } else {
      payload[MEL_SET_PARAMS_OFFSET_TEMPERATURE_1] = (uint8_t) (31 - value);
    }

    control |= MEL_COMMAND_SET_TEMPERATURE;
    ESP_LOGD(TAG, "CONTROL> temperature: %.1f", value);
  }

  if (params.get_fan().has_value()) {
    auto fan = params.get_fan().value();
    payload[MEL_SET_PARAMS_OFFSET_FAN] = (uint8_t) fan;
    control |= MEL_COMMAND_SET_FAN;
    ESP_LOGD(TAG, "CONTROL> fan: 0x%02X [%s]", fan, params.get_fan_name().c_str());
  }

  if (params.get_vane_vert().has_value()) {
    auto vane = params.get_vane_vert().value();
    payload[MEL_SET_PARAMS_OFFSET_VANE_VERT] = (uint8_t) vane;
    control |= MEL_COMMAND_SET_VANE_VERT;
    ESP_LOGD(TAG, "CONTROL> vane_vert: 0x%02X [%s]", vane, params.get_vane_vert_name().c_str());
  }

  if (params.get_vane_horz().has_value()) {
    auto vane_horz = params.get_vane_horz().value();
    payload[MEL_SET_PARAMS_OFFSET_VANE_HORZ] = (uint8_t) vane_horz;
    control |= MEL_COMMAND_SET_VANE_HORZ;
    ESP_LOGD(TAG, "CONTROL> vane_horz: 0x%02X [%s]", vane_horz, params.get_vane_horz_name().c_str());
  }

  if (control == 0) {
    return false;  // Nothing was set, return early
  }

  payload[1] = (uint8_t) control & 0xFF;
  payload[2] = (uint8_t) (control >> 8) & 0xFF;

  ESP_LOGV(TAG, "REQ> SET_PARAMS");
  this->conn_.send_command(MEL_COMMAND_FLAGS_SET, payload, sizeof(payload));

  return true;
}

void MelAirConditioner::do_poll(void) {
  if (!this->get_connected()) {
    this->set_poll_flag(MEL_POLL_ALL);
    return;
  }

  if (this->do_update()) {
    return;
  }

  if (this->check_poll_flag(MEL_POLL_GET_PARAMS)) {
    this->request_params();
    return;
  }

  if (this->check_poll_flag(MEL_POLL_GET_TEMP)) {
    this->request_room_temperature();
    return;
  }

  if (this->check_poll_flag(MEL_POLL_GET_STATUS)) {
    this->request_status();
    return;
  }

  if (this->check_poll_idle()) {
    this->restart_poll();
    this->do_publish();
  }
}

void MelAirConditioner::control(const climate::ClimateCall &call) {
  auto &params = this->set_params();

  if (call.get_mode().has_value()) {
    auto mode = call.get_mode().value();
    this->mode = mode;
    switch (mode) {
      case climate::CLIMATE_MODE_COOL: {
        params.set_power(MelAcParamPower::MEL_AC_PARAM_POWER_ON);
        params.set_mode(MelAcParamMode::MEL_AC_PARAM_MODE_COOL);
      } break;

      case climate::CLIMATE_MODE_HEAT: {
        params.set_power(MelAcParamPower::MEL_AC_PARAM_POWER_ON);
        params.set_mode(MelAcParamMode::MEL_AC_PARAM_MODE_HEAT);
      } break;

      case climate::CLIMATE_MODE_DRY: {
        params.set_power(MelAcParamPower::MEL_AC_PARAM_POWER_ON);
        params.set_mode(MelAcParamMode::MEL_AC_PARAM_MODE_DRY);
      } break;

      case climate::CLIMATE_MODE_HEAT_COOL: {
        params.set_power(MelAcParamPower::MEL_AC_PARAM_POWER_ON);
        params.set_mode(MelAcParamMode::MEL_AC_PARAM_MODE_AUTO);
      } break;

      case climate::CLIMATE_MODE_FAN_ONLY: {
        params.set_power(MelAcParamPower::MEL_AC_PARAM_POWER_ON);
        params.set_mode(MelAcParamMode::MEL_AC_PARAM_MODE_FAN);
      } break;

      case climate::CLIMATE_MODE_AUTO: {
        params.set_power(MelAcParamPower::MEL_AC_PARAM_POWER_ON);
        params.set_mode(MelAcParamMode::MEL_AC_PARAM_MODE_AUTO);
      } break;

      case climate::CLIMATE_MODE_OFF:
      default: {
        params.set_power(MelAcParamPower::MEL_AC_PARAM_POWER_OFF);
      } break;
    }
  }

  if (call.get_target_temperature().has_value()) {
    auto temperature = call.get_target_temperature().value();
    this->target_temperature = temperature;
    params.set_temperature(temperature);
  }

  if (call.get_fan_mode().has_value()) {
    auto fan_mode = call.get_fan_mode().value();
    this->fan_mode = fan_mode;
    switch (fan_mode) {
      case climate::CLIMATE_FAN_QUIET: {
        params.set_fan(MelAcParamFan::MEL_AC_PARAM_FAN_QUIET);
      } break;

      case climate::CLIMATE_FAN_AUTO: {
        params.set_fan(MelAcParamFan::MEL_AC_PARAM_FAN_AUTO);
      } break;

      case climate::CLIMATE_FAN_LOW: {
        params.set_fan(MelAcParamFan::MEL_AC_PARAM_FAN_1);
      } break;

      case climate::CLIMATE_FAN_MEDIUM: {
        params.set_fan(MelAcParamFan::MEL_AC_PARAM_FAN_2);
      } break;

      case climate::CLIMATE_FAN_HIGH: {
        params.set_fan(MelAcParamFan::MEL_AC_PARAM_FAN_3);
      } break;

      case climate::CLIMATE_FAN_FOCUS: {
        params.set_fan(MelAcParamFan::MEL_AC_PARAM_FAN_4);
      } break;

      default: {
        ESP_LOGW(TAG, "> Unsupported fan mode: %s", LOG_STR_ARG(climate_fan_mode_to_string(fan_mode)));
      } break;
    }
  }

  if (call.get_swing_mode().has_value()) {
    auto swing_mode = call.get_swing_mode().value();
    this->swing_mode = swing_mode;
    switch (swing_mode) {
      case climate::CLIMATE_SWING_OFF: {
        params.set_vane_vert(MelAcParamVaneVert::MEL_AC_PARAM_VANE_VERT_AUTO);
        params.set_vane_horz(MelAcParamVaneHorz::MEL_AC_PARAM_VANE_HORZ_AUTO);
      } break;

      case climate::CLIMATE_SWING_HORIZONTAL: {
        params.set_vane_vert(MelAcParamVaneVert::MEL_AC_PARAM_VANE_VERT_AUTO);
        params.set_vane_horz(MelAcParamVaneHorz::MEL_AC_PARAM_VANE_HORZ_SWING);
      } break;

      case climate::CLIMATE_SWING_VERTICAL: {
        params.set_vane_vert(MelAcParamVaneVert::MEL_AC_PARAM_VANE_VERT_SWING);
        params.set_vane_horz(MelAcParamVaneHorz::MEL_AC_PARAM_VANE_HORZ_AUTO);
      } break;

      case climate::CLIMATE_SWING_BOTH: {
        params.set_vane_vert(MelAcParamVaneVert::MEL_AC_PARAM_VANE_VERT_SWING);
        params.set_vane_horz(MelAcParamVaneHorz::MEL_AC_PARAM_VANE_HORZ_SWING);
      } break;

      default: {
        ESP_LOGW(TAG, "> Unsupported swing mode: %s", LOG_STR_ARG(climate_swing_mode_to_string(swing_mode)));
      } break;
    }
  }

  params.set_pending(true);
  this->publish_state();
}

/*******************************************************************************
 * STATIC
 ******************************************************************************/

void validate_baud_rate(uint32_t baud_rate) {
  int count{std::extent<decltype(MEL_SUPPORTED_BAUD_RATES)>::value};

  for (int i = 0; i < count; i++) {
    if (MEL_SUPPORTED_BAUD_RATES[i] == baud_rate) {
      return;  // is a supported baud rate
    }
  }

  ESP_LOGE(TAG, "Unsupported UART baud_rate: %" PRIu32 "!", baud_rate);
  ESP_LOGE(TAG, "  Supported baud rates:");
  for (int i = 0; i < count; i++) {
    ESP_LOGE(TAG, "    - %" PRIu32, MEL_SUPPORTED_BAUD_RATES[i]);
  }
}

/*******************************************************************************
 * PUBLIC
 ******************************************************************************/

void MelAirConditioner::dump_config() {
  ESP_LOGCONFIG(TAG, "MelAirConditioner:");
  this->dump_traits_(TAG);

  // Validate UART settings
  auto baud_rate = this->parent_->get_baud_rate();
  validate_baud_rate(baud_rate);
  this->check_uart_settings(baud_rate, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
}

void MelAirConditioner::update() {
  const uint32_t now = millis();

  if (now < this->startup_delay_) {
    return;
  }

  this->conn_.tick();
  if (this->conn_.is_busy()) {
    return;
  }

  this->process_response();
  this->do_connect();
  this->do_poll();
}

void MelAirConditioner::set_supported_modes(const std::set<climate::ClimateMode> &modes) {
  this->traits_.set_supported_modes(modes);
  // Modes that are always available
  this->traits_.add_supported_mode(climate::CLIMATE_MODE_OFF);
  this->traits_.add_supported_mode(climate::CLIMATE_MODE_AUTO);
}

void MelAirConditioner::set_supported_fan_modes(const std::set<climate::ClimateFanMode> &modes) {
  this->traits_.set_supported_fan_modes(modes);
  // Modes that are always available
  this->traits_.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
  this->traits_.add_supported_fan_mode(climate::CLIMATE_FAN_LOW);
  this->traits_.add_supported_fan_mode(climate::CLIMATE_FAN_MEDIUM);
  this->traits_.add_supported_fan_mode(climate::CLIMATE_FAN_HIGH);
}

void MelAirConditioner::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes) {
  this->traits_.set_supported_swing_modes(modes);
  // Modes that are always available
  this->traits_.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
}

MelAirConditioner::MelAirConditioner() : conn_(this) {
  this->traits_ = climate::ClimateTraits();
  this->traits_.set_supports_action(true);
  this->traits_.set_supports_current_temperature(true);
};

}  // namespace ac
}  // namespace mel
}  // namespace esphome
