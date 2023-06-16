#include <Arduino.h>
#include <SPIFFS.h>
#define LCD_IMPLEMENTATION
#include <lcd_init.h>
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
using lcd_color = color<rgb_pixel<LCD_BIT_DEPTH>>;
using color32_t = color<rgba_pixel<32>>;

constexpr static const size_t lcd_buffer_size = 32*1024;
static uint8_t lcd_buffer1[lcd_buffer_size];
static uint8_t lcd_buffer2[lcd_buffer_size];

using screen_t = screen<LCD_WIDTH,LCD_HEIGHT,rgb_pixel<LCD_BIT_DEPTH>>;
using label_t = label<screen_t::control_surface_type>;
static screen_t main_screen(lcd_buffer_size,lcd_buffer1,lcd_buffer2);
static label_t db_label(main_screen);
static char text_buffer[12];
static Audio audio = Audio();

static double decibelAccum = 0;

constexpr static const uint8_t red_hue = 0;
constexpr static const uint8_t green_hue = 128 - 32;
static bool lcd_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                            esp_lcd_panel_io_event_data_t* edata, 
                            void* user_ctx) {
    main_screen.set_flush_complete();
    return true;
}
void uix_flush(const rect16& bounds, const void* bmp, void* state) {
  const_bitmap<screen_t::pixel_type,
              screen_t::palette_type> cbmp({bounds.width(),bounds.height()},bmp,main_screen.palette());
      lcd_panel_draw_bitmap(bounds.x1, 
                        bounds.y1, 
                        bounds.x2, 
                        bounds.y2,
                        (void*) bmp);

}

void setup() {
  Serial.begin(115200);
  
  // set up the UI
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
  lcd_panel_init(lcd_buffer_size,lcd_flush_ready);

  

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

  typename screen_t::pixel_type color;

  convert<hsv_pixel<24>, screen_t::pixel_type>(color_hsv, &color);
  main_screen.background_color(color);
  sprintf(text_buffer, "%.0f", decibelAccum);
  db_label.text(text_buffer);
  main_screen.update();
}