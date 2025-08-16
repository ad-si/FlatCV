#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "rgba_to_grayscale.h"
#else
#include "flatcv.h"
#endif

/**
 * Convert raw RGBA row-major top-to-bottom image data
 * to a single channel grayscale image data.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param data Pointer to the pixel data.
 * @return Pointer to the single channel grayscale image data.
 */
uint8_t *
rgba_to_grayscale(uint32_t width, uint32_t height, uint8_t const *const data) {
  uint32_t img_length_px = width * height;
  uint8_t *grayscale_data = malloc(img_length_px);

  if (!grayscale_data) { // Memory allocation failed
    return NULL;
  }

  // Process each pixel row by row
  for (uint32_t i = 0; i < width * height; i++) {
    uint32_t rgba_index = i * 4;

    uint8_t r = data[rgba_index];
    uint8_t g = data[rgba_index + 1];
    uint8_t b = data[rgba_index + 2];

    uint8_t gray = (r * R_WEIGHT + g * G_WEIGHT + b * B_WEIGHT) >> 8;

    grayscale_data[i] = gray;
  }

  return grayscale_data;
}
