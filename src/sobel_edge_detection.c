#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "conversion.h"
#include "perspectivetransform.h"
#include "rgba_to_grayscale.h"
#include "sobel_edge_detection.h"
#else
#include "flatcv.h"
#endif

/**
 * Apply Sobel edge detection to the image data and return single-channel
 * grayscale data. Uses Sobel kernels to detect edges in horizontal and vertical
 * directions, then combines them to get the edge magnitude.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param channels Number of channels in the input image.
 *                 (1: Grayscale, 3: RGB, 4: RGBA)
 * @param data Pointer to the input pixel data.
 * @return Pointer to the single-channel grayscale edge-detected image data.
 */
uint8_t *fcv_sobel_edge_detection(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  uint8_t const *const data
) {
  if (!data || width == 0 || height == 0 || channels == 0) {
    return NULL;
  }

  // Check for overflow: width * height
  if (width > SIZE_MAX / height) {
    return NULL;
  }
  size_t img_length_px = (size_t)width * height;

  // Check for overflow in magnitudes allocation
  if (img_length_px > SIZE_MAX / sizeof(double)) {
    return NULL;
  }

  uint8_t *grayscale_data;
  bool allocated_grayscale = false;

  if (channels == 1) {
    // Single-channel input, use data directly
    grayscale_data = (uint8_t *)data;
    allocated_grayscale = false;
  }
  else {
    // Multi-channel input, convert to grayscale
    grayscale_data = fcv_rgba_to_grayscale(width, height, data);
    if (!grayscale_data) {
      return NULL;
    }
    allocated_grayscale = true;
  }

  uint8_t *sobel_data = malloc(img_length_px);
  if (!sobel_data) {
    if (allocated_grayscale) {
      free(grayscale_data);
    }
    return NULL;
  }

  // Sobel kernels
  int32_t sobel_x[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
  int32_t sobel_y[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

  // Temporary buffer to store magnitudes for normalization
  double *magnitudes = malloc(img_length_px * sizeof(double));
  if (!magnitudes) {
    if (allocated_grayscale) {
      free(grayscale_data);
    }
    free(sobel_data);
    return NULL;
  }

  double min_magnitude = INFINITY;
  double max_magnitude = 0.0;

  // First pass: calculate all magnitudes and find min/max
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      int32_t gx = 0;
      int32_t gy = 0;

      // Apply Sobel kernels
      for (int32_t ky = -1; ky <= 1; ky++) {
        for (int32_t kx = -1; kx <= 1; kx++) {
          int32_t px = x + kx;
          int32_t py = y + ky;

          // Handle boundaries by using nearest pixel values
          if (px < 0) {
            px = 0;
          }
          if ((uint32_t)px >= width) {
            px = width - 1;
          }
          if (py < 0) {
            py = 0;
          }
          if ((uint32_t)py >= height) {
            py = height - 1;
          }

          uint8_t pixel = grayscale_data[(size_t)py * width + px];
          gx += pixel * sobel_x[ky + 1][kx + 1];
          gy += pixel * sobel_y[ky + 1][kx + 1];
        }
      }

      double magnitude = sqrt((double)gx * gx + (double)gy * gy);
      magnitudes[(size_t)y * width + x] = magnitude;

      if (magnitude < min_magnitude) {
        min_magnitude = magnitude;
      }
      if (magnitude > max_magnitude) {
        max_magnitude = magnitude;
      }
    }
  }

  // Second pass: normalize and assign to output
  double range = max_magnitude - min_magnitude;
  if (range == 0.0) {
    range = 1.0; // Avoid division by zero
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      double magnitude = magnitudes[(size_t)y * width + x];

      // Normalize to 0-255 range based on actual min/max
      int32_t final_magnitude =
        (int32_t)(((magnitude - min_magnitude) / range) * 255.0);

      // Clamp to valid range
      if (final_magnitude < 0) {
        final_magnitude = 0;
      }
      if (final_magnitude > 255) {
        final_magnitude = 255;
      }

      sobel_data[(size_t)y * width + x] = (uint8_t)final_magnitude;
    }
  }

  free(magnitudes);

  if (allocated_grayscale) {
    free(grayscale_data);
  }
  return sobel_data;
}
