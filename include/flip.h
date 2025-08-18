#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

uint8_t *fcv_flip_x(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data
);

uint8_t *fcv_flip_y(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data
);