#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

unsigned char *watershed_segmentation(
  unsigned int width,
  unsigned int height,
  unsigned char const * const grayscale_data,
  Point2D* markers,
  unsigned int num_markers,
  bool create_boundaries
);
