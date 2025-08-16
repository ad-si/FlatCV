#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

void parse_hex_color(
  const char *hex_color,
  uint8_t *r,
  uint8_t *g,
  uint8_t *b
);
