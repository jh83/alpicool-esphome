#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "alpicool_ble.h"

namespace esphome {
namespace alpicool_ble {

class AlpicoolSwitch : public switch_::Switch, public Component, public AlpicoolListener {
 public:
  void set_hub(AlpicoolBLEClient *hub) { this->hub_ = hub; }

  void setup() override {}
  void dump_config() override;

  // Switch override
  void write_state(bool state) override;

  // AlpicoolListener overrides
  void on_alpicool_status(const AlpicoolStatus &status) override;
  void on_alpicool_connected(bool connected) override;

 protected:
  AlpicoolBLEClient *hub_{nullptr};
};

}  // namespace alpicool_ble
}  // namespace esphome
