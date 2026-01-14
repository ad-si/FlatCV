#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

uint8_t *fcv_binary_dilation_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
);

uint8_t *fcv_binary_erosion_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
);

uint8_t *fcv_binary_closing_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
);

uint8_t *fcv_binary_opening_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
);
