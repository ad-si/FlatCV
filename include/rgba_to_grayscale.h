#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

// Avoid using floating point arithmetic by pre-multiplying the weights
#define R_WEIGHT 76  // 0.299 * 256
#define G_WEIGHT 150 // 0.587 * 256
#define B_WEIGHT 30  // 0.114 * 256

unsigned char *rgba_to_grayscale(
  unsigned int width,
  unsigned int height,
  unsigned char const * const data
);
