#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "convert_to_binary.h"
#include "parse_hex_color.h"
#else
#include "flatcv.h"
#endif

/**
 * Convert an image to binary format based on foreground and background colors.
 * Return a grayscale image where the foreground color is white and the
 * background color is black.
 *
 * @param image_data Pointer to the input image data (RGBA format).
 * @param width Width of the image.
 * @param height Height of the image.
 * @param foreground_hex Hex color code for the foreground color (e.g.,
 * "FF0000").
 * @param background_hex Hex color code for the background color (e.g.,
 * "00FF00").
 */
uint8_t *fcv_convert_to_binary(
  const uint8_t *image_data,
  int32_t width,
  int32_t height,
  const char *foreground_hex,
  const char *background_hex
) {
  if (!image_data || !foreground_hex || !background_hex) {
    return NULL;
  }

  if (width <= 0 || height <= 0) {
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

  uint8_t r_fg, g_fg, b_fg;
  fcv_parse_hex_color(foreground_hex, &r_fg, &g_fg, &b_fg);

  uint8_t r_bg, g_bg, b_bg;
  fcv_parse_hex_color(background_hex, &r_bg, &g_bg, &b_bg);

  for (size_t i = 0; i < num_pixels; i++) {
    size_t rgba_index = i * 4;
    uint8_t r = image_data[rgba_index];
    uint8_t g = image_data[rgba_index + 1];
    uint8_t b = image_data[rgba_index + 2];

    if (r == r_fg && g == g_fg && b == b_fg) {
      // Convert foreground color to white
      result[i] = 255;
    }
    else if (r == r_bg && g == g_bg && b == b_bg) {
      // Convert background color to black
      result[i] = 0;
    }
    else {
      // Default to black for unmatched colors
      result[i] = 0;
    }
  }

  return result;
}
