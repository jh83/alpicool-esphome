# Alpicool BLE — ESPHome External Component

An ESPHome external component that lets an ESP32 communicate directly with **Alpicool portable refrigerators** over Bluetooth Low Energy. Instead of running a HACS integration inside Home Assistant, the BLE logic lives on the ESP32 itself and the fridge appears in HA as a standard ESPHome device.

Based on the reverse-engineered protocol from [Gruni22/alpicool_ha_ble](https://github.com/Gruni22/alpicool_ha_ble) and [klightspeed/BrassMonkeyFridgeMonitor](https://github.com/klightspeed/BrassMonkeyFridgeMonitor).

---

## Features

| Entity | Type | Description |
|--------|------|-------------|
| Fridge (Left Zone) | Climate | Current temp, target temp, on/off, Max/Eco preset |
| Fridge (Right Zone) | Climate | Same as above — dual-zone models only |
| Battery | Sensor | Charge percentage (%) |
| Battery Voltage | Sensor | Voltage in volts (e.g. 12.5 V) |
| Current Temperature | Sensor | Actual measured temperature per zone |
| Panel Lock | Switch | Locks/unlocks the physical control panel |
| Hysteresis | Number | Compressor on/off temperature band (1–10 °C) |
| Compressor Start Delay | Number | Delay before compressor starts (0–10 min) |
| Max Temperature | Number | Maximum selectable target temperature per zone (+8 for fridge, -12 for freezer) |
| Min Temperature | Number | Minimum selectable target temperature per zone (0 for fridge, –20 for freezer) |
| Battery Saver | Select | Battery protection level: Low / Medium / High |

---

## Requirements

- ESP32 board (any variant with Bluetooth)
- ESPHome 2023.6 or newer
- `esp-idf` framework (required for BLE client — see config below)
- Alpicool fridge with BLE (T/CF/B series and compatible brands: Iceco, Bodega, etc.)

---

## Installation

### 1. Find your fridge's Bluetooth MAC address

Use a BLE scanner app such as **nRF Connect** (Android/iOS) or **LightBlue** and look for a device advertising service UUID `0x1234`. The MAC address will look like `AA:BB:CC:DD:EE:FF`.

Alternatively, if you previously used the HACS integration, the MAC address is shown in the integration's device page.

### 2. Create your ESPHome configuration

Copy `example.yaml` and edit the substitutions at the top:

```yaml
substitutions:
  device_name: alpicool-fridge
  fridge_mac: "AA:BB:CC:DD:EE:FF"  # Your fridge's MAC address
```

Reference this repository as an external component:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/jh83/alpicool-esphome
      ref: main
    components: [alpicool_ble]
```

> **Important:** The `esp-idf` framework is required. Add this to your config:
>
> ```yaml
> esp32:
>   board: esp32dev
>   framework:
>     type: esp-idf
> ```

### 3. Flash and verify

```bash
esphome run example.yaml
```

Watch the logs for the connection sequence:

```
[D][alpicool_ble] Connected — discovering services
[D][alpicool_ble] Write handle: 0x000e
[D][alpicool_ble] Notify handle: 0x0011
[D][alpicool_ble] Notifications enabled
[D][alpicool_ble] Sending BIND packet
[D][alpicool_ble] BIND echo received
[D][alpicool_ble] Status: pwr=1 mode=0 left_cur=-5°C left_tgt=-5°C bat=85%
```

Once the first STATUS is received, all entities will appear in Home Assistant.

---

## Full Configuration Reference

### Hub (`alpicool_ble`)

```yaml
alpicool_ble:
  id: my_fridge
  ble_client_id: fridge_ble_client  # Required: ID of the ble_client entry
  update_interval: 30s              # How often to poll (default: 30s)
  bind: false                       # Send BLE bind packet on first connect (default: false)
```

### Climate

```yaml
climate:
  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge"
    zone: left   # "left" (default) or "right" (dual-zone only)
```

The climate entity supports:
- **Modes**: `off` / `cool`
- **Presets**: `boost` (Max) / `eco` (Eco)
- **Temperature range**: dynamically set from the fridge's reported min/max (defaults to −20 °C to +20 °C until first status is received), 1 °C steps

### Sensors

```yaml
sensor:
  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Battery"
    type: battery_percent           # or: battery_voltage,
                                    #     left_current_temperature,
                                    #     right_current_temperature
```

| `type` | Unit | Notes |
|--------|------|-------|
| `battery_percent` | % | 0–100 |
| `battery_voltage` | V | e.g. `12.5` |
| `left_current_temperature` | °C | Actual measured temp, left zone |
| `right_current_temperature` | °C | Dual-zone models only |

### Switch

```yaml
switch:
  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Panel Lock"
```

### Number

```yaml
number:
  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Hysteresis"
    type: hysteresis    # Range: 1–10 °C

  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Start Delay"
    type: start_delay   # Range: 0–10 min

  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Left Max Temperature"
    type: temp_max
    zone: left          # Range: −20–20 °C

  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Left Min Temperature"
    type: temp_min
    zone: left          # Range: −20–20 °C

  # Dual-zone fridges only:
  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Right Max Temperature"
    type: temp_max
    zone: right

  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Right Min Temperature"
    type: temp_min
    zone: right
```

### Select

```yaml
select:
  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Battery Saver"
    # Options: "Low", "Medium", "High"
```

---

## Dual-Zone Models

For fridges with two independent temperature zones, add a second climate entity with `zone: right` and a `right_current_temperature` sensor:

```yaml
climate:
  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Left Zone"
    zone: left

  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Fridge Right Zone"
    zone: right

sensor:
  - platform: alpicool_ble
    alpicool_id: my_fridge
    name: "Right Zone Temperature"
    type: right_current_temperature
```

Dual-zone detection is automatic at runtime — the right-zone entities will remain unavailable until the fridge reports dual-zone status.

---

## How It Works

The ESP32 acts as a BLE GATT client connecting to the fridge's custom BLE service (`0x1234`). The protocol uses two characteristics:

- **Write** (`0x1235`): send commands to the fridge
- **Notify** (`0x1236`): receive status updates and command echoes

### Connection sequence

```
ESP32 connects to fridge
  → discovers GATT services
  → registers for BLE notifications
  → sends BIND packet (authentication)
  → waits up to 20 s for BIND echo
  → sends QUERY to request current status
  → parses STATUS response → pushes to HA
  → polls with QUERY every 30 s
```

On reconnect (including after an ESP32 restart), the BIND step is skipped for faster recovery. The bind state is persisted to flash, so the component remembers a successful bind across reboots.

### Packet format

```
[0xFE 0xFE] [Length] [Command] [Payload...] [Checksum: 2 bytes big-endian]
```

The checksum is the 16-bit sum of all preceding bytes. Temperature values are signed bytes (negative values use two's complement).

---

## Troubleshooting

**No connection / entities never appear**
- Confirm the MAC address is correct
- Make sure you're using `framework: type: esp-idf`, if BLE errors are logged try changing `esp-idf` to `arduino` to see if the BLE errors dissapear.
- ESP32 can only connect to one BLE device at a time as a client

**"Write characteristic 0x1235 not found"**
- Some fridge firmware versions use a different service UUID. Try scanning with nRF Connect and check which service UUID the write/notify characteristics are under. Open an issue with the scan results.

**BIND times out but fridge still works**
- This is normal for some models. The component logs a warning and proceeds — it is not an error. The result is still saved to flash, so the bind timeout only happens once (on the very first connection after flashing).

**Temperature shows −128 or unrealistic values**
- Verify the fridge is powered on and not in standby
- Check that the `zone` setting matches your fridge model (single vs dual-zone)

---

## Compatibility

Tested/reported working with:
- Only my fridge/freezer which is a dual unit running the Alpicool protocol.

If your model is not listed but uses the same Alpicool app, it will likely work. Please open an issue to report compatibility.

---

## License

MIT
