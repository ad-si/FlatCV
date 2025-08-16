#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

uint8_t *fcv_extract_document(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data,
  uint32_t output_width,
  uint32_t output_height
);

uint8_t *fcv_extract_document_auto(
  uint32_t width,
  uint32_t height,
  uint8_t const * const data,
  uint32_t *output_width,
  uint32_t *output_height
);