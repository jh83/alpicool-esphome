#include "alpicool_ble.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

// Make espbt:: accessible in this translation unit
namespace espbt = esphome::esp32_ble_tracker;

namespace esphome {
namespace alpicool_ble {

static const char *const TAG = "alpicool_ble";

// UUID short forms (Bluetooth SIG base: 0000xxxx-0000-1000-8000-00805f9b34fb)
static const espbt::ESPBTUUID SERVICE_UUID = espbt::ESPBTUUID::from_uint16(0x1234);
static const espbt::ESPBTUUID WRITE_CHAR_UUID = espbt::ESPBTUUID::from_uint16(0x1235);
static const espbt::ESPBTUUID NOTIFY_CHAR_UUID = espbt::ESPBTUUID::from_uint16(0x1236);

// Fixed magic packets (hardcoded per protocol spec — do NOT compute checksum for these)
static const uint8_t BIND_PACKET[] = {0xFE, 0xFE, 0x03, 0x00, 0x01, 0xFF};
static const uint8_t QUERY_PACKET[] = {0xFE, 0xFE, 0x03, 0x01, 0x02, 0x00};

// Command bytes
static const uint8_t CMD_BIND = 0x00;
static const uint8_t CMD_QUERY = 0x01;
static const uint8_t CMD_SET = 0x02;
static const uint8_t CMD_SET_LEFT = 0x05;
static const uint8_t CMD_SET_RIGHT = 0x06;

// Max RX buffer size before we assume sync loss and reset
static const size_t RX_BUFFER_MAX = 64;

// ─── Component Lifecycle ────────────────────────────────────────────────────

void AlpicoolBLEClient::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Alpicool BLE...");
  this->bound_pref_ = global_preferences->make_preference<bool>(0xA1C001U);
  bool stored = false;
  if (this->bound_pref_.load(&stored))
    this->was_previously_bound_ = stored;
}

void AlpicoolBLEClient::loop() {
  // Bind timeout: proceed without binding after 20 s
  if (this->bind_enabled_ &&
      this->state_ == AlpicoolState::BINDING &&
      (millis() - this->bind_start_ms_) > 20000) {
    ESP_LOGW(TAG, "BIND timed out — some models skip binding, continuing anyway");
    this->was_previously_bound_ = true;
    this->bound_pref_.save(&this->was_previously_bound_);
    this->send_query_packet_();
    this->state_ = AlpicoolState::READY;
  }
}

void AlpicoolBLEClient::update() {
  // Called at the configured update_interval by PollingComponent
  if (this->state_ == AlpicoolState::READY) {
    ESP_LOGD(TAG, "Polling: sending QUERY");
    this->send_query_packet_();
  }
}

void AlpicoolBLEClient::dump_config() {
  ESP_LOGCONFIG(TAG, "Alpicool BLE:");
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", this->get_update_interval());
}

// ─── BLE GATT Event Handler ─────────────────────────────────────────────────

void AlpicoolBLEClient::gattc_event_handler(esp_gattc_cb_event_t event,
                                              esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Connection failed, status=%d", param->open.status);
        this->state_ = AlpicoolState::IDLE;
        break;
      }
      ESP_LOGI(TAG, "Connected — discovering services");
      this->rx_buffer_.clear();
      this->write_handle_ = 0;
      this->notify_handle_ = 0;
      this->cccd_handle_ = 0;
      this->state_ = AlpicoolState::DISCOVERING;
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Service search failed, status=%d", param->search_cmpl.status);
        break;
      }

      // Find write characteristic (0x1235)
      auto *write_chr = this->parent_->get_characteristic(SERVICE_UUID, WRITE_CHAR_UUID);
      if (write_chr == nullptr) {
        ESP_LOGE(TAG, "Write characteristic 0x1235 not found");
        break;
      }
      this->write_handle_ = write_chr->handle;
      ESP_LOGD(TAG, "Write handle: 0x%04x", this->write_handle_);

      // Find notify characteristic (0x1236)
      auto *notify_chr = this->parent_->get_characteristic(SERVICE_UUID, NOTIFY_CHAR_UUID);
      if (notify_chr == nullptr) {
        ESP_LOGE(TAG, "Notify characteristic 0x1236 not found");
        break;
      }
      this->notify_handle_ = notify_chr->handle;
      ESP_LOGD(TAG, "Notify handle: 0x%04x", this->notify_handle_);

      // Find CCCD descriptor (UUID 0x2902) under the notify characteristic
      for (auto *desc : notify_chr->descriptors) {
        if (desc->uuid == espbt::ESPBTUUID::from_uint16(0x2902)) {
          this->cccd_handle_ = desc->handle;
          ESP_LOGD(TAG, "CCCD handle: 0x%04x", this->cccd_handle_);
          break;
        }
      }

      // Register for notifications at the ESP-IDF level
      auto status = esp_ble_gattc_register_for_notify(
          this->parent_->get_gattc_if(),
          this->parent_->get_remote_bda(),
          this->notify_handle_);
      if (status != ESP_OK) {
        ESP_LOGW(TAG, "register_for_notify failed: %d", status);
      }
      this->state_ = AlpicoolState::SUBSCRIBING;
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "REG_FOR_NOTIFY failed, status=%d", param->reg_for_notify.status);
        break;
      }

      // Enable notifications on the remote device by writing CCCD = 0x0001
      if (this->cccd_handle_ != 0) {
        uint8_t notify_en[2] = {0x01, 0x00};
        auto status = esp_ble_gattc_write_char_descr(
            this->parent_->get_gattc_if(),
            this->parent_->get_conn_id(),
            this->cccd_handle_,
            sizeof(notify_en),
            notify_en,
            ESP_GATT_WRITE_TYPE_RSP,
            ESP_GATT_AUTH_REQ_NONE);
        if (status != ESP_OK) {
          ESP_LOGW(TAG, "CCCD write failed: %d", status);
        }
        // Transition happens in WRITE_DESCR_EVT
      } else {
        // No CCCD found, proceed directly
        if (this->bind_enabled_ && !this->was_previously_bound_) {
          ESP_LOGD(TAG, "Sending BIND packet");
          this->send_bind_packet_();
          this->bind_start_ms_ = millis();
          this->state_ = AlpicoolState::BINDING;
        } else {
          ESP_LOGD(TAG, "Skipping BIND, sending QUERY");
          this->send_query_packet_();
          this->state_ = AlpicoolState::READY;
        }
      }
      break;
    }

    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.handle != this->cccd_handle_) break;
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "CCCD write failed, status=%d", param->write.status);
        break;
      }
      ESP_LOGD(TAG, "Notifications enabled");

      if (this->bind_enabled_ && !this->was_previously_bound_) {
        ESP_LOGD(TAG, "Sending BIND packet");
        this->send_bind_packet_();
        this->bind_start_ms_ = millis();
        this->state_ = AlpicoolState::BINDING;
      } else {
        ESP_LOGD(TAG, "Skipping BIND, sending QUERY");
        this->send_query_packet_();
        this->state_ = AlpicoolState::READY;
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->notify_handle_) break;
      this->on_notify_data_(param->notify.value, param->notify.value_len);
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGI(TAG, "Disconnected");
      this->state_ = AlpicoolState::IDLE;
      this->current_status_.valid = false;
      this->node_state = espbt::ClientState::IDLE;
      for (auto *listener : this->listeners_)
        listener->on_alpicool_connected(false);
      break;
    }

    default:
      break;
  }
}

// ─── Notification Data Handler ───────────────────────────────────────────────

void AlpicoolBLEClient::on_notify_data_(const uint8_t *data, uint16_t length) {
  // Accumulate into reassembly buffer
  for (uint16_t i = 0; i < length; i++)
    this->rx_buffer_.push_back(data[i]);

  if (this->rx_buffer_.size() > RX_BUFFER_MAX) {
    ESP_LOGW(TAG, "RX buffer overflow, clearing");
    this->rx_buffer_.clear();
    return;
  }

  this->try_parse_buffer_();
}

void AlpicoolBLEClient::try_parse_buffer_() {
  while (true) {
    // Search for FE FE preamble
    size_t start = std::string::npos;
    for (size_t i = 0; i + 1 < this->rx_buffer_.size(); i++) {
      if (this->rx_buffer_[i] == 0xFE && this->rx_buffer_[i + 1] == 0xFE) {
        start = i;
        break;
      }
    }

    if (start == std::string::npos) {
      if (!this->rx_buffer_.empty()) {
        ESP_LOGD(TAG, "No FE FE preamble in buffer, clearing");
        this->rx_buffer_.clear();
      }
      return;
    }

    // Discard bytes before preamble
    if (start > 0) {
      ESP_LOGD(TAG, "Discarding %u garbage bytes before preamble", (unsigned)start);
      this->rx_buffer_.erase(this->rx_buffer_.begin(),
                              this->rx_buffer_.begin() + start);
    }

    // Need at least 3 bytes to read length byte
    if (this->rx_buffer_.size() < 3) return;

    uint8_t length_byte = this->rx_buffer_[2];
    size_t expected_total = 3 + (size_t)length_byte;

    if (this->rx_buffer_.size() < expected_total) {
      ESP_LOGD(TAG, "Incomplete packet: have %u, need %u — waiting",
               (unsigned)this->rx_buffer_.size(), (unsigned)expected_total);
      return;
    }

    // We have a complete packet — extract and process it
    const uint8_t *pkt = this->rx_buffer_.data();
    uint8_t cmd = pkt[3];
    const uint8_t *payload = pkt + 4;
    size_t payload_len = expected_total - 4;  // includes trailing checksum bytes

    ESP_LOGD(TAG, "RX packet cmd=0x%02x len=%u", cmd, (unsigned)expected_total);

    if (cmd == CMD_QUERY) {
      // STATUS response — parse it
      if (this->parse_status_packet_(payload, payload_len)) {
        // First successful status → connected and bound
        if (!this->current_status_.valid) {
          this->current_status_.valid = true;
          this->node_state = espbt::ClientState::ESTABLISHED;
          for (auto *listener : this->listeners_)
            listener->on_alpicool_connected(true);
        }
        this->push_to_listeners_();

        // Bind state resolved on first STATUS
        if (this->state_ == AlpicoolState::BINDING) {
          this->state_ = AlpicoolState::READY;
          this->was_previously_bound_ = true;
          this->bound_pref_.save(&this->was_previously_bound_);
        }
      }
    } else if (cmd == CMD_BIND) {
      ESP_LOGD(TAG, "BIND echo received");
      if (this->state_ == AlpicoolState::BINDING) {
        this->was_previously_bound_ = true;
        this->bound_pref_.save(&this->was_previously_bound_);
        this->send_query_packet_();
        this->state_ = AlpicoolState::READY;
      }
    } else if (cmd == CMD_SET || cmd == CMD_SET_LEFT || cmd == CMD_SET_RIGHT) {
      ESP_LOGD(TAG, "SET echo ignored");
    } else {
      ESP_LOGD(TAG, "Unhandled notification cmd=0x%02x", cmd);
    }

    // Remove the consumed packet from the buffer and loop to handle next one
    this->rx_buffer_.erase(this->rx_buffer_.begin(),
                            this->rx_buffer_.begin() + expected_total);
  }
}

bool AlpicoolBLEClient::parse_status_packet_(const uint8_t *payload, size_t len) {
  // Minimum 18 bytes of actual data (+ 2 bytes checksum = 20 bytes payload)
  if (len < 18) {
    ESP_LOGW(TAG, "STATUS payload too short: %u bytes", (unsigned)len);
    return false;
  }

  AlpicoolStatus &s = this->current_status_;

  s.locked = payload[0] != 0;
  s.powered_on = payload[1] != 0;
  s.run_mode = payload[2];
  s.bat_saver = payload[3];
  s.left_target = to_signed_(payload[4]);
  s.temp_max = to_signed_(payload[5]);
  s.temp_min = to_signed_(payload[6]);
  s.left_ret_diff = to_signed_(payload[7]);
  s.start_delay = payload[8];
  s.unit = payload[9];
  s.left_tc_hot = to_signed_(payload[10]);
  s.left_tc_mid = to_signed_(payload[11]);
  s.left_tc_cold = to_signed_(payload[12]);
  s.left_tc_halt = to_signed_(payload[13]);
  s.left_current = to_signed_(payload[14]);
  s.bat_percent = payload[15];
  s.bat_vol_int = payload[16];
  s.bat_vol_dec = payload[17];

  // Dual-zone: need at least 28 bytes of actual data (+ 2 checksum = 30 payload bytes)
  if (len >= 30) {
    s.dual_zone = true;
    s.right_target = to_signed_(payload[18]);
    s.right_temp_max = to_signed_(payload[19]);
    s.right_temp_min = to_signed_(payload[20]);
    s.right_ret_diff = to_signed_(payload[21]);
    s.right_tc_hot = to_signed_(payload[22]);
    s.right_tc_mid = to_signed_(payload[23]);
    s.right_tc_cold = to_signed_(payload[24]);
    s.right_tc_halt = to_signed_(payload[25]);
    s.right_current = to_signed_(payload[26]);
    s.running_status = payload[27];
  } else {
    s.dual_zone = false;
  }

  ESP_LOGD(TAG, "Status: pwr=%d mode=%d left_cur=%d°C left_tgt=%d°C bat=%d%%",
           s.powered_on, s.run_mode, s.left_current, s.left_target, s.bat_percent);
  return true;
}

// ─── Packet Builders ─────────────────────────────────────────────────────────

void AlpicoolBLEClient::send_bind_packet_() {
  std::vector<uint8_t> pkt(BIND_PACKET, BIND_PACKET + sizeof(BIND_PACKET));
  this->send_raw_(pkt);
}

void AlpicoolBLEClient::send_query_packet_() {
  std::vector<uint8_t> pkt(QUERY_PACKET, QUERY_PACKET + sizeof(QUERY_PACKET));
  this->send_raw_(pkt);
}

std::vector<uint8_t> AlpicoolBLEClient::build_packet_(uint8_t cmd,
                                                        const std::vector<uint8_t> &data) {
  std::vector<uint8_t> packet;
  packet.push_back(0xFE);
  packet.push_back(0xFE);

  // Length = 1 (cmd) + data_size + 2 (checksum)
  uint8_t length = (uint8_t)(1 + data.size() + 2);
  packet.push_back(length);
  packet.push_back(cmd);
  for (auto b : data) packet.push_back(b);

  // Checksum = sum of all bytes so far, low 16 bits, big-endian
  uint16_t checksum = 0;
  for (auto b : packet) checksum += b;
  checksum &= 0xFFFF;
  packet.push_back((checksum >> 8) & 0xFF);
  packet.push_back(checksum & 0xFF);

  return packet;
}

void AlpicoolBLEClient::send_set_left_command(int8_t temp) {
  std::vector<uint8_t> payload = {(uint8_t)(temp & 0xFF)};
  auto pkt = this->build_packet_(CMD_SET_LEFT, payload);
  this->send_raw_(pkt);
}

void AlpicoolBLEClient::send_set_right_command(int8_t temp) {
  std::vector<uint8_t> payload = {(uint8_t)(temp & 0xFF)};
  auto pkt = this->build_packet_(CMD_SET_RIGHT, payload);
  this->send_raw_(pkt);
}

void AlpicoolBLEClient::send_set_command(const AlpicoolStatus &s) {
  std::vector<uint8_t> payload = {
      (uint8_t)(s.locked ? 1 : 0),
      (uint8_t)(s.powered_on ? 1 : 0),
      s.run_mode,
      s.bat_saver,
      (uint8_t)(s.left_target & 0xFF),
      (uint8_t)(s.temp_max & 0xFF),
      (uint8_t)(s.temp_min & 0xFF),
      (uint8_t)(s.left_ret_diff & 0xFF),
      s.start_delay,
      s.unit,
      (uint8_t)(s.left_tc_hot & 0xFF),
      (uint8_t)(s.left_tc_mid & 0xFF),
      (uint8_t)(s.left_tc_cold & 0xFF),
      (uint8_t)(s.left_tc_halt & 0xFF),
  };

  if (s.dual_zone) {
    payload.push_back((uint8_t)(s.right_target & 0xFF));
    payload.push_back((uint8_t)(s.right_temp_max & 0xFF));
    payload.push_back((uint8_t)(s.right_temp_min & 0xFF));
    payload.push_back((uint8_t)(s.right_ret_diff & 0xFF));
    payload.push_back((uint8_t)(s.right_tc_hot & 0xFF));
    payload.push_back((uint8_t)(s.right_tc_mid & 0xFF));
    payload.push_back((uint8_t)(s.right_tc_cold & 0xFF));
    payload.push_back((uint8_t)(s.right_tc_halt & 0xFF));
    payload.push_back(0x00);  // unknown
    payload.push_back(0x00);
    payload.push_back(0x00);
  }

  auto pkt = this->build_packet_(CMD_SET, payload);
  this->send_raw_(pkt);
}

void AlpicoolBLEClient::send_raw_(const std::vector<uint8_t> &data) {
  if (this->write_handle_ == 0) {
    ESP_LOGW(TAG, "Cannot send — write handle not initialized");
    return;
  }

  ESP_LOGD(TAG, "TX %u bytes", (unsigned)data.size());

  auto status = esp_ble_gattc_write_char(
      this->parent_->get_gattc_if(),
      this->parent_->get_conn_id(),
      this->write_handle_,
      data.size(),
      const_cast<uint8_t *>(data.data()),
      ESP_GATT_WRITE_TYPE_NO_RSP,
      ESP_GATT_AUTH_REQ_NONE);

  if (status != ESP_OK) {
    // Retry with write-with-response
    status = esp_ble_gattc_write_char(
        this->parent_->get_gattc_if(),
        this->parent_->get_conn_id(),
        this->write_handle_,
        data.size(),
        const_cast<uint8_t *>(data.data()),
        ESP_GATT_WRITE_TYPE_RSP,
        ESP_GATT_AUTH_REQ_NONE);
    if (status != ESP_OK) {
      ESP_LOGW(TAG, "BLE write failed: %d", status);
    }
  }
}

// ─── Listener Push ────────────────────────────────────────────────────────────

void AlpicoolBLEClient::push_to_listeners_() {
  for (auto *listener : this->listeners_)
    listener->on_alpicool_status(this->current_status_);
}

}  // namespace alpicool_ble
}  // namespace esphome
