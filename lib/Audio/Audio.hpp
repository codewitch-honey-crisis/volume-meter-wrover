#ifndef AUDIO_HPP
#define AUDIO_HPP

#include <Arduino.h>
#include <driver/i2s.h>

#include "sos-iir-filter.h"

#define PIN_LRCL 39
#define PIN_DOUT 34
#define PIN_BCLK 35

#define LEQ_PERIOD 0.1  // second(s)
#define WEIGHTING \
  A_weighting  // Also avaliable: 'C_weighting' or 'None' (Z_weighting)
#define LEQ_UNITS "LAeq"  // customize based on above weighting used
#define DB_UNITS "dBA"    // customize based on above weighting used
#define MIC_EQUALIZER \
  SPH0645LM4H_B_RB  // See below for defined IIR filters or set to 'None' to
                    // disable
#define MIC_OFFSET_DB \
  -18  // Default offset (sine-wave RMS vs. dBFS). Modify this value for
       // linear calibration
#define MIC_SENSITIVITY \
  -26  // dBFS value expected at MIC_REF_DB (Sensitivity value from datasheet)
#define MIC_REF_DB \
  94.0  // Value at which point sensitivity is specified in datasheet (dB)
#define MIC_OVERLOAD_DB 120.0  // dB - Acoustic overload point
#define MIC_NOISE_DB 10        // dB - Noise floor
#define MIC_BITS 24            // valid number of bits in I2S data
#define MIC_CONVERT(s) (s >> (SAMPLE_BITS - MIC_BITS))
#define MIC_TIMING_SHIFT \
  1  // Set to one to fix MSB timing for some microphones, i.e. SPH0645LM4H-x

#define I2S_WS PIN_LRCL
#define I2S_SCK PIN_BCLK
#define I2S_SD PIN_DOUT
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 48000  // Hz, fixed to design of IIR filters
#define SAMPLE_BITS 32     // bits
#define SAMPLE_T int32_t
#define SAMPLES_SHORT (SAMPLE_RATE / 8)  // ~125ms
#define SAMPLES_LEQ (SAMPLE_RATE * LEQ_PERIOD)
#define DMA_BANK_SIZE (SAMPLES_SHORT / 16)
#define DMA_BANKS 32

//
// FreeRTOS priority and stack size (in 32-bit words)
#define I2S_TASK_PRI 4
#define I2S_TASK_STACK 2048

struct sum_queue_t {
  // Sum of squares of mic samples, after Equalizer filter
  float sum_sqr_SPL;
  // Sum of squares of weighted mic samples
  float sum_sqr_weighted;
  // Debug only, FreeRTOS ticks we spent processing the I2S data
  uint32_t proc_ticks;
};

static void mic_i2s_reader_task(void* parameter);

class Audio {
  TaskHandle_t reader_task;

 public:
  void begin();
  double getDecibels();
  void end();
};

#endif