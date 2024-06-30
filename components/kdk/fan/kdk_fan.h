#pragma once

#include "esphome/components/fan/fan.h"

#include "../kdk_conn_client.h"

namespace esphome {
namespace kdk {

class KdkFan : public fan::Fan, public KdkConnectionClient {
 protected:
  std::string name() override { return "KDK Fan"; }

  uint8_t fan_state_{0};
  uint8_t fan_speed_{0};
  uint8_t fan_direction_{0};

  
  bool to_state(const uint8_t b) const;
  uint8_t from_state(bool v) const;

  int to_speed(const uint8_t b) const;
  uint8_t from_speed(int v) const;

  fan::FanDirection to_direction(const uint8_t b) const;
  uint8_t from_direction(fan::FanDirection v) const;

  bool is_valid(const std::vector<uint8_t> &data) const;
  bool is_state_changed(uint8_t state, uint8_t speed, uint8_t direction);

 public:
  fan::FanTraits get_traits() override;

  void control(const fan::FanCall &call) override;

  void on_parameter_update(void) override;
};

}  // namespace kdk
}  // namespace esphome
