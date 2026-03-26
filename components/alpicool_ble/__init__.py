import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL

CODEOWNERS = ["@yourhandle"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "switch", "number", "select", "climate"]

alpicool_ble_ns = cg.esphome_ns.namespace("alpicool_ble")
AlpicoolBLEClient = alpicool_ble_ns.class_(
    "AlpicoolBLEClient", ble_client.BLEClientNode, cg.PollingComponent
)

# Exported for use in platform files
CONF_ALPICOOL_ID = "alpicool_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AlpicoolBLEClient),
            cv.GenerateID(ble_client.CONF_BLE_CLIENT_ID): cv.use_id(
                ble_client.BLEClient
            ),
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
