#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "crop.h"
#include "trim.h"
#else
#include "flatcv.h"
#endif

static bool
pixels_match(const uint8_t *pixel1, const uint8_t *pixel2, uint32_t channels) {
  for (uint32_t c = 0; c < channels; c++) {
    if (pixel1[c] != pixel2[c]) {
      return false;
    }
  }
  return true;
}

/**
 * Check if two pixels match within a given tolerance.
 *
 * @param pixel1 First pixel data.
 * @param pixel2 Second pixel data.
 * @param channels Number of channels.
 * @param tolerance Maximum allowed difference per channel (0-255).
 * @return true if all channels are within tolerance, false otherwise.
 */
static bool pixels_match_threshold(
  const uint8_t *pixel1,
  const uint8_t *pixel2,
  uint32_t channels,
  uint8_t tolerance
) {
  for (uint32_t c = 0; c < channels; c++) {
    int32_t diff = (int32_t)pixel1[c] - (int32_t)pixel2[c];
    if (diff < 0) {
      diff = -diff;
    }
    if ((uint8_t)diff > tolerance) {
      return false;
    }
  }
  return true;
}

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
) {
  if (!data || !width || !height || *width <= 0 || *height <= 0 ||
      channels == 0) {
    return NULL;
  }

  uint32_t w = (uint32_t)*width;
  uint32_t h = (uint32_t)*height;

  // Check for potential overflow in pixel index calculations
  if (w > SIZE_MAX / h || (size_t)w * h > SIZE_MAX / channels) {
    return NULL;
  }
  uint32_t left = 0;
  uint32_t top = 0;
  uint32_t right = w;
  uint32_t bottom = h;

  // Trim from the left
  while (left < right) {
    const uint8_t *ref_pixel = &data[(top * w + left) * channels];
    bool can_trim = true;

    // Check if the entire left column has the same color as the reference pixel
    for (uint32_t y = top; y < bottom; y++) {
      const uint8_t *pixel = &data[(y * w + left) * channels];
      if (!pixels_match(pixel, ref_pixel, channels)) {
        can_trim = false;
        break;
      }
    }

    if (can_trim && right - left > 1) {
      left++;
    }
    else {
      break;
    }
  }

  // Trim from the right
  while (left < right) {
    const uint8_t *ref_pixel = &data[(top * w + (right - 1)) * channels];
    bool can_trim = true;

    // Check if the entire right column has the same color as the reference
    // pixel
    for (uint32_t y = top; y < bottom; y++) {
      const uint8_t *pixel = &data[(y * w + (right - 1)) * channels];
      if (!pixels_match(pixel, ref_pixel, channels)) {
        can_trim = false;
        break;
      }
    }

    if (can_trim && right - left > 1) {
      right--;
    }
    else {
      break;
    }
  }

  // Trim from the top
  while (top < bottom && bottom - top > 1) {
    const uint8_t *ref_pixel = &data[(top * w + left) * channels];
    bool can_trim = true;

    // Check if the entire top row has the same color as the reference pixel
    for (uint32_t x = left; x < right; x++) {
      const uint8_t *pixel = &data[(top * w + x) * channels];
      if (!pixels_match(pixel, ref_pixel, channels)) {
        can_trim = false;
        break;
      }
    }

    if (can_trim) {
      top++;
    }
    else {
      break;
    }
  }

  // Trim from the bottom
  while (top < bottom && bottom - top > 1) {
    const uint8_t *ref_pixel = &data[((bottom - 1) * w + left) * channels];
    bool can_trim = true;

    // Check if the entire bottom row has the same color as the reference pixel
    for (uint32_t x = left; x < right; x++) {
      const uint8_t *pixel = &data[((bottom - 1) * w + x) * channels];
      if (!pixels_match(pixel, ref_pixel, channels)) {
        can_trim = false;
        break;
      }
    }

    if (can_trim) {
      bottom--;
    }
    else {
      break;
    }
  }

  // Calculate new dimensions
  uint32_t new_width = right - left;
  uint32_t new_height = bottom - top;

  // If no trimming was done, return a copy
  if (left == 0 && top == 0 && right == w && bottom == h) {
    size_t alloc_size = (size_t)w * h * channels;
    uint8_t *result = malloc(alloc_size);
    if (result) {
      memcpy(result, data, alloc_size);
    }
    return result;
  }

  // Use existing crop function to extract the trimmed region
  uint8_t *trimmed_data =
    fcv_crop(w, h, channels, data, left, top, new_width, new_height);

  // Update dimensions
  *width = (int32_t)new_width;
  *height = (int32_t)new_height;

  return trimmed_data;
}

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
) {
  if (!data || !width || !height || *width <= 0 || *height <= 0 ||
      channels == 0) {
    return NULL;
  }

  // Validate threshold_percent - reject invalid values
  if (!isfinite(threshold_percent)) {
    return NULL;
  }

  // Clamp threshold to valid range and convert to absolute value (0-255)
  if (threshold_percent < 0.0) {
    threshold_percent = 0.0;
  }
  if (threshold_percent > 100.0) {
    threshold_percent = 100.0;
  }
  uint8_t tolerance = (uint8_t)(threshold_percent * 255.0 / 100.0);

  // If tolerance is 0, use the exact match version
  if (tolerance == 0) {
    return fcv_trim(width, height, channels, data);
  }

  uint32_t w = (uint32_t)*width;
  uint32_t h = (uint32_t)*height;

  // Check for potential overflow in pixel index calculations
  if (w > SIZE_MAX / h || (size_t)w * h > SIZE_MAX / channels) {
    return NULL;
  }
  uint32_t left = 0;
  uint32_t top = 0;
  uint32_t right = w;
  uint32_t bottom = h;

  // Trim from the left
  while (left < right) {
    const uint8_t *ref_pixel = &data[(top * w + left) * channels];
    bool can_trim = true;

    // Check if the entire left column matches within threshold
    for (uint32_t y = top; y < bottom; y++) {
      const uint8_t *pixel = &data[(y * w + left) * channels];
      if (!pixels_match_threshold(pixel, ref_pixel, channels, tolerance)) {
        can_trim = false;
        break;
      }
    }

    if (can_trim && right - left > 1) {
      left++;
    }
    else {
      break;
    }
  }

  // Trim from the right
  while (left < right) {
    const uint8_t *ref_pixel = &data[(top * w + (right - 1)) * channels];
    bool can_trim = true;

    // Check if the entire right column matches within threshold
    for (uint32_t y = top; y < bottom; y++) {
      const uint8_t *pixel = &data[(y * w + (right - 1)) * channels];
      if (!pixels_match_threshold(pixel, ref_pixel, channels, tolerance)) {
        can_trim = false;
        break;
      }
    }

    if (can_trim && right - left > 1) {
      right--;
    }
    else {
      break;
    }
  }

  // Trim from the top
  while (top < bottom && bottom - top > 1) {
    const uint8_t *ref_pixel = &data[(top * w + left) * channels];
    bool can_trim = true;

    // Check if the entire top row matches within threshold
    for (uint32_t x = left; x < right; x++) {
      const uint8_t *pixel = &data[(top * w + x) * channels];
      if (!pixels_match_threshold(pixel, ref_pixel, channels, tolerance)) {
        can_trim = false;
        break;
      }
    }

    if (can_trim) {
      top++;
    }
    else {
      break;
    }
  }

  // Trim from the bottom
  while (top < bottom && bottom - top > 1) {
    const uint8_t *ref_pixel = &data[((bottom - 1) * w + left) * channels];
    bool can_trim = true;

    // Check if the entire bottom row matches within threshold
    for (uint32_t x = left; x < right; x++) {
      const uint8_t *pixel = &data[((bottom - 1) * w + x) * channels];
      if (!pixels_match_threshold(pixel, ref_pixel, channels, tolerance)) {
        can_trim = false;
        break;
      }
    }

    if (can_trim) {
      bottom--;
    }
    else {
      break;
    }
  }

  // Calculate new dimensions
  uint32_t new_width = right - left;
  uint32_t new_height = bottom - top;

  // If no trimming was done, return a copy
  if (left == 0 && top == 0 && right == w && bottom == h) {
    size_t alloc_size = (size_t)w * h * channels;
    uint8_t *result = malloc(alloc_size);
    if (result) {
      memcpy(result, data, alloc_size);
    }
    return result;
  }

  // Use existing crop function to extract the trimmed region
  uint8_t *trimmed_data =
    fcv_crop(w, h, channels, data, left, top, new_width, new_height);

  // Update dimensions
  *width = (int32_t)new_width;
  *height = (int32_t)new_height;

  return trimmed_data;
}
