#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

uint8_t *fcv_sobel_edge_detection(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  uint8_t const * const data
);
