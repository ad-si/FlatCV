#ifndef FLATCV_AMALGAMATION
#pragma once
#include "1_types.h"
#endif

typedef struct {
  Point2D *points;
  unsigned int count;
} CornerPeaks;

CornerPeaks *corner_peaks(
  unsigned int width,
  unsigned int height,
  unsigned char const *data,
  unsigned int min_distance,
  double accuracy_thresh,
  double roundness_thresh
);
