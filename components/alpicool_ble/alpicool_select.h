#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "alpicool_ble.h"

namespace esphome {
namespace alpicool_ble {

class AlpicoolSelect : public select::Select, public Component, public AlpicoolListener {
 public:
  void set_hub(AlpicoolBLEClient *hub) { this->hub_ = hub; }

  void setup() override {}
  void dump_config() override;

  // Select override
  void control(const std::string &value) override;

  // AlpicoolListener overrides
  void on_alpicool_status(const AlpicoolStatus &status) override;
  void on_alpicool_connected(bool connected) override {}

 protected:
  AlpicoolBLEClient *hub_{nullptr};

  // Options in order: index maps to bat_saver value (0=Low, 1=Medium, 2=High)
  static const char *const OPTION_LOW;
  static const char *const OPTION_MEDIUM;
  static const char *const OPTION_HIGH;
};

}  // namespace alpicool_ble
}  // namespace esphome
