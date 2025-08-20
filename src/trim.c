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
  if (!data || !width || !height || *width <= 0 || *height <= 0) {
    return NULL;
  }

  uint32_t w = (uint32_t)*width;
  uint32_t h = (uint32_t)*height;
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
    uint8_t *result = malloc(w * h * channels);
    if (result) {
      memcpy(result, data, w * h * channels);
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
