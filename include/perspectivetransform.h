#ifndef FLATCV_AMALGAMATION
#pragma once
#include "1_types.h"
#endif

Matrix3x3 *calculate_perspective_transform(
  Corners *src_corners,
  Corners *dst_corners
);

uint8_t *apply_matrix_3x3(
  int32_t in_width,
  int32_t in_height,
  uint8_t *in_data,
  int32_t out_width,
  int32_t out_height,
  Matrix3x3 *tmat
);

Corners detect_corners(const uint8_t *image, int32_t width, int32_t height);
