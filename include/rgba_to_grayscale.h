#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

// Avoid using floating point arithmetic by pre-multiplying the weights
#define R_WEIGHT 76  // 0.299 * 256
#define G_WEIGHT 150 // 0.587 * 256
#define B_WEIGHT 30  // 0.114 * 256

uint8_t *fcv_rgba_to_grayscale(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data
);
