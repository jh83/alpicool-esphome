import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)
from . import alpicool_ble_ns, AlpicoolBLEClient, CONF_ALPICOOL_ID

DEPENDENCIES = ["alpicool_ble"]

AlpicoolSensor = alpicool_ble_ns.class_(
    "AlpicoolSensor", sensor.Sensor, cg.Component
)
AlpicoolSensorType = alpicool_ble_ns.enum("AlpicoolSensorType", is_class=True)

SENSOR_TYPES = {
    "battery_percent": {
        "enum": AlpicoolSensorType.BATTERY_PERCENT,
        "unit": UNIT_PERCENT,
        "device_class": DEVICE_CLASS_BATTERY,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "battery_voltage": {
        "enum": AlpicoolSensorType.BATTERY_VOLTAGE,
        "unit": UNIT_VOLT,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 1,
    },
    "left_current_temperature": {
        "enum": AlpicoolSensorType.LEFT_CURRENT_TEMPERATURE,
        "unit": UNIT_CELSIUS,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
    "right_current_temperature": {
        "enum": AlpicoolSensorType.RIGHT_CURRENT_TEMPERATURE,
        "unit": UNIT_CELSIUS,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    },
}

CONFIG_SCHEMA = sensor.sensor_schema(AlpicoolSensor).extend(
    {
        cv.GenerateID(CONF_ALPICOOL_ID): cv.use_id(AlpicoolBLEClient),
        cv.Required(CONF_TYPE): cv.one_of(*SENSOR_TYPES, lower=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    sensor_type = config[CONF_TYPE]
    type_info = SENSOR_TYPES[sensor_type]

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    hub = await cg.get_variable(config[CONF_ALPICOOL_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_type(type_info["enum"]))
    cg.add(hub.register_listener(var))
