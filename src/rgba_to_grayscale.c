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
unsigned char *rgba_to_grayscale(
  unsigned int width,
  unsigned int height,
  unsigned char const *const data
) {
  unsigned int img_length_px = width * height;
  unsigned char *grayscale_data = malloc(img_length_px);

  if (!grayscale_data) { // Memory allocation failed
    return NULL;
  }

  // Process each pixel row by row
  for (unsigned int i = 0; i < width * height; i++) {
    unsigned int rgba_index = i * 4;

    unsigned char r = data[rgba_index];
    unsigned char g = data[rgba_index + 1];
    unsigned char b = data[rgba_index + 2];

    unsigned char gray = (r * R_WEIGHT + g * G_WEIGHT + b * B_WEIGHT) >> 8;

    grayscale_data[i] = gray;
  }

  return grayscale_data;
}
