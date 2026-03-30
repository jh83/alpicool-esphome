import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from . import alpicool_ble_ns, AlpicoolBLEClient, CONF_ALPICOOL_ID

DEPENDENCIES = ["alpicool_ble"]

AlpicoolSwitch = alpicool_ble_ns.class_(
    "AlpicoolSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = switch.switch_schema(AlpicoolSwitch, icon="mdi:lock").extend(
    {
        cv.GenerateID(CONF_ALPICOOL_ID): cv.use_id(AlpicoolBLEClient),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    hub = await cg.get_variable(config[CONF_ALPICOOL_ID])
    cg.add(var.set_hub(hub))
    cg.add(hub.register_listener(var))
