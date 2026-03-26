#include "alpicool_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace alpicool_ble {

static const char *const TAG = "alpicool_ble.climate";

void AlpicoolClimate::setup() {}

void AlpicoolClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Alpicool Climate:");
  ESP_LOGCONFIG(TAG, "  Zone: %s", this->zone_ == 0 ? "left" : "right");
}

climate::ClimateTraits AlpicoolClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
  });
  traits.set_supported_presets({
      climate::CLIMATE_PRESET_BOOST,  // Max
      climate::CLIMATE_PRESET_ECO,    // Eco
  });
  traits.set_visual_min_temperature(-20.0f);
  traits.set_visual_max_temperature(20.0f);
  traits.set_visual_temperature_step(1.0f);
  return traits;
}

void AlpicoolClimate::control(const climate::ClimateCall &call) {
  if (!this->hub_->get_status().valid) {
    ESP_LOGW(TAG, "Cannot control — status not yet received");
    return;
  }

  // For temperature changes, use the quick SET_LEFT/SET_RIGHT command.
  // For everything else (mode, preset), use the full SET command.
  bool needs_full_set = false;
  AlpicoolStatus desired = this->hub_->get_status();

  if (call.get_target_temperature().has_value()) {
    int8_t temp = (int8_t)(*call.get_target_temperature());
    if (this->zone_ == 0) {
      this->hub_->send_set_left_command(temp);
    } else {
      this->hub_->send_set_right_command(temp);
    }
    // Update local state optimistically
    if (this->zone_ == 0)
      desired.left_target = temp;
    else
      desired.right_target = temp;
    this->target_temperature = *call.get_target_temperature();
  }

  if (call.get_mode().has_value()) {
    desired.powered_on = (*call.get_mode() != climate::CLIMATE_MODE_OFF);
    needs_full_set = true;
  }

  if (call.get_preset().has_value()) {
    desired.run_mode = (*call.get_preset() == climate::CLIMATE_PRESET_ECO) ? 1 : 0;
    needs_full_set = true;
  }

  if (needs_full_set) {
    this->hub_->send_set_command(desired);
  }

  this->publish_state();
}

void AlpicoolClimate::on_alpicool_status(const AlpicoolStatus &s) {
  // Only publish right-zone data if this device is actually dual-zone
  if (this->zone_ == 1 && !s.dual_zone) return;

  this->current_temperature =
      (this->zone_ == 0) ? (float)s.left_current : (float)s.right_current;

  this->target_temperature =
      (this->zone_ == 0) ? (float)s.left_target : (float)s.right_target;

  this->mode = s.powered_on ? climate::CLIMATE_MODE_COOL : climate::CLIMATE_MODE_OFF;

  this->preset = (s.run_mode == 1) ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_BOOST;

  this->publish_state();
}

void AlpicoolClimate::on_alpicool_connected(bool connected) {
  if (!connected) {
    // Mark unavailable by clearing current temperature
    this->current_temperature = NAN;
    this->publish_state();
  }
}

}  // namespace alpicool_ble
}  // namespace esphome
