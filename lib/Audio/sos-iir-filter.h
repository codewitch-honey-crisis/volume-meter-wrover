/*
 * ESP32 Second-Order Sections IIR Filter implementation
 *
 * (c)2019 Ivan Kostoski
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SOS_IIR_FILTER_H
#define SOS_IIR_FILTER_H

#include <Arduino.h>
#include <stdint.h>

struct SOS_Coefficients {
  float b1;
  float b2;
  float a1;
  float a2;
};

struct SOS_Delay_State {
  float w0 = 0;
  float w1 = 0;
};

extern "C" {
int sos_filter_f32(float *input, float *output, int len,
                   const SOS_Coefficients &coeffs, SOS_Delay_State &w);
}

extern "C" {
float sos_filter_sum_sqr_f32(float *input, float *output, int len,
                             const SOS_Coefficients &coeffs, SOS_Delay_State &w,
                             float gain);
}

/**
 * Envelops above asm functions into C++ class
 */
struct SOS_IIR_Filter {
  const int num_sos;
  const float gain;
  SOS_Coefficients *sos = NULL;
  SOS_Delay_State *w = NULL;

  // Dynamic constructor
  SOS_IIR_Filter(size_t num_sos, const float gain,
                 const SOS_Coefficients _sos[] = NULL)
      : num_sos(num_sos), gain(gain) {
    if (num_sos > 0) {
      sos = new SOS_Coefficients[num_sos];
      if ((sos != NULL) && (_sos != NULL))
        memcpy(sos, _sos, num_sos * sizeof(SOS_Coefficients));
      w = new SOS_Delay_State[num_sos]();
    }
  };

  // Template constructor for const filter declaration
  template <size_t Array_Size>
  SOS_IIR_Filter(const float gain, const SOS_Coefficients (&sos)[Array_Size])
      : SOS_IIR_Filter(Array_Size, gain, sos){};

  /**
   * Apply defined IIR Filter to input array of floats, write filtered values to
   * output, and return sum of squares of all filtered values
   */
  inline float filter(float *input, float *output, size_t len) {
    if ((num_sos < 1) || (sos == NULL) || (w == NULL)) return 0;
    float *source = input;
    // Apply all but last Second-Order-Section
    for (int i = 0; i < (num_sos - 1); i++) {
      sos_filter_f32(source, output, len, sos[i], w[i]);
      source = output;
    }
    // Apply last SOS with gain and return the sum of squares of all samples
    return sos_filter_sum_sqr_f32(source, output, len, sos[num_sos - 1],
                                  w[num_sos - 1], gain);
  }

  ~SOS_IIR_Filter() {
    if (w != NULL) delete[] w;
    if (sos != NULL) delete[] sos;
  }
};

//
// For testing only
//
struct No_IIR_Filter {
  const int num_sos = 0;
  const float gain = 1.0;

  No_IIR_Filter(){};

  inline float filter(float *input, float *output, size_t len) {
    float sum_sqr = 0;
    float s;
    for (int i = 0; i < len; i++) {
      s = input[i];
      sum_sqr += s * s;
    }
    if (input != output) {
      for (int i = 0; i < len; i++) output[i] = input[i];
    }
    return sum_sqr;
  };
};

#endif  // SOS_IIR_FILTER_H