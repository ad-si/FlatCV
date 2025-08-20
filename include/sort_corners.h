#ifndef FLATCV_AMALGAMATION
#pragma once
#include "1_types.h"
#endif

Corners sort_corners(
  uint32_t width, uint32_t height,
  uint32_t out_width, uint32_t out_height,
  Point2D *corners, uint32_t num_corners,
  Point2D *result
);
