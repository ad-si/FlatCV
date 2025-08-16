#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "conversion.h"
#include "perspectivetransform.h"
#else
#include "flatcv.h"
#endif

static unsigned char *binary_dilation_disk(
  unsigned char const *image_data,
  int width,
  int height,
  int radius
) {
  if (!image_data || width <= 0 || height <= 0 || radius < 0) {
    return NULL;
  }

  unsigned char *result = malloc(width * height);
  if (!result) {
    return NULL;
  }

  // Initialize result to 0 (black)
  memset(result, 0, width * height);

  // For each pixel in the image
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = y * width + x;

      // If current pixel is white (255), dilate it
      if (image_data[idx] == 255) {
        // Apply disk-shaped structuring element
        for (int dy = -radius; dy <= radius; dy++) {
          for (int dx = -radius; dx <= radius; dx++) {
            // Check if point is within disk (use slightly larger radius for
            // better connectivity)
            double r_eff = radius + 0.5;
            if (dx * dx + dy * dy <= r_eff * r_eff) {
              int ny = y + dy;
              int nx = x + dx;

              // Check bounds
              if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                int nidx = ny * width + nx;
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

static unsigned char *binary_erosion_disk(
  unsigned char const *image_data,
  int width,
  int height,
  int radius
) {
  if (!image_data || width <= 0 || height <= 0 || radius < 0) {
    return NULL;
  }

  unsigned char *result = malloc(width * height);
  if (!result) {
    return NULL;
  }

  // Initialize result to 0 (black)
  memset(result, 0, width * height);

  // For each pixel in the image
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = y * width + x;

      // Check if structuring element fits entirely within white pixels
      bool can_erode = true;

      for (int dy = -radius; dy <= radius && can_erode; dy++) {
        for (int dx = -radius; dx <= radius && can_erode; dx++) {
          // Check if point is within disk (use slightly larger radius for
          // better connectivity)
          double r_eff = radius + 0.5;
          if (dx * dx + dy * dy <= r_eff * r_eff) {
            int ny = y + dy;
            int nx = x + dx;

            // Check bounds - pixels outside image are considered black (0)
            if (ny < 0 || ny >= height || nx < 0 || nx >= width) {
              can_erode = false;
            }
            else {
              int nidx = ny * width + nx;
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

unsigned char *binary_closing_disk(
  unsigned char const *image_data,
  int width,
  int height,
  int radius
) {
  if (!image_data || width <= 0 || height <= 0 || radius < 0) {
    return NULL;
  }

  // Step 1: Dilation
  unsigned char *dilated =
    binary_dilation_disk(image_data, width, height, radius);
  if (!dilated) {
    return NULL;
  }

  // Step 2: Erosion of the dilated image
  unsigned char *result = binary_erosion_disk(dilated, width, height, radius);

  // Free intermediate result
  free(dilated);

  return result;
}
