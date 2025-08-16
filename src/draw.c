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
void set_circle_pixel(
  unsigned char *data,
  unsigned int width,
  unsigned int height,
  unsigned int channels,
  int px,
  int py,
  unsigned char r,
  unsigned char g,
  unsigned char b
) {
  if (px >= 0 && px < (int)width && py >= 0 && py < (int)height) {
    unsigned int pixel_index = (py * width + px) * channels;

    if (channels == 1) {
      // Grayscale: use luminance formula
      data[pixel_index] = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
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
 * Helper function to draw 8 symmetric points of a circle
 */
void draw_circle_points(
  unsigned char *data,
  unsigned int width,
  unsigned int height,
  unsigned int channels,
  int cx,
  int cy,
  int x,
  int y,
  unsigned char r,
  unsigned char g,
  unsigned char b
) {
  set_circle_pixel(data, width, height, channels, cx + x, cy + y, r, g, b);
  set_circle_pixel(data, width, height, channels, cx - x, cy + y, r, g, b);
  set_circle_pixel(data, width, height, channels, cx + x, cy - y, r, g, b);
  set_circle_pixel(data, width, height, channels, cx - x, cy - y, r, g, b);
  set_circle_pixel(data, width, height, channels, cx + y, cy + x, r, g, b);
  set_circle_pixel(data, width, height, channels, cx - y, cy + x, r, g, b);
  set_circle_pixel(data, width, height, channels, cx + y, cy - x, r, g, b);
  set_circle_pixel(data, width, height, channels, cx - y, cy - x, r, g, b);
}

/**
 * Helper function to fill horizontal lines for a filled circle (disk)
 */
void fill_disk_lines(
  unsigned char *data,
  unsigned int width,
  unsigned int height,
  unsigned int channels,
  int cx,
  int cy,
  int x,
  int y,
  unsigned char r,
  unsigned char g,
  unsigned char b
) {
  // Fill horizontal lines at y offsets
  for (int i = cx - x; i <= cx + x; i++) {
    set_circle_pixel(data, width, height, channels, i, cy + y, r, g, b);
    set_circle_pixel(data, width, height, channels, i, cy - y, r, g, b);
  }

  // Fill horizontal lines at x offsets
  // (avoid duplicating center line when x == y)
  if (x != y) {
    for (int i = cx - y; i <= cx + y; i++) {
      set_circle_pixel(data, width, height, channels, i, cy + x, r, g, b);
      set_circle_pixel(data, width, height, channels, i, cy - x, r, g, b);
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
 * @param data Pointer to the pixel data (modified in-place).
 */
void draw_circle(
  unsigned int width,
  unsigned int height,
  unsigned int channels,
  const char *hex_color,
  double radius,
  double center_x,
  double center_y,
  unsigned char *data
) {
  if (!data) {
    return;
  }

  // Parse color
  unsigned char r, g, b;
  parse_hex_color(hex_color, &r, &g, &b);

  int radius_int = (int)radius;
  int cx = (int)center_x;
  int cy = (int)center_y;

  // Bresenham's circle algorithm
  int x = 0;
  int y = radius_int;
  int d = 3 - 2 * radius_int;

  // Initial points
  draw_circle_points(data, width, height, channels, cx, cy, x, y, r, g, b);

  while (y >= x) {
    x++;
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    }
    else {
      d = d + 4 * x + 6;
    }
    draw_circle_points(data, width, height, channels, cx, cy, x, y, r, g, b);
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
 * @param data Pointer to the pixel data (modified in-place).
 */
void draw_disk(
  unsigned int width,
  unsigned int height,
  unsigned int channels,
  const char *hex_color,
  double radius,
  double center_x,
  double center_y,
  unsigned char *data
) {
  if (!data) {
    return;
  }

  // Parse color
  unsigned char r, g, b;
  parse_hex_color(hex_color, &r, &g, &b);

  int radius_int = (int)radius;
  int cx = (int)center_x;
  int cy = (int)center_y;

  // Bresenham's circle algorithm for filled disk
  int x = 0;
  int y = radius_int;
  int d = 3 - 2 * radius_int;

  // Initial filled lines
  fill_disk_lines(data, width, height, channels, cx, cy, x, y, r, g, b);

  while (y >= x) {
    x++;
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    }
    else {
      d = d + 4 * x + 6;
    }
    fill_disk_lines(data, width, height, channels, cx, cy, x, y, r, g, b);
  }
}
