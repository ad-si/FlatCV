#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "histogram.h"
#else
#include "flatcv.h"
#endif

/**
 * Generate a histogram visualization image from input image data.
 * For grayscale images, creates a single histogram.
 * For RGB(A) images, creates overlapping histograms for each channel.
 *
 * @param width Width of the input image.
 * @param height Height of the input image.
 * @param channels Number of channels in the input image (1, 3, or 4).
 * @param data Pointer to the input pixel data.
 * @param out_width Pointer to store the output histogram width.
 * @param out_height Pointer to store the output histogram height.
 * @return Pointer to the histogram image data (RGBA format).
 */
uint8_t *fcv_generate_histogram(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  uint8_t const *const data,
  uint32_t *out_width,
  uint32_t *out_height
) {
  if (!data || !out_width || !out_height) {
    return NULL;
  }

  // Output histogram dimensions
  const uint32_t hist_width = 256;  // One pixel per brightness value
  const uint32_t hist_height = 200; // Fixed height for visualization

  *out_width = hist_width;
  *out_height = hist_height;

  uint32_t img_size = width * height;
  uint32_t output_size = (*out_width) * (*out_height) * 4;

  // Allocate output image (initialized to black background)
  uint8_t *output = calloc(output_size, sizeof(uint8_t));
  if (!output) {
    return NULL;
  }

  // Initialize with black background and full alpha
  for (uint32_t i = 0; i < (*out_width) * (*out_height); i++) {
    output[i * 4 + 3] = 255; // Alpha channel
  }

  // Count histograms for each channel
  uint32_t hist_r[256] = {0};
  uint32_t hist_g[256] = {0};
  uint32_t hist_b[256] = {0};
  uint32_t hist_gray[256] = {0};

  bool is_grayscale = (channels == 1);
  if (channels == 4) {
    // Check if it's actually grayscale (all RGB channels equal)
    is_grayscale = true;
    for (uint32_t i = 0; i < img_size && is_grayscale; i++) {
      uint32_t idx = i * 4;
      if (data[idx] != data[idx + 1] || data[idx] != data[idx + 2]) {
        is_grayscale = false;
      }
    }
  }

  if (is_grayscale) {
    // Single channel or grayscale image
    for (uint32_t i = 0; i < img_size; i++) {
      uint8_t value;
      if (channels == 1) {
        value = data[i];
      }
      else {
        // Use the red channel for grayscale RGBA
        value = data[i * 4];
      }
      hist_gray[value]++;
    }
  }
  else {
    // RGB or RGBA image - count each channel
    for (uint32_t i = 0; i < img_size; i++) {
      uint32_t idx = i * channels;
      hist_r[data[idx]]++;
      if (channels >= 3) {
        hist_g[data[idx + 1]]++;
        hist_b[data[idx + 2]]++;
      }
    }
  }

  // Find maximum count for scaling
  uint32_t max_count = 0;
  if (is_grayscale) {
    for (int i = 0; i < 256; i++) {
      if (hist_gray[i] > max_count) {
        max_count = hist_gray[i];
      }
    }
  }
  else {
    for (int i = 0; i < 256; i++) {
      if (hist_r[i] > max_count) {
        max_count = hist_r[i];
      }
      if (channels >= 3) {
        if (hist_g[i] > max_count) {
          max_count = hist_g[i];
        }
        if (hist_b[i] > max_count) {
          max_count = hist_b[i];
        }
      }
    }
  }

  if (max_count == 0) {
    return output; // Return black image if no data
  }

  // Draw histogram bars
  for (int x = 0; x < 256; x++) {
    if (is_grayscale) {
      // Draw white histogram for grayscale
      uint32_t bar_height = (hist_gray[x] * hist_height) / max_count;
      for (uint32_t y = 0; y < bar_height; y++) {
        uint32_t output_x = x;
        uint32_t output_y = hist_height - 1 - y;
        uint32_t output_idx = (output_y * (*out_width) + output_x) * 4;

        output[output_idx] = 255;     // R
        output[output_idx + 1] = 255; // G
        output[output_idx + 2] = 255; // B
        output[output_idx + 3] = 255; // A
      }
    }
    else {
      // Draw overlapping colored histograms for RGB
      uint32_t bar_height_r = (hist_r[x] * hist_height) / max_count;
      uint32_t bar_height_g =
        (channels >= 3) ? (hist_g[x] * hist_height) / max_count : 0;
      uint32_t bar_height_b =
        (channels >= 3) ? (hist_b[x] * hist_height) / max_count : 0;

      uint32_t max_bar_height = bar_height_r;
      if (bar_height_g > max_bar_height) {
        max_bar_height = bar_height_g;
      }
      if (bar_height_b > max_bar_height) {
        max_bar_height = bar_height_b;
      }

      for (uint32_t y = 0; y < max_bar_height; y++) {
        uint32_t output_x = x;
        uint32_t output_y = hist_height - 1 - y;
        uint32_t output_idx = (output_y * (*out_width) + output_x) * 4;

        // Initialize to black
        uint8_t r = 0, g = 0, b = 0;

        // Add red channel contribution
        if (y < bar_height_r) {
          r = 255;
        }

        // Add green channel contribution
        if (channels >= 3 && y < bar_height_g) {
          g = 255;
        }

        // Add blue channel contribution
        if (channels >= 3 && y < bar_height_b) {
          b = 255;
        }

        output[output_idx] = r;       // R
        output[output_idx + 1] = g;   // G
        output[output_idx + 2] = b;   // B
        output[output_idx + 3] = 255; // A
      }
    }
  }

  return output;
}
