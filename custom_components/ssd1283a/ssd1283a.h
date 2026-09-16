#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace ssd1283a {

class SSD1283ADisplay : public display::DisplayBuffer,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_8MHZ> {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void fill_screen(uint16_t color);

  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { reset_pin_ = reset_pin; }
  void set_dimensions(int width, int height) {
    width_ = width;
    height_ = height;
  }
  void set_rotation(int rotation) { hw_rotation_ = rotation; }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  void spi_cmd_(uint8_t cmd) {
    this->dc_pin_->digital_write(false);
    this->write_byte(cmd);
  }
  void spi_dat_(uint8_t data) {
    this->dc_pin_->digital_write(true);
    this->write_byte(data);
  }
  void spi_dat16_(uint16_t data) {
    this->dc_pin_->digital_write(true);
    this->write_byte16(data);
  }

  void write_reg(uint8_t cmd, uint16_t value);
  void set_addr_window_full();
  void init_();
  void apply_rotation_();
  void display_();

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override { return width_; }
  int get_height_internal() override { return height_; }

  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  int width_{130};
  int height_{130};
  uint16_t hw_rotation_{0};
  uint8_t *buffer_{nullptr};
};

}  // namespace ssd1283a
}  // namespace esphome
