#ifndef FLATCV_AMALGAMATION
#pragma once
#include "1_types.h"
#endif

Matrix3x3* calculate_perspective_transform(
  Corners* src_corners,
  Corners* dst_corners
);

unsigned char * apply_matrix_3x3(
  int in_width,
  int in_height,
  unsigned char* in_data,
  int out_width,
  int out_height,
  Matrix3x3* tmat
);

Corners detect_corners(const unsigned char *image, int width, int height);
