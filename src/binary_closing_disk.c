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

uint8_t *fcv_binary_dilation_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
) {
  if (!image_data || width <= 0 || height <= 0 || radius < 0) {
    return NULL;
  }

  // Check for overflow: width * height (width and height already validated > 0)
  if ((size_t)width > SIZE_MAX / (size_t)height) {
    return NULL;
  }
  size_t num_pixels = (size_t)width * (size_t)height;

  uint8_t *result = malloc(num_pixels);
  if (!result) {
    return NULL;
  }

  // Initialize result to 0 (black)
  memset(result, 0, num_pixels);

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

static uint8_t *binary_erosion_disk_internal(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius,
  bool replicate_border
) {
  if (!image_data || width <= 0 || height <= 0 || radius < 0) {
    return NULL;
  }

  // Check for overflow: width * height (width and height already validated > 0)
  if ((size_t)width > SIZE_MAX / (size_t)height) {
    return NULL;
  }
  size_t num_pixels = (size_t)width * (size_t)height;

  uint8_t *result = malloc(num_pixels);
  if (!result) {
    return NULL;
  }

  // Initialize result to 0 (black)
  memset(result, 0, num_pixels);

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

            uint8_t neighbor_value;

            // Check bounds
            if (ny < 0 || ny >= height || nx < 0 || nx >= width) {
              if (replicate_border) {
                // Replicate border: clamp coordinates to valid range
                int32_t clamped_y =
                  ny < 0 ? 0 : (ny >= height ? height - 1 : ny);
                int32_t clamped_x = nx < 0 ? 0 : (nx >= width ? width - 1 : nx);
                neighbor_value = image_data[clamped_y * width + clamped_x];
              }
              else {
                // Default: treat out-of-bounds as black
                neighbor_value = 0;
              }
            }
            else {
              neighbor_value = image_data[ny * width + nx];
            }

            if (neighbor_value != 255) {
              can_erode = false;
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

uint8_t *fcv_binary_erosion_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
) {
  // Public API uses default behavior: treat out-of-bounds as black
  return binary_erosion_disk_internal(image_data, width, height, radius, false);
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
  uint8_t *dilated =
    fcv_binary_dilation_disk(image_data, width, height, radius);
  if (!dilated) {
    return NULL;
  }

  // Step 2: Erosion of the dilated image
  // Use replicate_border=true to preserve original white pixels at borders
  // In a closing operation, out-of-bounds pixels use "replicate" border mode
  // which clamps coordinates to valid range, preventing border pixels from
  // being erroneously eroded due to artificial black boundary
  uint8_t *result =
    binary_erosion_disk_internal(dilated, width, height, radius, true);

  // Free intermediate result
  free(dilated);

  return result;
}

uint8_t *fcv_binary_opening_disk(
  uint8_t const *image_data,
  int32_t width,
  int32_t height,
  int32_t radius
) {
  if (!image_data || width <= 0 || height <= 0 || radius < 0) {
    return NULL;
  }

  // Step 1: Erosion
  uint8_t *eroded = fcv_binary_erosion_disk(image_data, width, height, radius);
  if (!eroded) {
    return NULL;
  }

  // Step 2: Dilation of the eroded image
  uint8_t *result = fcv_binary_dilation_disk(eroded, width, height, radius);

  // Free intermediate result
  free(eroded);

  return result;
}
