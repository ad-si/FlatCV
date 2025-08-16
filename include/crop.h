#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

unsigned char *crop(
  unsigned int width,
  unsigned int height,
  unsigned int channels,
  unsigned char const * const data,
  unsigned int x,
  unsigned int y,
  unsigned int new_width,
  unsigned int new_height
);
