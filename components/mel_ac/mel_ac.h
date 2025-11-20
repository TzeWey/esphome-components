#pragma once

#include <map>
#include <set>

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"

#include "mel_conn.h"

namespace esphome {
namespace mel {
namespace ac {

static const uint32_t MEL_SUPPORTED_BAUD_RATES[] = {2400, 4800, 9600};

// Temperature Limits
static const uint8_t MEL_AC_TEMP_MIN = 16;  // Minimum Temperature in Celsius
static const uint8_t MEL_AC_TEMP_MAX = 31;  // Maximum Temperature in Celsius
static const uint8_t MEL_AC_TEMP_DEF = 25;  // Default Temperature in Celsius
static const float MEL_AC_TEMP_STEP = 0.5;  // Temperature step size in Celsius

static const uint32_t MEL_POLL_NONE = 0;
static const uint32_t MEL_POLL_GET_PARAMS = (1 << 0);
static const uint32_t MEL_POLL_GET_TEMP = (1 << 1);
static const uint32_t MEL_POLL_GET_STATUS = (1 << 2);

static const uint32_t MEL_POLL_ALL = MEL_POLL_GET_PARAMS | MEL_POLL_GET_TEMP | MEL_POLL_GET_STATUS;

static const int MEL_GET_PARAMS_OFFSET_POWER = 3;
static const int MEL_GET_PARAMS_OFFSET_MODE = 4;
static const int MEL_GET_PARAMS_OFFSET_TEMPERATURE_1 = 5;
static const int MEL_GET_PARAMS_OFFSET_FAN = 6;
static const int MEL_GET_PARAMS_OFFSET_VANE_VERT = 7;
static const int MEL_GET_PARAMS_OFFSET_VANE_HORZ = 10;
static const int MEL_GET_PARAMS_OFFSET_TEMPERATURE_2 = 11;

static const int MEL_SET_PARAMS_OFFSET_POWER = 3;
static const int MEL_SET_PARAMS_OFFSET_MODE = 4;
static const int MEL_SET_PARAMS_OFFSET_TEMPERATURE_1 = 5;
static const int MEL_SET_PARAMS_OFFSET_FAN = 6;
static const int MEL_SET_PARAMS_OFFSET_VANE_VERT = 7;
static const int MEL_SET_PARAMS_OFFSET_VANE_HORZ = 12;
static const int MEL_SET_PARAMS_OFFSET_TEMPERATURE_2 = 14;

template<typename T> using ParamMap = std::map<T, std::string>;

enum MelAcParamPower { MEL_AC_PARAM_POWER_OFF = 0, MEL_AC_PARAM_POWER_ON = 1 };
static const ParamMap<enum MelAcParamPower> MEL_AC_PARAM_POWER_STR_MAP = {{MEL_AC_PARAM_POWER_OFF, "OFF"},
                                                                          {MEL_AC_PARAM_POWER_ON, "ON"}};

enum MelAcParamISee { MEL_AC_PARAM_ISEE_OFF = 0, MEL_AC_PARAM_ISEE_ON = 1 };
static const ParamMap<enum MelAcParamISee> MEL_AC_PARAM_ISEE_STR_MAP = {{MEL_AC_PARAM_ISEE_OFF, "OFF"},
                                                                        {MEL_AC_PARAM_ISEE_ON, "ON"}};

enum MelAcParamMode {
  MEL_AC_PARAM_MODE_HEAT = 1,
  MEL_AC_PARAM_MODE_DRY = 2,
  MEL_AC_PARAM_MODE_COOL = 3,
  MEL_AC_PARAM_MODE_FAN = 7,
  MEL_AC_PARAM_MODE_AUTO = 8
};
static const ParamMap<enum MelAcParamMode> MEL_AC_PARAM_MODE_STR_MAP = {{MEL_AC_PARAM_MODE_HEAT, "HEAT"},
                                                                        {MEL_AC_PARAM_MODE_DRY, "DRY"},
                                                                        {MEL_AC_PARAM_MODE_COOL, "COOL"},
                                                                        {MEL_AC_PARAM_MODE_FAN, "FAN"},
                                                                        {MEL_AC_PARAM_MODE_AUTO, "AUTO"}};

enum MelAcParamFan {
  MEL_AC_PARAM_FAN_AUTO = 0,
  MEL_AC_PARAM_FAN_QUIET = 1,
  MEL_AC_PARAM_FAN_1 = 2,
  MEL_AC_PARAM_FAN_2 = 3,
  MEL_AC_PARAM_FAN_3 = 5,
  MEL_AC_PARAM_FAN_4 = 6
};
static const ParamMap<enum MelAcParamFan> MEL_AC_PARAM_FAN_STR_MAP = {
    {MEL_AC_PARAM_FAN_AUTO, "AUTO"}, {MEL_AC_PARAM_FAN_QUIET, "QUIET"}, {MEL_AC_PARAM_FAN_1, "1"},
    {MEL_AC_PARAM_FAN_2, "2"},       {MEL_AC_PARAM_FAN_3, "3"},         {MEL_AC_PARAM_FAN_4, "4"}};

enum MelAcParamVaneVert {
  MEL_AC_PARAM_VANE_VERT_AUTO = 0,
  MEL_AC_PARAM_VANE_VERT_1 = 1,
  MEL_AC_PARAM_VANE_VERT_2 = 2,
  MEL_AC_PARAM_VANE_VERT_3 = 3,
  MEL_AC_PARAM_VANE_VERT_4 = 4,
  MEL_AC_PARAM_VANE_VERT_5 = 5,
  MEL_AC_PARAM_VANE_VERT_SWING = 7
};
static const ParamMap<enum MelAcParamVaneVert> MEL_AC_PARAM_VANE_VERT_STR_MAP = {
    {MEL_AC_PARAM_VANE_VERT_AUTO, "AUTO"},  {MEL_AC_PARAM_VANE_VERT_1, "1"}, {MEL_AC_PARAM_VANE_VERT_2, "2"},
    {MEL_AC_PARAM_VANE_VERT_3, "3"},        {MEL_AC_PARAM_VANE_VERT_4, "4"}, {MEL_AC_PARAM_VANE_VERT_5, "5"},
    {MEL_AC_PARAM_VANE_VERT_SWING, "SWING"}};

enum MelAcParamVaneHorz {
  MEL_AC_PARAM_VANE_HORZ_AUTO = 0,
  MEL_AC_PARAM_VANE_HORZ_LEFT_MAX = 1,
  MEL_AC_PARAM_VANE_HORZ_LEFT = 2,
  MEL_AC_PARAM_VANE_HORZ_CENTER = 3,
  MEL_AC_PARAM_VANE_HORZ_RIGHT = 4,
  MEL_AC_PARAM_VANE_HORZ_RIGHT_MAX = 5,
  MEL_AC_PARAM_VANE_HORZ_SPLIT = 8,
  MEL_AC_PARAM_VANE_HORZ_SWING = 12
};
static const ParamMap<enum MelAcParamVaneHorz> MEL_AC_PARAM_VANE_HORZ_STR_MAP = {
    {MEL_AC_PARAM_VANE_HORZ_AUTO, "AUTO"},   {MEL_AC_PARAM_VANE_HORZ_LEFT_MAX, "LEFT_MAX"},
    {MEL_AC_PARAM_VANE_HORZ_LEFT, "LEFT"},   {MEL_AC_PARAM_VANE_HORZ_CENTER, "CENTER"},
    {MEL_AC_PARAM_VANE_HORZ_RIGHT, "RIGHT"}, {MEL_AC_PARAM_VANE_HORZ_RIGHT_MAX, "RIGHT_MAX"},
    {MEL_AC_PARAM_VANE_HORZ_SPLIT, "SPLIT"}, {MEL_AC_PARAM_VANE_HORZ_SWING, "SWING"}};

enum MelAcTemperatureMode {
  MEL_AC_TEMP_MODE_1,  // Temperature in Celsius, (VALUE + 10)
  MEL_AC_TEMP_MODE_2,  // Temperature in Celsius, (VALUE & 0x7F) / 2
};

class MelAcParams {
 protected:
  bool updated_ = true;

  MelAcParamPower power_ = MEL_AC_PARAM_POWER_OFF;
  MelAcParamISee isee_ = MEL_AC_PARAM_ISEE_OFF;  // iSee sensor status (read-only)
  MelAcParamMode mode_ = MEL_AC_PARAM_MODE_AUTO;
  MelAcParamFan fan_ = MEL_AC_PARAM_FAN_AUTO;
  MelAcParamVaneVert vane_vert_ = MEL_AC_PARAM_VANE_VERT_AUTO;  // vane: up-down
  MelAcParamVaneHorz vane_horz_ = MEL_AC_PARAM_VANE_HORZ_AUTO;  // vane: left-right
  bool vane_horz_flag_ = false;                                 // upper bit of vane_horz is set
  MelAcTemperatureMode temperature_mode_ = MEL_AC_TEMP_MODE_1;
  float temperature_target_ = (float) MEL_AC_TEMP_DEF;
  float temperature_current_ = (float) MEL_AC_TEMP_DEF;
  bool compressor_operating_ = false;
  uint8_t compressor_frequency_ = 0;

 protected:
  template<typename T> bool update(T &value_store, T &new_value, bool synced = true) {
    // If value has changed, set 'updated' flag if 'synced' is 'true' and update the value store
    if (value_store != new_value) {
      this->updated_ |= synced;
      value_store = new_value;
    }
    return true;
  }

  template<typename T> bool is_valid(const ParamMap<T> m, const uint8_t v) { return m.find((T) v) != m.end(); }

 public:
  bool is_updated(void) { return this->updated_; }
  void clear_updated(void) { this->updated_ = false; }

  // POWER
  MelAcParamPower get_power(void) { return this->power_; }
  std::string get_power_name(void) { return MEL_AC_PARAM_POWER_STR_MAP.find(this->power_)->second; }
  bool set_power(MelAcParamPower value) { return this->update(this->power_, value); }
  bool set_power(uint8_t value) {
    if (this->is_valid(MEL_AC_PARAM_POWER_STR_MAP, value))
      return this->set_power((MelAcParamPower) value);
    return false;
  }

  // ISEE
  MelAcParamISee get_isee(void) { return this->isee_; }
  std::string get_isee_name(void) { return MEL_AC_PARAM_ISEE_STR_MAP.find(this->isee_)->second; }
  bool set_isee(MelAcParamISee value) { return this->update(this->isee_, value, false); }
  bool set_isee(uint8_t value) {
    if (this->is_valid(MEL_AC_PARAM_ISEE_STR_MAP, value))
      return this->set_isee((MelAcParamISee) value);
    return false;
  }

  // MODE
  MelAcParamMode get_mode(void) { return this->mode_; }
  std::string get_mode_name(void) { return MEL_AC_PARAM_MODE_STR_MAP.find(this->mode_)->second; }
  bool set_mode(MelAcParamMode value) { return this->update(this->mode_, value); }
  bool set_mode(uint8_t value) {
    if (this->is_valid(MEL_AC_PARAM_MODE_STR_MAP, value))
      return this->set_mode((MelAcParamMode) value);
    return false;
  }

  // FAN
  MelAcParamFan get_fan(void) { return this->fan_; }
  std::string get_fan_name(void) { return MEL_AC_PARAM_FAN_STR_MAP.find(this->fan_)->second; }
  bool set_fan(MelAcParamFan value) { return this->update(this->fan_, value); }
  bool set_fan(uint8_t value) {
    if (this->is_valid(MEL_AC_PARAM_FAN_STR_MAP, value))
      return this->set_fan((MelAcParamFan) value);
    return false;
  }

  // VANE VERT
  MelAcParamVaneVert get_vane_vert(void) { return this->vane_vert_; }
  std::string get_vane_vert_name(void) { return MEL_AC_PARAM_VANE_VERT_STR_MAP.find(this->vane_vert_)->second; }
  bool set_vane_vert(MelAcParamVaneVert value) { return this->update(this->vane_vert_, value); }
  bool set_vane_vert(uint8_t value) {
    if (this->is_valid(MEL_AC_PARAM_VANE_VERT_STR_MAP, value))
      return this->set_vane_vert((MelAcParamVaneVert) value);
    return false;
  }

  // VANE HORZ
  MelAcParamVaneHorz get_vane_horz(void) { return this->vane_horz_; }
  std::string get_vane_horz_name(void) { return MEL_AC_PARAM_VANE_HORZ_STR_MAP.find(this->vane_horz_)->second; }
  bool set_vane_horz(MelAcParamVaneHorz value) { return this->update(this->vane_horz_, value); }
  bool set_vane_horz(uint8_t value) {
    if (this->is_valid(MEL_AC_PARAM_VANE_HORZ_STR_MAP, value))
      return this->set_vane_horz((MelAcParamVaneHorz) value);
    return false;
  }

  // VANE HORZ FLAG
  bool get_vane_horz_flag(void) { return this->vane_horz_flag_; }
  void set_vane_horz_flag(bool value) { this->vane_horz_flag_ = value; }

  // TEMPERATURE MODE
  MelAcTemperatureMode get_temperature_mode(void) { return this->temperature_mode_; }
  void set_temperature_mode(MelAcTemperatureMode value) { this->update(this->temperature_mode_, value, false); }

  // TEMPERATURE TARGET
  float get_temperature_target(void) { return this->temperature_target_; }
  void set_temperature_target(float value) { this->update(this->temperature_target_, value); }

  // TEMPERATURE CURRENT
  float get_temperature_current(void) { return this->temperature_current_; }
  void set_temperature_current(float value) { this->update(this->temperature_current_, value); }

  // COMPRESSOR OPERATING
  bool get_compressor_operating(void) { return this->compressor_operating_; }
  void set_compressor_operating(bool value) { this->update(this->compressor_operating_, value); }

  // COMPRESSOR FREQUENCY
  uint8_t get_compressor_frequency(void) { return this->compressor_frequency_; }
  void set_compressor_frequency(uint8_t value) { this->update(this->compressor_frequency_, value, false); }

  MelAcParams(){};
};

class MelAcSetParams {
 protected:
  bool pending_;

  optional<MelAcParamPower> power_;
  optional<MelAcParamMode> mode_;
  optional<float> temperature_;
  optional<MelAcParamFan> fan_;
  optional<MelAcParamVaneVert> vane_vert_;
  optional<MelAcParamVaneHorz> vane_horz_;

 public:
  const optional<MelAcParamPower> &get_power() const { return this->power_; }
  std::string get_power_name(void) { return MEL_AC_PARAM_POWER_STR_MAP.find(*this->power_)->second; }
  MelAcSetParams &set_power(MelAcParamPower value) {
    this->power_ = value;
    return *this;
  }

  const optional<MelAcParamMode> &get_mode() const { return this->mode_; }
  std::string get_mode_name(void) { return MEL_AC_PARAM_MODE_STR_MAP.find(*this->mode_)->second; }
  MelAcSetParams &set_mode(MelAcParamMode value) {
    this->mode_ = value;
    return *this;
  }

  const optional<float> &get_temperature() const { return this->temperature_; }
  MelAcSetParams &set_temperature(float value) {
    this->temperature_ = value;
    return *this;
  }

  const optional<MelAcParamFan> &get_fan() const { return this->fan_; }
  std::string get_fan_name(void) { return MEL_AC_PARAM_FAN_STR_MAP.find(*this->fan_)->second; }
  MelAcSetParams &set_fan(MelAcParamFan value) {
    this->fan_ = value;
    return *this;
  }

  const optional<MelAcParamVaneVert> &get_vane_vert() const { return this->vane_vert_; }
  std::string get_vane_vert_name(void) { return MEL_AC_PARAM_VANE_VERT_STR_MAP.find(*this->vane_vert_)->second; }
  MelAcSetParams &set_vane_vert(MelAcParamVaneVert value) {
    this->vane_vert_ = value;
    return *this;
  }

  const optional<MelAcParamVaneHorz> &get_vane_horz() const { return this->vane_horz_; }
  std::string get_vane_horz_name(void) { return MEL_AC_PARAM_VANE_HORZ_STR_MAP.find(*this->vane_horz_)->second; }
  MelAcSetParams &set_vane_horz(MelAcParamVaneHorz value) {
    this->vane_horz_ = value;
    return *this;
  }

  void reset(void) {
    this->power_.reset();
    this->mode_.reset();
    this->temperature_.reset();
    this->fan_.reset();
    this->vane_vert_.reset();
    this->vane_horz_.reset();
  }

  bool is_pending(void) { return this->pending_; }
  void set_pending(bool value) { this->pending_ = value; }

  MelAcSetParams(){};
};

class MelAirConditioner : public PollingComponent, public uart::UARTDevice, public climate::Climate {
 protected:
  uint32_t startup_delay_ = 0;

  climate::ClimateTraits traits_;

  conn::MelConnectionManager conn_;

  bool ac_connected_ = false;

  uint32_t ac_poll_flag_ = MEL_POLL_ALL;
  uint32_t ac_poll_timestamp_ = 0;
  uint32_t ac_poll_refresh_rate_ = 2000;

  MelAcParams ac_params_;
  MelAcSetParams ac_set_params_;

  MelAcParams &params() { return this->ac_params_; }
  MelAcSetParams &set_params() { return this->ac_set_params_; }

  void set_connected(bool value) { this->ac_connected_ = value; }
  bool get_connected(void) { return this->ac_connected_; }

  void restart_poll(bool force = false) {
    const uint32_t now = millis();
    if (force || (now - this->ac_poll_timestamp_) > this->ac_poll_refresh_rate_) {
      this->set_poll_flag(MEL_POLL_ALL);
      this->ac_poll_timestamp_ = now;
    }
  }

  void set_poll_flag(uint32_t flag) { this->ac_poll_flag_ |= flag; }
  void clear_poll_flag(uint32_t flag) { this->ac_poll_flag_ &= ~flag; }
  bool check_poll_flag(uint32_t flag) { return (this->ac_poll_flag_ & flag) != 0; }
  bool check_poll_idle(void) { return (this->ac_poll_flag_ == MEL_POLL_NONE); }

  void request_connect(void);
  void request_params(void);
  void request_room_temperature(void);
  void request_status(void);

  void process_response(void);

  void do_publish(void);
  bool do_control(void);

  void do_connect(void);
  void do_poll(void);

  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override { return this->traits_; }

 public:
  void dump_config() override;
  void update() override;

  void set_poll_refresh_rate(uint32_t value) { this->ac_poll_refresh_rate_ = value; }
  void set_startup_delay(uint32_t value) { this->startup_delay_ = value; }

  void set_supported_modes(climate::ClimateModeMask modes);
  void set_supported_fan_modes(climate::ClimateFanModeMask fan_modes);
  void set_supported_swing_modes(climate::ClimateSwingModeMask swing_modes);

  MelAirConditioner();
};

}  // namespace ac
}  // namespace mel
}  // namespace esphome
