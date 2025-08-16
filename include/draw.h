#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

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
);

void draw_circle(
  unsigned int width,
  unsigned int height,
  unsigned int channels,
  const char *hex_color,
  double radius,
  double center_x,
  double center_y,
  unsigned char *data
);

void draw_disk(
  unsigned int width,
  unsigned int height,
  unsigned int channels,
  const char *hex_color,
  double radius,
  double center_x,
  double center_y,
  unsigned char *data
);

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
);

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
);
