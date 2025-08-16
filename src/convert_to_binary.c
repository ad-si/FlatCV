#include <assert.h>
#include <math.h>
#include <stdbool.h>
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
unsigned char *convert_to_binary(
  const unsigned char *image_data,
  int width,
  int height,
  const char *foreground_hex,
  const char *background_hex
) {
  assert(image_data != NULL);
  assert(width > 0);
  assert(height > 0);
  assert(foreground_hex != NULL);
  assert(background_hex != NULL);

  unsigned char *result = malloc(width * height);
  if (!result) {
    fprintf(stderr, "Error: Failed to allocate memory for binary image\n");
    exit(EXIT_FAILURE);
  }

  unsigned char r_fg, g_fg, b_fg;
  parse_hex_color(foreground_hex, &r_fg, &g_fg, &b_fg);

  unsigned char r_bg, g_bg, b_bg;
  parse_hex_color(background_hex, &r_bg, &g_bg, &b_bg);

  for (int i = 0; i < width * height * 4; i += 4) {
    unsigned char r = image_data[i];
    unsigned char g = image_data[i + 1];
    unsigned char b = image_data[i + 2];

    int pixel_index = i / 4;
    if (r == r_fg && g == g_fg && b == b_fg) {
      // Convert foreground color to white
      result[pixel_index] = 255;
    }
    else if (r == r_bg && g == g_bg && b == b_bg) {
      // Convert background color to black
      result[pixel_index] = 0;
    }
    else {
      // Default to black for unmatched colors
      result[pixel_index] = 0;
    }
  }

  return result;
}
