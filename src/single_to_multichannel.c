#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "single_to_multichannel.h"
#else
#include "flatcv.h"
#endif

/**
 * Convert single channel grayscale image data to
 * RGBA row-major top-to-bottom image data.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param data Pointer to the pixel data.
 */
uint8_t *fcv_single_to_multichannel(
  uint32_t width,
  uint32_t height,
  uint8_t const *const data
) {
  uint32_t img_length_px = width * height;
  uint8_t *multichannel_data = malloc(img_length_px * 4);

  if (!multichannel_data) { // Memory allocation failed
    return NULL;
  }

  for (uint32_t i = 0; i < img_length_px; i++) {
    uint32_t rgba_index = i * 4;
    multichannel_data[rgba_index] = data[i];
    multichannel_data[rgba_index + 1] = data[i];
    multichannel_data[rgba_index + 2] = data[i];
    multichannel_data[rgba_index + 3] = 255;
  }

  return multichannel_data;
}
