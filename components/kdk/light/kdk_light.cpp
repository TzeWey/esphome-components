#include "esphome/core/log.h"

#include "../kdk_conn.h"

#include "kdk_light.h"

namespace esphome {
namespace kdk {

static const char *const TAG = "kdk.light";

static const uint32_t KDK_LIGHT_UPDATE_BLANKING_TIME = 1500;

static const uint8_t KDK_LIGHT_STATE_ON = 0x30;
static const uint8_t KDK_LIGHT_STATE_OFF = 0x31;

static const uint8_t KDK_LIGHT_MODE_NORMAL = 0x42;
static const uint8_t KDK_LIGHT_MODE_NIGHTLIGHT = 0x43;

static const uint16_t KDK_PARAM_LIGHT_STATE = 0xF300;            // Light ON/OFF
static const uint16_t KDK_PARAM_LIGHT_MODE = 0xF400;             // Light Mode
static const uint16_t KDK_PARAM_LIGHT_BRIGHTNESS = 0xF500;       // Light Brightness
static const uint16_t KDK_PARAM_LIGHT_COLOR = 0xF600;            // Light Color
static const uint16_t KDK_PARAM_NIGHTLIGHT_BRIGHTNESS = 0xF700;  // NightLight Brightness

bool KdkLight::to_state(const uint8_t light_state, const uint8_t light_mode) const {
  // Assume Light is OFF when NightLight is enabled
  return (light_mode == KDK_LIGHT_MODE_NORMAL) && (light_state == KDK_LIGHT_STATE_ON);
}
uint8_t KdkLight::from_state(bool v) const { return v ? KDK_LIGHT_STATE_ON : KDK_LIGHT_STATE_OFF; }

float KdkLight::to_brightness(const uint8_t b) const { return ((float) b / 100); }
uint8_t KdkLight::from_brightness(const float v) const {
  return clamp((uint8_t) round(v * 100), (uint8_t) 1, (uint8_t) 100);
}

float KdkLight::to_color(const uint8_t b) const {
  return this->cold_white_temperature_ +
         (this->warm_white_temperature_ - this->cold_white_temperature_) * ((float) (99 - (b - 1)) / 99);
}
uint8_t KdkLight::from_color(const float v) const {
  return clamp((uint8_t) round(((1.0f - v) * 99) + 1), (uint8_t) 1, (uint8_t) 100);
}

bool KdkLight::is_valid(const std::vector<uint8_t> &data) const {
  if (data.size() == 0) {
    return false;
  }
  return true;
}

bool KdkLight::is_state_changed(uint8_t mode, uint8_t state, uint8_t brightness, uint8_t color) {
  if (this->light_mode_ != mode) {
    return true;
  }

  if (this->light_state_ != state) {
    return true;
  }

  // Ignore brightness and color if state if OFF
  if (state == KDK_LIGHT_STATE_OFF) {
    return false;
  }

  if (this->light_brightness_ != brightness) {
    return true;
  }

  if (this->light_color_ != color) {
    return true;
  }

  return false;
}

light::LightTraits KdkLight::get_traits() {
  light::LightTraits traits{};

  traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});

  traits.set_min_mireds(this->cold_white_temperature_);
  traits.set_max_mireds(this->warm_white_temperature_);

  return traits;
}

void KdkLight::update_state(light::LightState *state) {
  const auto conn = this->get_parent();

  if (!conn->is_ready()) {
    ESP_LOGW(TAG, "Connection not ready!");
    return;
  }

  if (this->skip_parameter_update_) {
    this->skip_parameter_update_ = false;
    return;
  }

  // Convert current states to parameter values
  auto color_temperature = 0.0f;
  auto white_brightness = 0.0f;
  auto is_on = state->current_values.is_on();
  state->current_values_as_ct(&color_temperature, &white_brightness);

  auto light_mode = KDK_LIGHT_MODE_NORMAL;
  auto light_state = is_on ? KDK_LIGHT_STATE_ON : KDK_LIGHT_STATE_OFF;
  auto light_brightness = this->from_brightness(white_brightness);
  auto light_color = this->from_color(color_temperature);

  // Push parameter update
  std::vector<struct KdkParamUpdate> parameters;
  parameters.push_back({.id = KDK_PARAM_LIGHT_STATE, .data = {light_state}});
  parameters.push_back({.id = KDK_PARAM_LIGHT_MODE, .data = {light_mode}});
  parameters.push_back({.id = KDK_PARAM_LIGHT_BRIGHTNESS, .data = {light_brightness}});
  parameters.push_back({.id = KDK_PARAM_LIGHT_COLOR, .data = {light_color}});
  conn->update_parameter_data(parameters);

  // Set last update timestamp to prevent slider jitter
  this->last_update_timestamp_ = millis();
}

void KdkLight::on_parameter_update(void) {
  const auto &conn = this->get_parent();

  // Get parameter values
  const auto v_mode = conn->get_parameter_data(KDK_PARAM_LIGHT_MODE);
  const auto v_state = conn->get_parameter_data(KDK_PARAM_LIGHT_STATE);
  const auto v_brightness = conn->get_parameter_data(KDK_PARAM_LIGHT_BRIGHTNESS);
  const auto v_color = conn->get_parameter_data(KDK_PARAM_LIGHT_COLOR);

  // Check whether they are valid
  if (!this->is_valid(v_mode) ||        //
      !this->is_valid(v_state) ||       //
      !this->is_valid(v_brightness) ||  //
      !this->is_valid(v_color)) {
    return;
  }

  // They should all be 1-byte in size
  auto light_mode = v_mode[0];
  auto light_state = v_state[0];
  auto light_brightness = v_brightness[0];
  auto light_color = v_color[0];

  if (!this->is_state_changed(light_mode, light_state, light_brightness, light_color)) {
    return;  // no change
  }

  // Update internal cache
  this->light_mode_ = light_mode;
  this->light_state_ = light_state;
  this->light_brightness_ = light_brightness;
  this->light_color_ = light_color;

  auto call = this->state_->make_call();

  call.set_state(this->to_state(light_state, light_mode));

  // Suppress if last update is too recent
  if ((millis() - this->last_update_timestamp_) > KDK_LIGHT_UPDATE_BLANKING_TIME) {
    call.set_brightness(this->to_brightness(light_brightness));
    call.set_color_temperature(this->to_color(light_color));
  }

  // Since the most recent state was pulled, skip parameter update in `update_state`
  this->skip_parameter_update_ = true;

  // Publish and update states, this will in turn call 'update_state'
  call.perform();
}

}  // namespace kdk
}  // namespace esphome
