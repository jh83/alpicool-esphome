#include "alpicool_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace alpicool_ble {

static const char *const TAG = "alpicool_ble.number";

void AlpicoolNumber::dump_config() {
  LOG_NUMBER("", "Alpicool Number", this);
}

void AlpicoolNumber::control(float value) {
  if (!this->hub_->get_status().valid) {
    ESP_LOGW(TAG, "Cannot set value — status not yet received");
    return;
  }
  AlpicoolStatus desired = this->hub_->get_status();
  switch (this->type_) {
    case AlpicoolNumberType::HYSTERESIS:
      desired.left_ret_diff = (int8_t)value;
      break;
    case AlpicoolNumberType::START_DELAY:
      desired.start_delay = (uint8_t)value;
      break;
    case AlpicoolNumberType::TEMP_MAX:
      if (this->zone_ == 0)
        desired.temp_max = (int8_t)value;
      else
        desired.right_temp_max = (int8_t)value;
      break;
    case AlpicoolNumberType::TEMP_MIN:
      if (this->zone_ == 0)
        desired.temp_min = (int8_t)value;
      else
        desired.right_temp_min = (int8_t)value;
      break;
  }
  this->hub_->send_set_command(desired);
  this->publish_state(value);
}

void AlpicoolNumber::on_alpicool_status(const AlpicoolStatus &s) {
  switch (this->type_) {
    case AlpicoolNumberType::HYSTERESIS:
      this->publish_state((float)s.left_ret_diff);
      break;
    case AlpicoolNumberType::START_DELAY:
      this->publish_state((float)s.start_delay);
      break;
    case AlpicoolNumberType::TEMP_MAX:
      this->publish_state((float)(this->zone_ == 0 ? s.temp_max : s.right_temp_max));
      break;
    case AlpicoolNumberType::TEMP_MIN:
      this->publish_state((float)(this->zone_ == 0 ? s.temp_min : s.right_temp_min));
      break;
  }
}

}  // namespace alpicool_ble
}  // namespace esphome
