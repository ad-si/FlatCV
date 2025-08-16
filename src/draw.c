#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "draw.h"
#include "parse_hex_color.h"
#else
#include "flatcv.h"
#endif

/**
 * Helper function to set a pixel to specified color (for circle drawing)
 */
void fcv_set_circle_pixel(
  uint8_t *data,
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  int32_t px,
  int32_t py,
  uint8_t r,
  uint8_t g,
  uint8_t b
) {
  if (px >= 0 && px < (int32_t)width && py >= 0 && py < (int32_t)height) {
    uint32_t pixel_index = (py * width + px) * channels;

    if (channels == 1) {
      // Grayscale: use luminance formula
      data[pixel_index] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
    }
    else if (channels >= 3) {
      data[pixel_index] = r;     // R
      data[pixel_index + 1] = g; // G
      data[pixel_index + 2] = b; // B
      if (channels == 4) {
        data[pixel_index + 3] = 255; // A
      }
    }
  }
}

/**
 * Helper function to draw 8 symmetric point32_ts of a circle
 */
void fcv_draw_circle_points(
  uint8_t *data,
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  int32_t cx,
  int32_t cy,
  int32_t x,
  int32_t y,
  uint8_t r,
  uint8_t g,
  uint8_t b
) {
  fcv_set_circle_pixel(data, width, height, channels, cx + x, cy + y, r, g, b);
  fcv_set_circle_pixel(data, width, height, channels, cx - x, cy + y, r, g, b);
  fcv_set_circle_pixel(data, width, height, channels, cx + x, cy - y, r, g, b);
  fcv_set_circle_pixel(data, width, height, channels, cx - x, cy - y, r, g, b);
  fcv_set_circle_pixel(data, width, height, channels, cx + y, cy + x, r, g, b);
  fcv_set_circle_pixel(data, width, height, channels, cx - y, cy + x, r, g, b);
  fcv_set_circle_pixel(data, width, height, channels, cx + y, cy - x, r, g, b);
  fcv_set_circle_pixel(data, width, height, channels, cx - y, cy - x, r, g, b);
}

/**
 * Helper function to fill horizontal lines for a filled circle (disk)
 */
void fcv_fill_disk_lines(
  uint8_t *data,
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  int32_t cx,
  int32_t cy,
  int32_t x,
  int32_t y,
  uint8_t r,
  uint8_t g,
  uint8_t b
) {
  // Fill horizontal lines at y offsets
  for (int32_t i = cx - x; i <= cx + x; i++) {
    fcv_set_circle_pixel(data, width, height, channels, i, cy + y, r, g, b);
    fcv_set_circle_pixel(data, width, height, channels, i, cy - y, r, g, b);
  }

  // Fill horizontal lines at x offsets
  // (avoid duplicating center line when x == y)
  if (x != y) {
    for (int32_t i = cx - y; i <= cx + y; i++) {
      fcv_set_circle_pixel(data, width, height, channels, i, cy + x, r, g, b);
      fcv_set_circle_pixel(data, width, height, channels, i, cy - x, r, g, b);
    }
  }
}

/**
 * Draw a circle on an image using Bresenham's circle algorithm
 * (modifies in-place).
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param channels Number of channels in the image (1, 3, or 4).
 * @param hex_color Hex color code (e.g., "FF0000" for red, "#00FF00" for
 * green).
 * @param radius Radius of the circle.
 * @param center_x X coordinate of the circle center.
 * @param center_y Y coordinate of the circle center.
 * @param data Point32_ter to the pixel data (modified in-place).
 */
void fcv_draw_circle(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  const char *hex_color,
  double radius,
  double center_x,
  double center_y,
  uint8_t *data
) {
  if (!data) {
    return;
  }

  // Parse color
  uint8_t r, g, b;
  fcv_parse_hex_color(hex_color, &r, &g, &b);

  int32_t radius_int32_t = (int32_t)radius;
  int32_t cx = (int32_t)center_x;
  int32_t cy = (int32_t)center_y;

  // Bresenham's circle algorithm
  int32_t x = 0;
  int32_t y = radius_int32_t;
  int32_t d = 3 - 2 * radius_int32_t;

  // Initial point32_ts
  fcv_draw_circle_points(data, width, height, channels, cx, cy, x, y, r, g, b);

  while (y >= x) {
    x++;
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    }
    else {
      d = d + 4 * x + 6;
    }
    fcv_draw_circle_points(
      data,
      width,
      height,
      channels,
      cx,
      cy,
      x,
      y,
      r,
      g,
      b
    );
  }
}

/**
 * Draw a filled circle (disk) on an image using Bresenham's circle
 * algorithm (modifies in-place).
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param channels Number of channels in the image (1, 3, or 4).
 * @param hex_color Hex color code (e.g., "FF0000" for red, "#00FF00" for
 * green).
 * @param radius Radius of the disk.
 * @param center_x X coordinate of the disk center.
 * @param center_y Y coordinate of the disk center.
 * @param data Point32_ter to the pixel data (modified in-place).
 */
void fcv_draw_disk(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  const char *hex_color,
  double radius,
  double center_x,
  double center_y,
  uint8_t *data
) {
  if (!data) {
    return;
  }

  // Parse color
  uint8_t r, g, b;
  fcv_parse_hex_color(hex_color, &r, &g, &b);

  int32_t radius_int32_t = (int32_t)radius;
  int32_t cx = (int32_t)center_x;
  int32_t cy = (int32_t)center_y;

  // Bresenham's circle algorithm for filled disk
  int32_t x = 0;
  int32_t y = radius_int32_t;
  int32_t d = 3 - 2 * radius_int32_t;

  // Initial filled lines
  fcv_fill_disk_lines(data, width, height, channels, cx, cy, x, y, r, g, b);

  while (y >= x) {
    x++;
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    }
    else {
      d = d + 4 * x + 6;
    }
    fcv_fill_disk_lines(data, width, height, channels, cx, cy, x, y, r, g, b);
  }
}
