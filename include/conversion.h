#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdbool.h>
#include <stdint.h>

uint8_t *fcv_apply_gaussian_blur(
  uint32_t width,
  uint32_t height,
  double radius,
  uint8_t const * const data
);

uint8_t *fcv_grayscale(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data
);

uint8_t *fcv_grayscale_stretch(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data
);

void fcv_apply_global_threshold(
  uint32_t img_length,
  uint8_t *data,
  uint8_t threshold
);

uint8_t *fcv_otsu_threshold_rgba(
  uint32_t width,
  uint32_t height,
  bool use_double_threshold,
  uint8_t const * const data
);

uint8_t *fcv_bw_smart(
  uint32_t width,
  uint32_t height,
  bool use_double_threshold,
  uint8_t const * const data
);

uint8_t *fcv_resize(
  uint32_t width,
  uint32_t height,
  double scale_x,
  double scale_y,
  uint32_t* out_width,
  uint32_t* out_height,
  uint8_t const * const data
);
