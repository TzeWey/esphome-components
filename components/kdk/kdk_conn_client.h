#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace kdk {

class KdkConnectionManager;

class KdkConnectionClient : public Parented<KdkConnectionManager> {
 public:
  virtual void on_parameter_update(void) = 0;

 protected:
  friend KdkConnectionManager;
  virtual std::string name() = 0;
};

}  // namespace kdk
}  // namespace esphome
