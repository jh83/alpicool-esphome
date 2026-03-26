#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "alpicool_ble.h"

namespace esphome {
namespace alpicool_ble {

enum class AlpicoolSensorType : uint8_t {
  BATTERY_PERCENT,
  BATTERY_VOLTAGE,
  LEFT_CURRENT_TEMPERATURE,
  RIGHT_CURRENT_TEMPERATURE,
};

class AlpicoolSensor : public sensor::Sensor, public Component, public AlpicoolListener {
 public:
  void set_hub(AlpicoolBLEClient *hub) { this->hub_ = hub; }
  void set_type(AlpicoolSensorType type) { this->type_ = type; }

  void setup() override {}
  void dump_config() override;

  void on_alpicool_status(const AlpicoolStatus &status) override;
  void on_alpicool_connected(bool connected) override;

 protected:
  AlpicoolBLEClient *hub_{nullptr};
  AlpicoolSensorType type_{AlpicoolSensorType::BATTERY_PERCENT};
};

}  // namespace alpicool_ble
}  // namespace esphome
