#ifndef FLATCV_AMALGAMATION
#pragma once
#include "1_types.h"
#endif

#include <stdint.h>
#include <stdbool.h>

uint8_t *fcv_watershed_segmentation(
  uint32_t width,
  uint32_t height,
  uint8_t const * const grayscale_data,
  Point2D* markers,
  uint32_t num_markers,
  bool create_boundaries
);
