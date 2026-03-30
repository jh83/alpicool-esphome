#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "alpicool_ble.h"

namespace esphome {
namespace alpicool_ble {

enum class AlpicoolNumberType : uint8_t {
  HYSTERESIS,    // left_ret_diff: 1-10 °C
  START_DELAY,   // start_delay: 0-10 min
  TEMP_MAX,      // temp_max: maximum selectable temperature
  TEMP_MIN,      // temp_min: minimum selectable temperature
};

class AlpicoolNumber : public number::Number, public Component, public AlpicoolListener {
 public:
  void set_hub(AlpicoolBLEClient *hub) { this->hub_ = hub; }
  void set_type(AlpicoolNumberType type) { this->type_ = type; }
  void set_zone(uint8_t zone) { this->zone_ = zone; }  // 0=left, 1=right

  void setup() override {}
  void dump_config() override;

  // Number override
  void control(float value) override;

  // AlpicoolListener overrides
  void on_alpicool_status(const AlpicoolStatus &status) override;
  void on_alpicool_connected(bool connected) override {}

 protected:
  AlpicoolBLEClient *hub_{nullptr};
  AlpicoolNumberType type_{AlpicoolNumberType::HYSTERESIS};
  uint8_t zone_{0};
};

}  // namespace alpicool_ble
}  // namespace esphome
