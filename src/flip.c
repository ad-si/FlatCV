#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "flip.h"
#else
#include "flatcv.h"
#endif

/**
 * Flip an image horizontally (mirror along vertical axis).
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param data Pointer to the pixel data.
 * @return Pointer to the flipped image data.
 */
uint8_t *
fcv_flip_x(uint32_t width, uint32_t height, uint8_t const *const data) {
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

  uint8_t *flipped_data = malloc(img_length_byte);

  if (!flipped_data) { // Memory allocation failed
    return NULL;
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      size_t src_index = ((size_t)y * width + x) * 4;
      size_t dst_index = ((size_t)y * width + (width - 1 - x)) * 4;

      flipped_data[dst_index] = data[src_index];         // R
      flipped_data[dst_index + 1] = data[src_index + 1]; // G
      flipped_data[dst_index + 2] = data[src_index + 2]; // B
      flipped_data[dst_index + 3] = data[src_index + 3]; // A
    }
  }

  return flipped_data;
}

/**
 * Flip an image vertically (mirror along horizontal axis).
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param data Pointer to the pixel data.
 * @return Pointer to the flipped image data.
 */
uint8_t *
fcv_flip_y(uint32_t width, uint32_t height, uint8_t const *const data) {
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

  uint8_t *flipped_data = malloc(img_length_byte);

  if (!flipped_data) { // Memory allocation failed
    return NULL;
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      size_t src_index = ((size_t)y * width + x) * 4;
      size_t dst_index = ((size_t)(height - 1 - y) * width + x) * 4;

      flipped_data[dst_index] = data[src_index];         // R
      flipped_data[dst_index + 1] = data[src_index + 1]; // G
      flipped_data[dst_index + 2] = data[src_index + 2]; // B
      flipped_data[dst_index + 3] = data[src_index + 3]; // A
    }
  }

  return flipped_data;
}

/**
 * Transpose an image (flip along main diagonal).
 */
uint8_t *
fcv_transpose(uint32_t width, uint32_t height, uint8_t const *const data) {
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

  uint8_t *transposed_data = malloc(img_length_byte);

  if (!transposed_data) {
    return NULL;
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      size_t src_index = ((size_t)y * width + x) * 4;
      size_t dst_index = ((size_t)x * height + y) * 4;

      transposed_data[dst_index] = data[src_index];
      transposed_data[dst_index + 1] = data[src_index + 1];
      transposed_data[dst_index + 2] = data[src_index + 2];
      transposed_data[dst_index + 3] = data[src_index + 3];
    }
  }

  return transposed_data;
}

/**
 * Transverse an image (flip along anti-diagonal).
 */
uint8_t *
fcv_transverse(uint32_t width, uint32_t height, uint8_t const *const data) {
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

  uint8_t *transposed_data = malloc(img_length_byte);

  if (!transposed_data) {
    return NULL;
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      size_t src_index = ((size_t)y * width + x) * 4;
      size_t dst_index =
        ((size_t)(width - 1 - x) * height + (height - 1 - y)) * 4;

      transposed_data[dst_index] = data[src_index];
      transposed_data[dst_index + 1] = data[src_index + 1];
      transposed_data[dst_index + 2] = data[src_index + 2];
      transposed_data[dst_index + 3] = data[src_index + 3];
    }
  }

  return transposed_data;
}
