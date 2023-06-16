#include "sos-iir-filter.h"
No_IIR_Filter None;
__asm__(
    //
    // ESP32 implementation of IIR Second-Order Section filter
    // Assumes a0 and b0 coefficients are one (1.0)
    //
    // float* a2 = input;
    // float* a3 = output;
    // int    a4 = len;
    // float* a5 = coeffs;
    // float* a6 = w;
    // float  a7 = gain;
    //
    ".text                    \n"
    ".align  4                \n"
    ".global sos_filter_f32   \n"
    ".type   sos_filter_f32,@function\n"
    "sos_filter_f32:          \n"
    "  entry   a1, 16         \n"
    "  lsi     f0, a5, 0      \n"  // float f0 = coeffs.b1;
    "  lsi     f1, a5, 4      \n"  // float f1 = coeffs.b2;
    "  lsi     f2, a5, 8      \n"  // float f2 = coeffs.a1;
    "  lsi     f3, a5, 12     \n"  // float f3 = coeffs.a2;
    "  lsi     f4, a6, 0      \n"  // float f4 = w[0];
    "  lsi     f5, a6, 4      \n"  // float f5 = w[1];
    "  loopnez a4, 1f         \n"  // for (; len>0; len--) {
    "    lsip    f6, a2, 4    \n"  //   float f6 = *input++;
    "    madd.s  f6, f2, f4   \n"  //   f6 += f2 * f4; // coeffs.a1 * w0
    "    madd.s  f6, f3, f5   \n"  //   f6 += f3 * f5; // coeffs.a2 * w1
    "    mov.s   f7, f6       \n"  //   f7 = f6; // b0 assumed 1.0
    "    madd.s  f7, f0, f4   \n"  //   f7 += f0 * f4; // coeffs.b1 * w0
    "    madd.s  f7, f1, f5   \n"  //   f7 += f1 * f5; // coeffs.b2 * w1 ->
                                   //   result
    "    ssip    f7, a3, 4    \n"  //   *output++ = f7;
    "    mov.s   f5, f4       \n"  //   f5 = f4; // w1 = w0
    "    mov.s   f4, f6       \n"  //   f4 = f6; // w0 = f6
    "  1:                     \n"  // }
    "  ssi     f4, a6, 0      \n"  // w[0] = f4;
    "  ssi     f5, a6, 4      \n"  // w[1] = f5;
    "  movi.n   a2, 0         \n"  // return 0;
    "  retw.n                 \n");

__asm__(
    //
    // ESP32 implementation of IIR Second-Order section filter with applied
    // gain. Assumes a0 and b0 coefficients are one (1.0) Returns sum of squares
    // of filtered samples
    //
    // float* a2 = input;
    // float* a3 = output;
    // int    a4 = len;
    // float* a5 = coeffs;
    // float* a6 = w;
    // float  a7 = gain;
    //
    ".text                    \n"
    ".align  4                \n"
    ".global sos_filter_sum_sqr_f32 \n"
    ".type   sos_filter_sum_sqr_f32,@function \n"
    "sos_filter_sum_sqr_f32:  \n"
    "  entry   a1, 16         \n"
    "  lsi     f0, a5, 0      \n"  // float f0 = coeffs.b1;
    "  lsi     f1, a5, 4      \n"  // float f1 = coeffs.b2;
    "  lsi     f2, a5, 8      \n"  // float f2 = coeffs.a1;
    "  lsi     f3, a5, 12     \n"  // float f3 = coeffs.a2;
    "  lsi     f4, a6, 0      \n"  // float f4 = w[0];
    "  lsi     f5, a6, 4      \n"  // float f5 = w[1];
    "  wfr     f6, a7         \n"  // float f6 = gain;
    "  const.s f10, 0         \n"  // float sum_sqr = 0;
    "  loopnez a4, 1f         \n"  // for (; len>0; len--) {
    "    lsip    f7, a2, 4    \n"  //   float f7 = *input++;
    "    madd.s  f7, f2, f4   \n"  //   f7 += f2 * f4; // coeffs.a1 * w0
    "    madd.s  f7, f3, f5   \n"  //   f7 += f3 * f5; // coeffs.a2 * w1;
    "    mov.s   f8, f7       \n"  //   f8 = f7; // b0 assumed 1.0
    "    madd.s  f8, f0, f4   \n"  //   f8 += f0 * f4; // coeffs.b1 * w0;
    "    madd.s  f8, f1, f5   \n"  //   f8 += f1 * f5; // coeffs.b2 * w1;
    "    mul.s   f9, f8, f6   \n"  //   f9 = f8 * f6;  // f8 * gain -> result
    "    ssip    f9, a3, 4    \n"  //   *output++ = f9;
    "    mov.s   f5, f4       \n"  //   f5 = f4; // w1 = w0
    "    mov.s   f4, f7       \n"  //   f4 = f7; // w0 = f7;
    "    madd.s  f10, f9, f9  \n"  //   f10 += f9 * f9; // sum_sqr += f9 * f9;
    "  1:                     \n"  // }
    "  ssi     f4, a6, 0      \n"  // w[0] = f4;
    "  ssi     f5, a6, 4      \n"  // w[1] = f5;
    "  rfr     a2, f10        \n"  // return sum_sqr;
    "  retw.n                 \n"  //
);