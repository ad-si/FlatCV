#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

unsigned char *convert_to_binary(
  const unsigned char *image_data,
  int width,
  int height,
  const char *foreground_hex,
  const char *background_hex
);
