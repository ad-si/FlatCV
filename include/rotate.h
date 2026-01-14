#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

uint8_t *fcv_rotate_90_cw(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data
);

uint8_t *fcv_rotate_180(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data
);

uint8_t *fcv_rotate_270_cw(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data
);
