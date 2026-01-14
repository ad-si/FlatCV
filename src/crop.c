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
#else
#include "flatcv.h"
#endif

/**
 * Crop an image.
 *
 * @param width Width of the original image.
 * @param height Height of the original image.
 * @param channels Number of channels in the image.
 * @param data Pointer to the pixel data.
 * @param x The x-coordinate of the top-left corner of the crop area.
 * @param y The y-coordinate of the top-left corner of the crop area.
 * @param new_width The width of the crop area.
 * @param new_height The height of the crop area.
 * @return Pointer to the new cropped image data.
 */
uint8_t *fcv_crop(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  uint8_t const *const data,
  uint32_t x,
  uint32_t y,
  uint32_t new_width,
  uint32_t new_height
) {
  if (!data) {
    return NULL;
  }

  if (width == 0 || height == 0 || channels == 0 || new_width == 0 ||
      new_height == 0) {
    return NULL;
  }

  // Check for overflow in x + new_width and y + new_height
  if (x > UINT32_MAX - new_width || y > UINT32_MAX - new_height) {
    return NULL;
  }

  if (x + new_width > width || y + new_height > height) {
    fprintf(stderr, "Crop area is outside the original image bounds.\n");
    return NULL;
  }

  // Check for overflow in memory allocation: new_width * new_height * channels
  if (new_width > SIZE_MAX / new_height) {
    return NULL;
  }
  size_t num_pixels = (size_t)new_width * new_height;
  if (num_pixels > SIZE_MAX / channels) {
    return NULL;
  }
  size_t alloc_size = num_pixels * channels;

  uint8_t *cropped_data = malloc(alloc_size);
  if (!cropped_data) {
    fprintf(stderr, "Memory allocation failed for cropped image.\n");
    return NULL;
  }

  size_t row_bytes = (size_t)new_width * channels;
  for (uint32_t i = 0; i < new_height; ++i) {
    size_t src_index = ((size_t)(y + i) * width + x) * channels;
    size_t dst_index = (size_t)i * row_bytes;
    memcpy(cropped_data + dst_index, data + src_index, row_bytes);
  }

  return cropped_data;
}
