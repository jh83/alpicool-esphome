#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/ble_client/ble_client.h"

#include <vector>
#include <string>

namespace esphome {
namespace alpicool_ble {

struct AlpicoolStatus {
  bool valid{false};
  bool locked{false};
  bool powered_on{true};
  uint8_t run_mode{0};   // 0=Max, 1=Eco
  uint8_t bat_saver{0};  // 0=Low, 1=Med, 2=High
  int8_t left_target{0};
  int8_t temp_max{20};
  int8_t temp_min{-20};
  int8_t left_ret_diff{1};
  uint8_t start_delay{0};
  uint8_t unit{0};  // 0=C, 1=F
  int8_t left_tc_hot{0};
  int8_t left_tc_mid{0};
  int8_t left_tc_cold{0};
  int8_t left_tc_halt{0};
  int8_t left_current{0};
  uint8_t bat_percent{0};
  uint8_t bat_vol_int{0};
  uint8_t bat_vol_dec{0};
  // Dual-zone fields (valid only when dual_zone == true)
  bool dual_zone{false};
  int8_t right_target{0};
  int8_t right_temp_max{20};
  int8_t right_temp_min{-20};
  int8_t right_ret_diff{1};
  int8_t right_tc_hot{0};
  int8_t right_tc_mid{0};
  int8_t right_tc_cold{0};
  int8_t right_tc_halt{0};
  int8_t right_current{0};
  uint8_t running_status{0};
};

class AlpicoolListener {
 public:
  virtual void on_alpicool_status(const AlpicoolStatus &status) = 0;
  virtual void on_alpicool_connected(bool connected) = 0;
};

enum class AlpicoolState : uint8_t {
  IDLE,
  DISCOVERING,
  SUBSCRIBING,
  BINDING,
  READY,
};

class AlpicoolBLEClient : public PollingComponent, public ble_client::BLEClientNode {
 public:
  // ESPHome component lifecycle
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // BLEClientNode overrides
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                            esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {}

  // Registration (called from Python codegen)
  void register_listener(AlpicoolListener *listener) { this->listeners_.push_back(listener); }
  void set_bind_enabled(bool bind_enabled) { this->bind_enabled_ = bind_enabled; }

  // State access (used by child components to read current status)
  const AlpicoolStatus &get_status() const { return this->current_status_; }

  // Commands (called by child components to write state)
  void send_set_command(const AlpicoolStatus &desired);
  void send_set_left_command(int8_t temp);
  void send_set_right_command(int8_t temp);

 protected:
  void send_bind_packet_();
  void send_query_packet_();
  void send_raw_(const std::vector<uint8_t> &data);
  void on_notify_data_(const uint8_t *data, uint16_t length);
  void try_parse_buffer_();
  bool parse_status_packet_(const uint8_t *buf, size_t len);
  void push_to_listeners_();
  std::vector<uint8_t> build_packet_(uint8_t cmd, const std::vector<uint8_t> &payload);
  static int8_t to_signed_(uint8_t b) { return (b > 127) ? (int8_t)(b - 256) : (int8_t)b; }

  AlpicoolStatus current_status_;
  std::vector<uint8_t> rx_buffer_;
  AlpicoolState state_{AlpicoolState::IDLE};
  std::vector<AlpicoolListener *> listeners_;

  uint16_t write_handle_{0};
  uint16_t notify_handle_{0};
  uint16_t cccd_handle_{0};
  bool bind_enabled_{false};
  bool was_previously_bound_{false};
  uint32_t bind_start_ms_{0};
  ESPPreferenceObject bound_pref_;
};

}  // namespace alpicool_ble
}  // namespace esphome
