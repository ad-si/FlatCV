#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

uint8_t *single_to_multichannel(
  uint32_t width,
  uint32_t height,
  uint8_t const *const data
);
