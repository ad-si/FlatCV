#ifndef FLATCV_AMALGAMATION
#pragma once
#endif

#include <stdint.h>

/**
 * Get EXIF orientation from a JPEG file.
 *
 * @param filename Path to the JPEG file.
 * @return Orientation value (1-8), or 1 if not found or error.
 */
int32_t fcv_get_exif_orientation(char const *filename);
