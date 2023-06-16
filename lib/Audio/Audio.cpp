#include "Audio.hpp"

SOS_IIR_Filter DC_BLOCKER = {gain : 1.0, sos : {{-1.0, 0.0, +0.9992, 0}}};

SOS_IIR_Filter SPH0645LM4H_B_RB = {
  gain : 1.00123377961525,
  sos : {                          // Second-Order Sections {b1, b2, -a1, -a2}
         {-1.0, 0.0, +0.9992, 0},  // DC blocker, a1 = -0.9992
         {-1.988897663539382, +0.988928479008099, +1.993853376183491,
          -0.993862821429572}}
};

SOS_IIR_Filter A_weighting = {
  gain : 0.169994948147430,
  sos : {// Second-Order Sections {b1, b2, -a1, -a2}
         {-2.00026996133106, +1.00027056142719, -1.060868438509278,
          -0.163987445885926},
         {+4.35912384203144, +3.09120265783884, +1.208419926363593,
          -0.273166998428332},
         {-0.70930303489759, -0.29071868393580, +1.982242159753048,
          -0.982298594928989}}
};

SOS_IIR_Filter C_weighting = {
  gain : -0.491647169337140,
  sos : {{+1.4604385758204708, +0.5275070373815286, +1.9946144559930252,
          -0.9946217070140883},
         {+0.2376222404939509, +0.0140411206016894, -1.3396585608422749,
          -0.4421457807694559},
         {-2.0000000000000000, +1.0000000000000000, +0.3775800047420818,
          -0.0356365756680430}}
};

static double MIC_REF_AMPL =
    pow(10, double(MIC_SENSITIVITY) / 20) * ((1 << (MIC_BITS - 1)) - 1);

static sum_queue_t q;
static uint32_t Leq_samples = 0;
static double Leq_sum_sqr = 0;
static double Leq_dB = 0;
static QueueHandle_t samples_queue;
static float samples[SAMPLES_SHORT] __attribute__((aligned(4)));

void Audio::begin() {
  samples_queue = xQueueCreate(8, sizeof(sum_queue_t));

  xTaskCreate(mic_i2s_reader_task, "Mic I2S Reader", I2S_TASK_STACK, NULL,
              I2S_TASK_PRI, &reader_task);
}

double Audio::getDecibels() {
  if (xQueueReceive(samples_queue, &q, portMAX_DELAY)) {
    double short_RMS = sqrt(double(q.sum_sqr_SPL) / SAMPLES_SHORT);
    double short_SPL_dB =
        MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(short_RMS / MIC_REF_AMPL);

    if (short_SPL_dB > MIC_OVERLOAD_DB) {
      Leq_sum_sqr = INFINITY;
    } else if (isnan(short_SPL_dB) || (short_SPL_dB < MIC_NOISE_DB)) {
      Leq_sum_sqr = -INFINITY;
    }

    // Accumulate Leq sum
    Leq_sum_sqr += q.sum_sqr_weighted;
    Leq_samples += SAMPLES_SHORT;

    // When we gather enough samples, calculate new Leq value
    if (Leq_samples >= SAMPLE_RATE * LEQ_PERIOD) {
      double Leq_RMS = sqrt(Leq_sum_sqr / Leq_samples);
      Leq_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(Leq_RMS / MIC_REF_AMPL);
      Leq_sum_sqr = 0;
      Leq_samples = 0;

      return Leq_dB;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

void Audio::end() { vTaskDelete(reader_task); }

static void mic_i2s_reader_task(void* parameter) {
  // Setup I2S to sample mono channel for SAMPLE_RATE * SAMPLE_BITS
  // NOTE: Recent update to Arduino_esp32 (1.0.2 -> 1.0.3)
  //       seems to have swapped ONLY_LEFT and ONLY_RIGHT channels
  const i2s_config_t i2s_config = {
    mode : i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    sample_rate : SAMPLE_RATE,
    bits_per_sample : i2s_bits_per_sample_t(SAMPLE_BITS),
    channel_format : I2S_CHANNEL_FMT_ONLY_LEFT,
    communication_format : i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    intr_alloc_flags : ESP_INTR_FLAG_LEVEL1,
    dma_buf_count : DMA_BANKS,
    dma_buf_len : DMA_BANK_SIZE,
    use_apll : true,
    tx_desc_auto_clear : false,
    fixed_mclk : 0
  };
  // I2S pin mapping
  const i2s_pin_config_t pin_config = {
    bck_io_num : I2S_SCK,
    ws_io_num : I2S_WS,
    data_out_num : -1,  // not used
    data_in_num : I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

  i2s_set_pin(I2S_PORT, &pin_config);

  // Discard first block, microphone may have startup time (i.e. INMP441 up to
  // 83ms)
  size_t bytes_read = 0;
  i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read,
           portMAX_DELAY);

  while (true) {
    // Block and wait for microphone values from I2S
    //
    // Data is moved from DMA buffers to our 'samples' buffer by the driver ISR
    // and when there is requested ammount of data, task is unblocked
    //
    // Note: i2s_read does not care it is writing in float[] buffer, it will
    // write
    //       integer values to the given address, as received from the hardware
    //       peripheral.
    i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(SAMPLE_T), &bytes_read,
             portMAX_DELAY);

    TickType_t start_tick = xTaskGetTickCount();

    // Convert (including shifting) integer microphone values to floats,
    // using the same buffer (assumed sample size is same as size of float),
    // to save a bit of memory
    SAMPLE_T* int_samples = (SAMPLE_T*)&samples;
    for (int i = 0; i < SAMPLES_SHORT; i++)
      samples[i] = MIC_CONVERT(int_samples[i]);

    sum_queue_t q;
    // Apply equalization and calculate Z-weighted sum of squares,
    // writes filtered samples back to the same buffer.
    q.sum_sqr_SPL = MIC_EQUALIZER.filter(samples, samples, SAMPLES_SHORT);

    // Apply weighting and calucate weigthed sum of squares
    q.sum_sqr_weighted = WEIGHTING.filter(samples, samples, SAMPLES_SHORT);

    // Debug only. Ticks we spent filtering and summing block of I2S data
    q.proc_ticks = xTaskGetTickCount() - start_tick;

    // Send the sums to FreeRTOS queue where main task will pick them up
    // and further calcualte decibel values (division, logarithms, etc...)
    xQueueSend(samples_queue, &q, portMAX_DELAY);
  }
}