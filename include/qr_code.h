#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stddef.h>
#include <stdint.h>

#ifndef FLATCV_AMALGAMATION
#include "1_types.h"
#endif

typedef struct {
  char *text;
  Corners corners;
  Point2D finders[3]; // TL, TR, BL finder pattern centers
  Point2D *alignments;     // Alignment pattern centers (pixel coords)
  size_t alignment_count;  // Number of alignment patterns (0 for v1)
  int version;             // QR version, 1..40
  int qr_size;             // Module count per side: 21 + 4*(version-1)
  double module_size;
} FCVQRCode;

typedef struct {
  FCVQRCode *codes;
  size_t count;
} FCVQRCodeResult;

/**
 * Decode QR codes from a single-channel grayscale image.
 *
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param gray_pixels Grayscale pixel array (0 = black, 255 = white).
 * @return Result struct with decoded codes. Call fcv_free_qr_result to release.
 */
FCVQRCodeResult fcv_decode_qr_codes(
  uint32_t width,
  uint32_t height,
  uint8_t const *const gray_pixels
);

void fcv_free_qr_result(FCVQRCodeResult result);
