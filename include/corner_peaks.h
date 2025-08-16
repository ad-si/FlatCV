#ifndef FLATCV_AMALGAMATION
#pragma once
#include "1_types.h"
#endif

typedef struct {
  Point2D *points;
  uint32_t count;
} CornerPeaks;

CornerPeaks *corner_peaks(
  uint32_t width,
  uint32_t height,
  uint8_t const *data,
  uint32_t min_distance,
  double accuracy_thresh,
  double roundness_thresh
);
