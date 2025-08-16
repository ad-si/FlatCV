#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

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
);

void fcv_draw_circle(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  const char *hex_color,
  double radius,
  double center_x,
  double center_y,
  uint8_t *data
);

void fcv_draw_disk(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  const char *hex_color,
  double radius,
  double center_x,
  double center_y,
  uint8_t *data
);

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
);

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
);
