#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "rotate.h"
#else
#include "flatcv.h"
#endif

/**
 * Rotate an image 90 degrees clockwise.
 */
uint8_t *
fcv_rotate_90_cw(uint32_t width, uint32_t height, uint8_t const *const data) {
  if (!data || width == 0 || height == 0) {
    return NULL;
  }

  // Check for overflow: width * height * 4
  if (width > SIZE_MAX / height) {
    return NULL;
  }
  size_t num_pixels = (size_t)width * height;
  if (num_pixels > SIZE_MAX / 4) {
    return NULL;
  }
  size_t img_length_byte = num_pixels * 4;

  uint8_t *rotated_data = malloc(img_length_byte);

  if (!rotated_data) {
    return NULL;
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      size_t src_index = ((size_t)y * width + x) * 4;
      size_t dst_index = ((size_t)x * height + (height - 1 - y)) * 4;

      rotated_data[dst_index] = data[src_index];
      rotated_data[dst_index + 1] = data[src_index + 1];
      rotated_data[dst_index + 2] = data[src_index + 2];
      rotated_data[dst_index + 3] = data[src_index + 3];
    }
  }

  return rotated_data;
}

/**
 * Rotate an image 180 degrees.
 */
uint8_t *
fcv_rotate_180(uint32_t width, uint32_t height, uint8_t const *const data) {
  if (!data || width == 0 || height == 0) {
    return NULL;
  }

  // Check for overflow: width * height * 4
  if (width > SIZE_MAX / height) {
    return NULL;
  }
  size_t num_pixels = (size_t)width * height;
  if (num_pixels > SIZE_MAX / 4) {
    return NULL;
  }
  size_t img_length_byte = num_pixels * 4;

  uint8_t *rotated_data = malloc(img_length_byte);

  if (!rotated_data) {
    return NULL;
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      size_t src_index = ((size_t)y * width + x) * 4;
      size_t dst_index =
        ((size_t)(height - 1 - y) * width + (width - 1 - x)) * 4;

      rotated_data[dst_index] = data[src_index];
      rotated_data[dst_index + 1] = data[src_index + 1];
      rotated_data[dst_index + 2] = data[src_index + 2];
      rotated_data[dst_index + 3] = data[src_index + 3];
    }
  }

  return rotated_data;
}

/**
 * Rotate an image 270 degrees clockwise (90 degrees counter-clockwise).
 */
uint8_t *
fcv_rotate_270_cw(uint32_t width, uint32_t height, uint8_t const *const data) {
  if (!data || width == 0 || height == 0) {
    return NULL;
  }

  // Check for overflow: width * height * 4
  if (width > SIZE_MAX / height) {
    return NULL;
  }
  size_t num_pixels = (size_t)width * height;
  if (num_pixels > SIZE_MAX / 4) {
    return NULL;
  }
  size_t img_length_byte = num_pixels * 4;

  uint8_t *rotated_data = malloc(img_length_byte);

  if (!rotated_data) {
    return NULL;
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      size_t src_index = ((size_t)y * width + x) * 4;
      size_t dst_index = ((size_t)(width - 1 - x) * height + y) * 4;

      rotated_data[dst_index] = data[src_index];
      rotated_data[dst_index + 1] = data[src_index + 1];
      rotated_data[dst_index + 2] = data[src_index + 2];
      rotated_data[dst_index + 3] = data[src_index + 3];
    }
  }

  return rotated_data;
}
