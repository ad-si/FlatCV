#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

/**
 * Trim border pixels that have the same color.
 *
 * @param width Pointer to width of the image (will be updated).
 * @param height Pointer to height of the image (will be updated).
 * @param channels Number of channels in the image.
 * @param data Pointer to the pixel data.
 * @return Pointer to the new trimmed image data.
 */
uint8_t *fcv_trim(
  int32_t *width,
  int32_t *height,
  uint32_t channels,
  uint8_t const *const data
);

/**
 * Trim border pixels that are within a threshold of the reference color.
 * This is useful for images with JPEG artifacts or slight vignetting.
 *
 * @param width Pointer to width of the image (will be updated).
 * @param height Pointer to height of the image (will be updated).
 * @param channels Number of channels in the image.
 * @param data Pointer to the pixel data.
 * @param threshold_percent Tolerance percentage (0-100). A value of 2 means
 *        pixels within 2% (≈5 units) of the reference color will be trimmed.
 * @return Pointer to the new trimmed image data.
 */
uint8_t *fcv_trim_threshold(
  int32_t *width,
  int32_t *height,
  uint32_t channels,
  uint8_t const *const data,
  double threshold_percent
);
