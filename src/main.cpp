
#ifdef T_QT_PRO
#define LCD_SPI_HOST    SPI3_HOST
#define LCD_BCKL_ON_LEVEL 1
#define LCD_PIN_NUM_MOSI 2
#define LCD_PIN_NUM_CLK 3
#define LCD_PIN_NUM_CS 5
#define LCD_PIN_NUM_DC 6
#define LCD_PIN_NUM_RST 1
#define LCD_PIN_NUM_BCKL 10
#define LCD_HRES 128
#define LCD_VRES 128
#define LCD_ROTATION 0
#endif // T_QT_PRO
#ifdef ESP_WROVER_KIT
#define LCD_BCKL_ON_LEVEL 0
#define LCD_SPI_HOST    HSPI
#define LCD_PIN_NUM_MISO 25
#define LCD_PIN_NUM_MOSI 23
#define LCD_PIN_NUM_CLK  19
#define LCD_PIN_NUM_CS   22
#define LCD_PIN_NUM_DC   21
#define LCD_PIN_NUM_RST  18
#define LCD_PIN_NUM_BCKL 5
#define LCD_HRES 320
#define LCD_VRES 240
#define LCD_GAP_X 0
#define LCD_GAP_Y 0
#define LCD_ROTATION 1
#endif // ESP_WROVER_KIT
#include <Arduino.h>
#include <SPIFFS.h>
//#define LCD_IMPLEMENTATION
//#include <lcd_init.h>
#include <tft_spi.hpp>
#include <ili9341.hpp>
#include <Audio.hpp>
#include <uix.hpp>
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

using namespace gfx;
using namespace uix;
using namespace arduino;
using lcd_color = color<rgb_pixel<16>>;
using color32_t = color<rgba_pixel<32>>;

using bus_t = tft_spi_ex<LCD_SPI_HOST,LCD_PIN_NUM_CS,LCD_PIN_NUM_MOSI,LCD_PIN_NUM_MISO,LCD_PIN_NUM_CLK,SPI_MODE0,LCD_PIN_NUM_MISO!=-1,LCD_HRES*LCD_VRES*2+1>;
using lcd_t = ili9341<LCD_PIN_NUM_DC,LCD_PIN_NUM_RST,LCD_PIN_NUM_BCKL,bus_t,LCD_ROTATION,false,400,200>;

lcd_t lcd;

constexpr static const size_t lcd_buffer_size = 32*1024;
static uint8_t lcd_buffer1[lcd_buffer_size];
static uint8_t lcd_buffer2[lcd_buffer_size];

using screen_t = screen<LCD_HRES,LCD_VRES,rgb_pixel<16>>;
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
    draw::bitmap_async(lcd,bounds,cbmp,cbmp.bounds());
}

void setup() {
  Serial.begin(115200);
  lcd.initialize();
  // set up the UI
  main_screen.wait_flush_callback(uix_wait);
  main_screen.on_flush_callback(uix_flush);
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
  //lcd_panel_init(lcd_buffer_size,lcd_flush_ready);

  

  // start microphone task loop
  //audio.begin();
}

void loop() {
  // check if we have audio data
  double db = rand()%10;//audio.getDecibels();
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

  rgba_pixel<32> color;

  convert(color_hsv, &color);
  db_label.background_color(color);
  sprintf(text_buffer, "%.0f", decibelAccum);
  db_label.text(text_buffer);
  main_screen.update();
}