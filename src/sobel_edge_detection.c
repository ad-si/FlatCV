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
unsigned char *sobel_edge_detection(
  unsigned int width,
  unsigned int height,
  unsigned int channels,
  unsigned char const *const data
) {
  if (!data || width == 0 || height == 0) {
    return NULL;
  }

  unsigned int img_length_px = width * height;
  unsigned char *grayscale_data;
  bool allocated_grayscale = false;

  if (channels == 1) {
    // Single-channel input, use data directly
    grayscale_data = (unsigned char *)data;
    allocated_grayscale = false;
  }
  else {
    // Multi-channel input, convert to grayscale
    grayscale_data = rgba_to_grayscale(width, height, data);
    if (!grayscale_data) {
      return NULL;
    }
    allocated_grayscale = true;
  }

  unsigned char *sobel_data = malloc(img_length_px);
  if (!sobel_data) {
    if (allocated_grayscale) {
      free(grayscale_data);
    }
    return NULL;
  }

  // Sobel kernels
  int sobel_x[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
  int sobel_y[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

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
  for (unsigned int y = 0; y < height; y++) {
    for (unsigned int x = 0; x < width; x++) {
      int gx = 0;
      int gy = 0;

      // Apply Sobel kernels
      for (int ky = -1; ky <= 1; ky++) {
        for (int kx = -1; kx <= 1; kx++) {
          int px = x + kx;
          int py = y + ky;

          // Handle boundaries by using nearest pixel values
          if (px < 0) {
            px = 0;
          }
          if ((unsigned int)px >= width) {
            px = width - 1;
          }
          if (py < 0) {
            py = 0;
          }
          if ((unsigned int)py >= height) {
            py = height - 1;
          }

          unsigned char pixel = grayscale_data[py * width + px];
          gx += pixel * sobel_x[ky + 1][kx + 1];
          gy += pixel * sobel_y[ky + 1][kx + 1];
        }
      }

      double magnitude = sqrt(gx * gx + gy * gy);
      magnitudes[y * width + x] = magnitude;

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

  for (unsigned int y = 0; y < height; y++) {
    for (unsigned int x = 0; x < width; x++) {
      double magnitude = magnitudes[y * width + x];

      // Normalize to 0-255 range based on actual min/max
      int final_magnitude =
        (int)(((magnitude - min_magnitude) / range) * 255.0);

      sobel_data[y * width + x] = final_magnitude;
    }
  }

  free(magnitudes);

  if (allocated_grayscale) {
    free(grayscale_data);
  }
  return sobel_data;
}
