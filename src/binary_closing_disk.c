#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "conversion.h"
#include "perspectivetransform.h"
#else
#include "flatcv.h"
#endif

static uint8_t *binary_dilation_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
) {
  if (!image_data || width <= 0 || height <= 0 || radius < 0) {
    return NULL;
  }

  uint8_t *result = malloc(width * height);
  if (!result) {
    return NULL;
  }

  // Initialize result to 0 (black)
  memset(result, 0, width * height);

  // For each pixel in the image
  for (int32_t y = 0; y < height; y++) {
    for (int32_t x = 0; x < width; x++) {
      int32_t idx = y * width + x;

      // If current pixel is white (255), dilate it
      if (image_data[idx] == 255) {
        // Apply disk-shaped structuring element
        for (int32_t dy = -radius; dy <= radius; dy++) {
          for (int32_t dx = -radius; dx <= radius; dx++) {
            // Check if point32_t is within disk (use slightly larger radius for
            // better connectivity)
            double r_eff = radius + 0.5;
            if (dx * dx + dy * dy <= r_eff * r_eff) {
              int32_t ny = y + dy;
              int32_t nx = x + dx;

              // Check bounds
              if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                int32_t nidx = ny * width + nx;
                result[nidx] = 255;
              }
            }
          }
        }
      }
    }
  }

  return result;
}

static uint8_t *binary_erosion_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
) {
  if (!image_data || width <= 0 || height <= 0 || radius < 0) {
    return NULL;
  }

  uint8_t *result = malloc(width * height);
  if (!result) {
    return NULL;
  }

  // Initialize result to 0 (black)
  memset(result, 0, width * height);

  // For each pixel in the image
  for (int32_t y = 0; y < height; y++) {
    for (int32_t x = 0; x < width; x++) {
      int32_t idx = y * width + x;

      // Check if structuring element fits entirely within white pixels
      bool can_erode = true;

      for (int32_t dy = -radius; dy <= radius && can_erode; dy++) {
        for (int32_t dx = -radius; dx <= radius && can_erode; dx++) {
          // Check if point32_t is within disk (use slightly larger radius for
          // better connectivity)
          double r_eff = radius + 0.5;
          if (dx * dx + dy * dy <= r_eff * r_eff) {
            int32_t ny = y + dy;
            int32_t nx = x + dx;

            // Check bounds - pixels outside image are considered black (0)
            if (ny < 0 || ny >= height || nx < 0 || nx >= width) {
              can_erode = false;
            }
            else {
              int32_t nidx = ny * width + nx;
              if (image_data[nidx] != 255) {
                can_erode = false;
              }
            }
          }
        }
      }

      if (can_erode) {
        result[idx] = 255;
      }
    }
  }

  return result;
}

uint8_t *fcv_binary_closing_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
) {
  if (!image_data || width <= 0 || height <= 0 || radius < 0) {
    return NULL;
  }

  // Step 1: Dilation
  uint8_t *dilated = binary_dilation_disk(image_data, width, height, radius);
  if (!dilated) {
    return NULL;
  }

  // Step 2: Erosion of the dilated image
  uint8_t *result = binary_erosion_disk(dilated, width, height, radius);

  // Free int32_termediate result
  free(dilated);

  return result;
}
