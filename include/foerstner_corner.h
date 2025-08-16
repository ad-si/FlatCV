#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

uint8_t *foerstner_corner(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data,
  double sigma
);
