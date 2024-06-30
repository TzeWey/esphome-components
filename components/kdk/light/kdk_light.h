#pragma once

#include "esphome/components/light/light_output.h"

#include "../kdk_conn_client.h"

namespace esphome {
namespace kdk {

class KdkLight : public light::LightOutput, public KdkConnectionClient {
 protected:
  light::LightState *state_{nullptr};

  float cold_white_temperature_{0};
  float warm_white_temperature_{0};

  uint32_t last_update_timestamp_{0};
  bool skip_parameter_update_{false};

  uint8_t light_state_{0};
  uint8_t light_mode_{0};
  uint8_t light_brightness_{0};
  uint8_t light_color_{0};

  std::string name() override { return "KDK Light"; }

  bool to_state(const uint8_t light_state, const uint8_t light_mode) const;
  uint8_t from_state(const bool v) const;

  float to_brightness(const uint8_t b) const;
  uint8_t from_brightness(const float v) const;

  float to_color(const uint8_t b) const;
  uint8_t from_color(const float v) const;

  bool is_valid(const std::vector<uint8_t> &data) const;
  bool is_state_changed(uint8_t mode, uint8_t state, uint8_t brightness, uint8_t color);

 public:
  void set_cold_white_temperature(float cold_white_temperature) { cold_white_temperature_ = cold_white_temperature; }
  void set_warm_white_temperature(float warm_white_temperature) { warm_white_temperature_ = warm_white_temperature; }

  light::LightTraits get_traits() override;

  void setup_state(light::LightState *state) override { this->state_ = state; }

  void update_state(light::LightState *state) override;

  void write_state(light::LightState *state) override {}

  void on_parameter_update(void) override;
};

}  // namespace kdk
}  // namespace esphome
