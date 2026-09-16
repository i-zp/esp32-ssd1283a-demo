import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_DC_PIN, CONF_RESET_PIN, CONF_WIDTH, CONF_HEIGHT, CONF_LAMBDA

from . import ssd1283a_ns

SSD1283ADisplay = ssd1283a_ns.class_("SSD1283ADisplay", display.DisplayBuffer, spi.SPIDevice)

CONFIG_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SSD1283ADisplay),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_WIDTH, default=130): cv.int_,
            cv.Optional(CONF_HEIGHT, default=130): cv.int_,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema())
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)
    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    cg.add(var.set_dimensions(config[CONF_WIDTH], config[CONF_HEIGHT]))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
