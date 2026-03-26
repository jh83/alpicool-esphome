#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "alpicool_ble.h"

namespace esphome {
namespace alpicool_ble {

class AlpicoolClimate : public climate::Climate, public Component, public AlpicoolListener {
 public:
  void set_hub(AlpicoolBLEClient *hub) { this->hub_ = hub; }
  void set_zone(uint8_t zone) { this->zone_ = zone; }  // 0=left, 1=right

  void setup() override;
  void dump_config() override;

  // Climate overrides
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

  // AlpicoolListener overrides
  void on_alpicool_status(const AlpicoolStatus &status) override;
  void on_alpicool_connected(bool connected) override;

 protected:
  AlpicoolBLEClient *hub_{nullptr};
  uint8_t zone_{0};
};

}  // namespace alpicool_ble
}  // namespace esphome
