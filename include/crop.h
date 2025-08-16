#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

uint8_t *fcv_crop(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  uint8_t const * const data,
  uint32_t x,
  uint32_t y,
  uint32_t new_width,
  uint32_t new_height
);
