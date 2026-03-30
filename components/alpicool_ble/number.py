import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, CONF_TYPE, UNIT_CELSIUS, UNIT_MINUTE
from . import alpicool_ble_ns, AlpicoolBLEClient, CONF_ALPICOOL_ID

DEPENDENCIES = ["alpicool_ble"]

AlpicoolNumber = alpicool_ble_ns.class_(
    "AlpicoolNumber", number.Number, cg.Component
)
AlpicoolNumberType = alpicool_ble_ns.enum("AlpicoolNumberType", is_class=True)

NUMBER_TYPES = {
    "hysteresis": {
        "enum": AlpicoolNumberType.HYSTERESIS,
        "min": 1.0,
        "max": 10.0,
        "step": 1.0,
        "unit": UNIT_CELSIUS,
    },
    "start_delay": {
        "enum": AlpicoolNumberType.START_DELAY,
        "min": 0.0,
        "max": 10.0,
        "step": 1.0,
        "unit": UNIT_MINUTE,
    },
    "temp_max": {
        "enum": AlpicoolNumberType.TEMP_MAX,
        "min": -20.0,
        "max": 20.0,
        "step": 1.0,
        "unit": UNIT_CELSIUS,
    },
    "temp_min": {
        "enum": AlpicoolNumberType.TEMP_MIN,
        "min": -20.0,
        "max": 20.0,
        "step": 1.0,
        "unit": UNIT_CELSIUS,
    },
}

CONF_ZONE = "zone"

# Types that are per-zone (left/right)
ZONE_TYPES = {"temp_max", "temp_min"}

CONFIG_SCHEMA = number.number_schema(AlpicoolNumber).extend(
    {
        cv.GenerateID(CONF_ALPICOOL_ID): cv.use_id(AlpicoolBLEClient),
        cv.Required(CONF_TYPE): cv.one_of(*NUMBER_TYPES, lower=True),
        cv.Optional(CONF_ZONE, default="left"): cv.one_of("left", "right", lower=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    number_type = config[CONF_TYPE]
    type_info = NUMBER_TYPES[number_type]

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=type_info["min"],
        max_value=type_info["max"],
        step=type_info["step"],
    )

    hub = await cg.get_variable(config[CONF_ALPICOOL_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_type(type_info["enum"]))
    if number_type in ZONE_TYPES:
        cg.add(var.set_zone(0 if config[CONF_ZONE] == "left" else 1))
    cg.add(hub.register_listener(var))
