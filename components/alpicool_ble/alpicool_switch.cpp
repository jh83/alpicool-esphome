#include "alpicool_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace alpicool_ble {

static const char *const TAG = "alpicool_ble.switch";

void AlpicoolSwitch::dump_config() {
  LOG_SWITCH("", "Alpicool Panel Lock", this);
}

void AlpicoolSwitch::write_state(bool state) {
  if (!this->hub_->get_status().valid) {
    ESP_LOGW(TAG, "Cannot set panel lock — status not yet received");
    return;
  }
  AlpicoolStatus desired = this->hub_->get_status();
  desired.locked = state;
  this->hub_->send_set_command(desired);
  // Optimistic update
  this->publish_state(state);
}

void AlpicoolSwitch::on_alpicool_status(const AlpicoolStatus &s) {
  this->publish_state(s.locked);
}

void AlpicoolSwitch::on_alpicool_connected(bool connected) {
  // Nothing to do on disconnect — state becomes stale but switch doesn't
  // have a "unavailable" publish mechanism; it will update on reconnect.
}

}  // namespace alpicool_ble
}  // namespace esphome
