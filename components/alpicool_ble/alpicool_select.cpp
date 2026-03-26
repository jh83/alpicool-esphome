#include "alpicool_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace alpicool_ble {

static const char *const TAG = "alpicool_ble.select";

const char *const AlpicoolSelect::OPTION_LOW = "Low";
const char *const AlpicoolSelect::OPTION_MEDIUM = "Medium";
const char *const AlpicoolSelect::OPTION_HIGH = "High";

void AlpicoolSelect::dump_config() {
  LOG_SELECT("", "Alpicool Battery Saver", this);
}

void AlpicoolSelect::control(const std::string &value) {
  if (!this->hub_->get_status().valid) {
    ESP_LOGW(TAG, "Cannot set battery saver — status not yet received");
    return;
  }

  uint8_t bat_saver = 0;
  if (value == OPTION_LOW) bat_saver = 0;
  else if (value == OPTION_MEDIUM) bat_saver = 1;
  else if (value == OPTION_HIGH) bat_saver = 2;
  else {
    ESP_LOGW(TAG, "Unknown battery saver option: %s", value.c_str());
    return;
  }

  AlpicoolStatus desired = this->hub_->get_status();
  desired.bat_saver = bat_saver;
  this->hub_->send_set_command(desired);
  this->publish_state(value);
}

void AlpicoolSelect::on_alpicool_status(const AlpicoolStatus &s) {
  const char *option = OPTION_LOW;
  if (s.bat_saver == 1) option = OPTION_MEDIUM;
  else if (s.bat_saver == 2) option = OPTION_HIGH;
  this->publish_state(option);
}

}  // namespace alpicool_ble
}  // namespace esphome
