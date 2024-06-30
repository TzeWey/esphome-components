#include "esphome/core/log.h"

#include "../kdk_conn.h"

#include "kdk_fan.h"

namespace esphome {
namespace kdk {

static const char *const TAG = "kdk.fan";

static const int KDK_FAN_SPEED_STEP_SIZE = 10;
static const int KDK_FAN_SPEED_COUNT = 10;

static const uint8_t KDK_FAN_STATE_ON = 0x30;
static const uint8_t KDK_FAN_STATE_OFF = 0x31;

static const uint8_t KDK_FAN_YURAGI_ON = 0x30;
static const uint8_t KDK_FAN_YURAGI_OFF = 0x31;

static const uint8_t KDK_FAN_DIRECTION_NORMAL = 0x41;
static const uint8_t KDK_FAN_DIRECTION_REVERSE = 0x42;

static const uint16_t KDK_PARAM_FAN_STATE = 0x8000;      // Fan ON/OFF
static const uint16_t KDK_PARAM_FAN_SPEED = 0xF000;      // Fan Speed
static const uint16_t KDK_PARAM_FAN_DIRECTION = 0xF100;  // Fan Direction
static const uint16_t KDK_PARAM_FAN_YURAGI = 0xF200;     // Fan Yuragi

bool KdkFan::is_valid(const std::vector<uint8_t> &data) const {
  if (data.size() == 0) {
    return false;
  }
  return true;
}

bool KdkFan::to_state(const uint8_t b) const { return (b == KDK_FAN_STATE_ON); }
uint8_t KdkFan::from_state(bool v) const { return v ? KDK_FAN_STATE_ON : KDK_FAN_STATE_OFF; }

int KdkFan::to_speed(const uint8_t b) const { return (b - 0x30); }
uint8_t KdkFan::from_speed(int v) const { return (v + 0x30) & 0xFF; }

fan::FanDirection KdkFan::to_direction(const uint8_t b) const {
  return (b == KDK_FAN_DIRECTION_REVERSE) ? fan::FanDirection::REVERSE : fan::FanDirection::FORWARD;
}
uint8_t KdkFan::from_direction(fan::FanDirection v) const {
  return (v == fan::FanDirection::REVERSE) ? KDK_FAN_DIRECTION_REVERSE : KDK_FAN_DIRECTION_NORMAL;
}

bool KdkFan::is_state_changed(uint8_t state, uint8_t speed, uint8_t direction) {
  if (this->fan_state_ != state) {
    return true;
  }

  if (this->fan_speed_ != speed) {
    return true;
  }

  if (this->fan_direction_ != direction) {
    return true;
  }

  return false;
}

fan::FanTraits KdkFan::get_traits() { return fan::FanTraits(false, true, true, KDK_FAN_SPEED_COUNT); }

void KdkFan::control(const fan::FanCall &call) {
  const auto conn = this->get_parent();

  if (!conn->is_ready()) {
    ESP_LOGW(TAG, "Connection not ready!");
    return;
  }

  // Push parameter update
  std::vector<struct KdkParamUpdate> parameters;

  if (call.get_state().has_value()) {
    this->state = *call.get_state();
  }
  auto fan_state = this->from_state(this->state);
  ESP_LOGD(TAG, "  State: %s [0x%02X]", ONOFF(state), fan_state);
  parameters.push_back({.id = KDK_PARAM_FAN_STATE, .data = {fan_state}});

  if (call.get_speed().has_value()) {
    this->speed = *call.get_speed();
  }
  auto fan_speed = this->from_speed(speed);
  ESP_LOGD(TAG, "  Speed: %d [0x%02X]", speed, fan_speed);
  parameters.push_back({.id = KDK_PARAM_FAN_SPEED, .data = {fan_speed}});

  if (call.get_direction().has_value()) {
    this->direction = *call.get_direction();
  }
  auto fan_direction = this->from_direction(direction);
  ESP_LOGD(TAG, "  Direction: %s [0x%02X]", LOG_STR_ARG(fan_direction_to_string(direction)), fan_direction);
  parameters.push_back({.id = KDK_PARAM_FAN_DIRECTION, .data = {fan_direction}});

  // Always turn OFF Yuragi
  parameters.push_back({.id = KDK_PARAM_FAN_YURAGI, .data = {KDK_FAN_YURAGI_OFF}});

  conn->update_parameter_data(parameters);
}

void KdkFan::on_parameter_update(void) {
  const auto conn = this->get_parent();

  // Get parameter values
  const auto v_state = conn->get_parameter_data(KDK_PARAM_FAN_STATE);
  const auto v_speed = conn->get_parameter_data(KDK_PARAM_FAN_SPEED);
  const auto v_direction = conn->get_parameter_data(KDK_PARAM_FAN_DIRECTION);

  // Check whether they are valid
  if (!this->is_valid(v_state) ||  //
      !this->is_valid(v_speed) ||  //
      !this->is_valid(v_direction)) {
    return;
  }

  // They should all be 1-byte in size
  auto fan_state = v_state[0];
  auto fan_speed = v_speed[0];
  auto fan_direction = v_direction[0];

  if (!this->is_state_changed(fan_state, fan_speed, fan_direction)) {
    return;  // no change
  }

  // Update internal cache
  this->fan_state_ = fan_state;
  this->fan_speed_ = fan_speed;
  this->fan_direction_ = fan_direction;

  // Publish states
  this->state = this->to_state(fan_state);
  this->speed = this->to_speed(fan_speed);
  this->direction = this->to_direction(fan_direction);
  this->publish_state();
}

}  // namespace kdk
}  // namespace esphome
