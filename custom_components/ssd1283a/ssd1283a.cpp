#include "ssd1283a.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace ssd1283a {

static const char *TAG = "ssd1283a";

// ---------- 16-битная запись регистра ----------

void SSD1283ADisplay::write_reg(uint8_t cmd, uint16_t value) {
  this->enable();
  this->spi_cmd_(cmd);
  this->spi_dat16_(value);
  this->disable();
}

// ---------- Адресное окно на весь экран ----------
// Формат байт и смещения взяты из эталонной библиотеки SSD1283A (Jean-Marc Zingg).
// Регистры 0x44/0x45/0x21 принимают одиночные байты (не 16-бит), порядок зависит от поворота.

void SSD1283ADisplay::set_addr_window_full() {
  this->enable();
  int x2 = width_ - 1;
  int y2 = height_ - 1;

  switch (hw_rotation_) {
    case 0:
      spi_cmd_(0x44); spi_dat_(x2 + 2); spi_dat_(2);
      spi_cmd_(0x45); spi_dat_(y2 + 2); spi_dat_(2);
      spi_cmd_(0x21); spi_dat_(2);      spi_dat_(2);
      break;
    case 90:
      spi_cmd_(0x44); spi_dat_(height_ + 1);         spi_dat_(height_ - y2 + 1);
      spi_cmd_(0x45); spi_dat_(width_ - 1);          spi_dat_(width_ - x2 - 1);
      spi_cmd_(0x21); spi_dat_(width_ - 1);          spi_dat_(height_ + 1);
      break;
    case 180:
      spi_cmd_(0x44); spi_dat_(width_ + 1);         spi_dat_(width_ - x2 + 1);
      spi_cmd_(0x45); spi_dat_(height_ + 1);        spi_dat_(height_ - y2 + 1);
      spi_cmd_(0x21); spi_dat_(height_ + 1);        spi_dat_(width_ + 1);
      break;
    case 270:
      spi_cmd_(0x44); spi_dat_(y2 + 2);   spi_dat_(2);
      spi_cmd_(0x45); spi_dat_(x2);       spi_dat_(0);
      spi_cmd_(0x21); spi_dat_(0);        spi_dat_(2);
      break;
  }
  spi_cmd_(0x22);  // Write to GRAM
  this->disable();
}

// ---------- Инициализация ----------

void SSD1283ADisplay::init_() {
  ESP_LOGI(TAG, "init_() start");

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
    delay(200);
  }

  // Полная последовательность инициализации из эталонной библиотеки SSD1283A
  write_reg(0x10, 0x2F8E);
  write_reg(0x11, 0x000C);
  write_reg(0x07, 0x0021);
  write_reg(0x28, 0x0006);
  write_reg(0x28, 0x0005);
  write_reg(0x27, 0x057F);
  write_reg(0x29, 0x89A1);
  write_reg(0x00, 0x0001);
  delay(100);
  write_reg(0x29, 0x80B0);
  delay(30);
  write_reg(0x29, 0xFFFE);
  write_reg(0x07, 0x0223);
  delay(30);
  write_reg(0x07, 0x0233);
  write_reg(0x01, 0x2183);
  write_reg(0x03, 0x6830);
  write_reg(0x2F, 0xFFFF);
  write_reg(0x2C, 0x8000);
  write_reg(0x27, 0x0570);
  write_reg(0x02, 0x0300);
  write_reg(0x0B, 0x580C);
  write_reg(0x12, 0x0609);
  write_reg(0x13, 0x3100);

  apply_rotation_();
}

// ---------- Аппаратный поворот ----------

void SSD1283ADisplay::apply_rotation_() {
  switch (hw_rotation_) {
    case 0:
      write_reg(0x01, 0x2183);
      write_reg(0x03, 0x6830);
      break;
    case 90:
      write_reg(0x01, 0x2283);
      write_reg(0x03, 0x6808);
      break;
    case 180:
      write_reg(0x01, 0x2183);
      write_reg(0x03, 0x6800);
      break;
    case 270:
      write_reg(0x01, 0x2283);
      write_reg(0x03, 0x6838);
      break;
    default:
      write_reg(0x01, 0x2183);
      write_reg(0x03, 0x6830);
      break;
  }
}

// ---------- Жизненный цикл ----------

void SSD1283ADisplay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SSD1283A...");
  this->spi_setup();
  this->dc_pin_->setup();
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
  }
  this->init_();

  this->buffer_ = new uint8_t[(uint32_t) width_ * height_ * 2];
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate display buffer!");
    return;
  }
  memset(this->buffer_, 0, (uint32_t) width_ * height_ * 2);

  ESP_LOGI(TAG, "SSD1283A setup complete (%dx%d, rotation %u)", width_, height_, hw_rotation_);
}

void SSD1283ADisplay::update() {
  this->do_update_();
  this->display_();
}

void SSD1283ADisplay::display_() {
  set_addr_window_full();

  uint32_t num_bytes = (uint32_t) width_ * height_ * 2;
  this->enable();
  this->dc_pin_->digital_write(true);
  this->write_array(this->buffer_, num_bytes);
  this->disable();
}

// ---------- Отрисовка ----------

void SSD1283ADisplay::fill_screen(uint16_t color) {
  set_addr_window_full();
  uint8_t hi = (color >> 8) & 0xFF;
  uint8_t lo = color & 0xFF;
  this->enable();
  this->dc_pin_->digital_write(true);
  for (int i = 0; i < width_ * height_; i++) {
    this->write_byte(hi);
    this->write_byte(lo);
  }
  this->disable();
}

void SSD1283ADisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= width_ || y < 0 || y >= height_)
    return;

  uint16_t color565 = ((color.r & 0xF8) << 8) | ((color.g & 0xFC) << 3) | (color.b >> 3);
  uint32_t pos = (uint32_t)(y * width_ + x) * 2;
  this->buffer_[pos] = (color565 >> 8) & 0xFF;
  this->buffer_[pos + 1] = color565 & 0xFF;
}

// ---------- Диагностика ----------

void SSD1283ADisplay::dump_config() {
  ESP_LOGI(TAG, "SSD1283A Display:");
  ESP_LOGI(TAG, "  Width: %d", width_);
  ESP_LOGI(TAG, "  Height: %d", height_);
  ESP_LOGI(TAG, "  Rotation: %u", hw_rotation_);
  LOG_PIN("  DC Pin: ", dc_pin_);
  if (reset_pin_ != nullptr) {
    LOG_PIN("  Reset Pin: ", reset_pin_);
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace ssd1283a
}  // namespace esphome
