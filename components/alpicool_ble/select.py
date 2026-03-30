import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID
from . import alpicool_ble_ns, AlpicoolBLEClient, CONF_ALPICOOL_ID

DEPENDENCIES = ["alpicool_ble"]

AlpicoolSelect = alpicool_ble_ns.class_(
    "AlpicoolSelect", select.Select, cg.Component
)

BATTERY_SAVER_OPTIONS = ["Low", "Medium", "High"]

CONFIG_SCHEMA = select.select_schema(AlpicoolSelect).extend(
    {
        cv.GenerateID(CONF_ALPICOOL_ID): cv.use_id(AlpicoolBLEClient),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await select.register_select(var, config, options=BATTERY_SAVER_OPTIONS)

    hub = await cg.get_variable(config[CONF_ALPICOOL_ID])
    cg.add(var.set_hub(hub))
    cg.add(hub.register_listener(var))
