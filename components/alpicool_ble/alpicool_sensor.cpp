#include "alpicool_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace alpicool_ble {

static const char *const TAG = "alpicool_ble.sensor";

void AlpicoolSensor::dump_config() {
  LOG_SENSOR("", "Alpicool Sensor", this);
}

void AlpicoolSensor::on_alpicool_status(const AlpicoolStatus &s) {
  switch (this->type_) {
    case AlpicoolSensorType::BATTERY_PERCENT:
      this->publish_state((float)s.bat_percent);
      break;

    case AlpicoolSensorType::BATTERY_VOLTAGE: {
      // Combine integer and decimal parts: e.g., int=12, dec=5 → 12.5 V
      float voltage = (float)s.bat_vol_int + (float)s.bat_vol_dec / 10.0f;
      this->publish_state(voltage);
      break;
    }

    case AlpicoolSensorType::LEFT_CURRENT_TEMPERATURE:
      this->publish_state((float)s.left_current);
      break;

    case AlpicoolSensorType::RIGHT_CURRENT_TEMPERATURE:
      if (s.dual_zone)
        this->publish_state((float)s.right_current);
      break;
  }
}

void AlpicoolSensor::on_alpicool_connected(bool connected) {
  if (!connected)
    this->publish_state(NAN);
}

}  // namespace alpicool_ble
}  // namespace esphome
