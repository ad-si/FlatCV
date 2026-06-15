#ifndef FLATCV_AMALGAMATION
#pragma once
#include "1_types.h"
#endif

#include <stdint.h>

uint8_t *fcv_flood_fill(
  uint32_t width,
  uint32_t height,
  uint8_t const *const grayscale_data,
  Point2D const *marker,
  uint8_t threshold,
  uint8_t new_value
);
