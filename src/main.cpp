#include <Arduino.h>
#include <SPIFFS.h>

#include <Audio.hpp>
#include <gfx.hpp>
#ifdef T_QT_PRO
#include <st7789.hpp>
#endif
#ifdef ESP_WROVER_KIT
#include <ili9341.hpp>
#endif
// #include "NotoSans_Bold.hpp"
#include "Telegrama.hpp"

#define VOLUME_DECAY_ALPHA 0.3

// #define LCD_WIDTH 128
// #define LCD_HEIGHT 128
// #define LCD_ROTATION 2
// #define LCD_HOST SPI1_HOST

// #define PIN_NUM_CS 5
// #define PIN_NUM_MOSI 2
// #define PIN_NUM_MISO -1
// #define PIN_NUM_CLK 3
// #define PIN_NUM_DC 6
// #define PIN_NUM_RST 1
// #define PIN_NUM_BCKL 10

#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define LCD_ROTATION 0
#define LCD_HOST SPI1_HOST

#define PIN_NUM_CS 22
#define PIN_NUM_MOSI 23
#define PIN_NUM_MISO 25
#define PIN_NUM_CLK 19
#define PIN_NUM_DC 21
#define PIN_NUM_RST 18
#define PIN_NUM_BCKL 5

using namespace arduino;
using namespace gfx;

using bus_type =
    tft_spi_ex<LCD_HOST, PIN_NUM_CS, PIN_NUM_MOSI, PIN_NUM_MISO, PIN_NUM_CLK,
               SPI_MODE0, LCD_WIDTH * LCD_HEIGHT * 2 + 8>;
#ifdef T_QT_PRO
using lcd_type = st7789<LCD_WIDTH, LCD_HEIGHT, PIN_NUM_DC, PIN_NUM_RST, -1,
                        bus_type, LCD_ROTATION, false, 400, 200>;
#endif
#ifdef ESP_WROVER_KIT
using lcd_type = ili9341< PIN_NUM_DC, PIN_NUM_RST, -1,
                        bus_type, LCD_ROTATION, false, 400, 200>;
#endif
using lcd_color = color<typename lcd_type::pixel_type>;

lcd_type lcd;

using bmp_type = bitmap<decltype(lcd)::pixel_type>;
using bmp_color = color<typename bmp_type::pixel_type>;

Audio audio = Audio();

double decibelAccum = 0;

const uint8_t red_hue = 0;
const uint8_t green_hue = 128 - 32;

void setup() {
  Serial.begin(115200);

  // override backlight control (active LOW)
  pinMode(PIN_NUM_BCKL, OUTPUT);
  digitalWrite(PIN_NUM_BCKL, LOW);

  // set up display
  lcd.initialize();

  // start microphone task loop
  audio.begin();
}

void loop() {
  // check if we have audio data
  double db = audio.getDecibels();

  if (db == 0) {
    return;
  }

  decibelAccum =
      (VOLUME_DECAY_ALPHA * db) + (1.0 - VOLUME_DECAY_ALPHA) * decibelAccum;

  auto color_hsv = hsv_pixel<24>();

  color_hsv.channel<channel_name::H>(green_hue + (red_hue - green_hue) *
                                                     (db / MIC_OVERLOAD_DB));
  color_hsv.channel<channel_name::S>(255);
  color_hsv.channel<channel_name::V>(255);

  auto color = rgb_pixel<16>();

  convert<hsv_pixel<24>, rgb_pixel<16>>(color_hsv, &color);

  char buffer[12];

  sprintf(buffer, "%.0f", decibelAccum);

  open_text_info oti;
  oti.font = &Telegrama;
  oti.scale = Telegrama.scale(64);
  oti.text = buffer;

  auto text_size = Telegrama.measure_text((ssize16)lcd.bounds().dimensions(),
                                          spoint16(0, 0), oti.text, oti.scale);

  auto text_pos = text_size.bounds().center((srect16)lcd.bounds());

  draw::filled_rectangle(lcd, lcd.bounds(), color);
  draw::text(lcd, text_pos, oti, lcd_color::black, color);
  vTaskDelay(5);
}