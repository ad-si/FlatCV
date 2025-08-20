#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

uint8_t *fcv_trim(
  int32_t *width,
  int32_t *height,
  uint32_t channels,
  uint8_t const * const data
);
