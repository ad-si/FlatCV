#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "parse_hex_color.h"
#else
#include "flatcv.h"
#endif

// Helper function to parse hex color code to RGB values
void parse_hex_color(
  const char *hex_color,
  unsigned char *r,
  unsigned char *g,
  unsigned char *b
) {
  const char *hex = hex_color;

  // Skip '#' if present
  if (hex[0] == '#') {
    hex++;
  }

  // Validate hex string length (should be 6 characters)
  if (strlen(hex) != 6) {
    // Default to white for invalid hex codes
    *r = 255;
    *g = 255;
    *b = 255;
    return;
  }

  // Validate that all characters are valid hex digits
  for (int i = 0; i < 6; i++) {
    if (!((hex[i] >= '0' && hex[i] <= '9') ||
          (hex[i] >= 'A' && hex[i] <= 'F') ||
          (hex[i] >= 'a' && hex[i] <= 'f'))) {
      // Default to white for invalid hex codes
      *r = 255;
      *g = 255;
      *b = 255;
      return;
    }
  }

  // Parse hex values
  unsigned int rgb_value;
  sscanf(hex, "%x", &rgb_value);

  *r = (rgb_value >> 16) & 0xFF;
  *g = (rgb_value >> 8) & 0xFF;
  *b = rgb_value & 0xFF;
}
