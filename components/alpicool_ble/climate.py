import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID
from . import alpicool_ble_ns, AlpicoolBLEClient, CONF_ALPICOOL_ID

DEPENDENCIES = ["alpicool_ble"]

AlpicoolClimate = alpicool_ble_ns.class_(
    "AlpicoolClimate", climate.Climate, cg.Component
)

CONF_ZONE = "zone"

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AlpicoolClimate),
        cv.GenerateID(CONF_ALPICOOL_ID): cv.use_id(AlpicoolBLEClient),
        cv.Optional(CONF_ZONE, default="left"): cv.one_of("left", "right", lower=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    hub = await cg.get_variable(config[CONF_ALPICOOL_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_zone(0 if config[CONF_ZONE] == "left" else 1))
    cg.add(hub.register_listener(var))
