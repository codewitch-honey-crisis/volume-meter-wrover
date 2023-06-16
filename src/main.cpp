#include <Arduino.h>
#include <SPIFFS.h>

#include <Audio.hpp>
#include <uix.hpp>
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
#define LCD_ROTATION 1
#define LCD_HOST HSPI

#define PIN_NUM_CS 22
#define PIN_NUM_MOSI 23
#define PIN_NUM_MISO 25
#define PIN_NUM_CLK 19
#define PIN_NUM_DC 21
#define PIN_NUM_RST 18
#define PIN_NUM_BCKL 5

using namespace arduino;
using namespace gfx;
using namespace uix;
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
using color32_t = color<rgba_pixel<32>>;
lcd_type lcd;
constexpr static const size_t lcd_buffer_size = 32*1024;
static uint8_t lcd_buffer1[lcd_buffer_size];
static uint8_t lcd_buffer2[lcd_buffer_size];

using screen_t = screen<LCD_WIDTH,LCD_HEIGHT,typename lcd_type::pixel_type>;
using label_t = label<screen_t::control_surface_type>;
static screen_t main_screen(lcd_buffer_size,lcd_buffer1,lcd_buffer2);
static label_t db_label(main_screen);
static char text_buffer[12];
static Audio audio = Audio();

static double decibelAccum = 0;

constexpr static const uint8_t red_hue = 0;
constexpr static const uint8_t green_hue = 128 - 32;

void uix_wait(void* state) {
  draw::wait_all_async(lcd);
}
void uix_flush(const rect16& bounds, const void* bmp, void* state) {
  const_bitmap<screen_t::pixel_type,
              screen_t::palette_type> cbmp({bounds.width(),bounds.height()},bmp,main_screen.palette());
  draw::bitmap_async(lcd,lcd.bounds(),cbmp,cbmp.bounds());
}

void setup() {
  Serial.begin(115200);
  // override backlight control (active LOW)
  pinMode(PIN_NUM_BCKL, OUTPUT);
  digitalWrite(PIN_NUM_BCKL, LOW);

  // set up the UI
  main_screen.on_flush_callback(uix_flush);
  main_screen.wait_flush_callback(uix_wait);
  main_screen.background_color(lcd_color::black);
  db_label.text_justify(uix_justify::center);
  db_label.bounds(main_screen.bounds());
  db_label.text_open_font(&Telegrama);
  db_label.text_line_height(64);
  db_label.background_color(color32_t::black);
  db_label.border_color(db_label.background_color());
  db_label.text_color(color32_t::white);
  main_screen.register_control(db_label);
  // set up display
  lcd.initialize();

  

  // start microphone task loop
  //audio.begin();
}

void loop() {
  // check if we have audio data
  //double db = audio.getDecibels();
  double db=1;
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

  typename screen_t::pixel_type color;

  convert<hsv_pixel<24>, screen_t::pixel_type>(color_hsv, &color);
  main_screen.background_color(color);
  sprintf(text_buffer, "%.0f", decibelAccum);
  db_label.text(text_buffer);
  main_screen.update();
}