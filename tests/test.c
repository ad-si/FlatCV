#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "binary_closing_disk.h"
#include "conversion.h"
#include "corner_peaks.h"
#include "draw.h"
#include "foerstner_corner.h"
#include "histogram.h"
#include "perspectivetransform.h"
#include "rgba_to_grayscale.h"
#include "sort_corners.h"
#include "trim.h"

/**
 * Utility function to create binary images from arrays of 0s and 1s.
 *
 * @param pattern Array of 0s and 1s representing the binary pattern
 * @param width Width of the image
 * @param height Height of the image
 * @return Pointer to newly allocated uint8_t array with binary data (0 or 255)
 *
 * Example usage:
 *   int pattern[9] = {
 *     1, 0, 1,
 *     0, 1, 0,
 *     1, 0, 1
 *   };
 *   uint8_t *image = create_binary_image(pattern, 3, 3);
 *   // Creates a 3x3 checkerboard pattern with 255 for 1s and 0 for 0s
 */
uint8_t *
create_binary_image(const int *pattern, uint32_t width, uint32_t height) {
  if (!pattern || width == 0 || height == 0) {
    return NULL;
  }

  uint8_t *data = malloc(width * height);
  if (!data) {
    return NULL;
  }

  for (uint32_t i = 0; i < width * height; i++) {
    data[i] = pattern[i] ? 255 : 0;
  }

  return data;
}

/**
 * Utility function to create RGBA images from character patterns.
 *
 * @param pattern Array of characters representing different colors
 * @param width Width of the image
 * @param height Height of the image
 * @param color_map Array of RGBA colors (4 bytes per color) indexed by
 * character
 * @return Pointer to newly allocated uint8_t array with RGBA data
 *
 * Example usage:
 *   char pattern[9] = {
 *     '0', '0', '0',
 *     '0', 'R', '0',
 *     '0', '0', '0'
 *   };
 *   uint8_t colors[] = {
 *     ['0'] = {0, 0, 0, 255},  // Black
 *     ['R'] = {255, 0, 0, 255} // Red
 *   };
 *   uint8_t *image = create_rgba_pattern_image(pattern, 3, 3, colors);
 */
uint8_t *create_rgba_pattern_image(
  const char *pattern,
  uint32_t width,
  uint32_t height
) {
  if (!pattern || width == 0 || height == 0) {
    return NULL;
  }

  // Color map
  uint8_t color_map[256][4] = {0}; // Initialize all to black
  // 0 = Black
  color_map['0'][0] = color_map['0'][1] = color_map['0'][2] = 0;
  color_map['0'][3] = 255;
  // 1 = White
  color_map['1'][0] = color_map['1'][1] = color_map['1'][2] =
    color_map['1'][3] = 255;
  // R = Red
  color_map['R'][0] = 255;
  color_map['R'][1] = color_map['R'][2] = 0;
  color_map['R'][3] = 255;
  // G = Green
  color_map['G'][0] = 0;
  color_map['G'][1] = 255;
  color_map['G'][2] = 0;
  color_map['G'][3] = 255;
  // B = Blue
  color_map['B'][0] = color_map['B'][1] = 0;
  color_map['B'][2] = color_map['B'][3] = 255;

  uint8_t *data = malloc(width * height * 4);
  if (!data) {
    return NULL;
  }

  for (uint32_t i = 0; i < width * height; i++) {
    unsigned char c = (unsigned char)pattern[i];
    data[i * 4 + 0] = color_map[c][0]; // R
    data[i * 4 + 1] = color_map[c][1]; // G
    data[i * 4 + 2] = color_map[c][2]; // B
    data[i * 4 + 3] = color_map[c][3]; // A
  }

  return data;
}

int32_t test_otsu_threshold() {
  uint32_t width = 4;
  uint32_t height = 4;
  uint8_t data[64] = {1, 1, 1, 255, 2, 2, 2, 255, 9, 9, 9, 255, 8, 8, 8, 255,
                      2, 2, 2, 255, 1, 1, 1, 255, 9, 9, 9, 255, 7, 7, 7, 255,
                      2, 2, 2, 255, 0, 0, 0, 255, 8, 8, 8, 255, 2, 2, 2, 255,
                      0, 0, 0, 255, 2, 2, 2, 255, 9, 9, 9, 255, 8, 8, 8, 255};

  uint8_t const *const monochrome_data =
    fcv_otsu_threshold_rgba(width, height, false, data);

  uint8_t expected_data[64] = {
    0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255, 0,   0,   0,   255,
    0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255
  };

  bool test_ok = true;

  for (uint32_t i = 0; i < 64; i++) {
    if (monochrome_data[i] != expected_data[i]) {
      test_ok = false;
      printf(
        "Test failed at index %d: Monochrome data %d != Expected data %d\n",
        i,
        monochrome_data[i],
        expected_data[i]
      );
      // return 1;
    }
  }

  free((void *)monochrome_data);

  if (test_ok) {
    printf("✅ Otsu's threshold test passed\n");
    return 0;
  }
  else {
    printf("❌ Test failed\n");
    return 1;
  }
}

int32_t test_perspective_transform() {
  Corners src = {
    100,
    100, // Top-left
    400,
    150, // Top-right
    380,
    400, // Bottom-right
    120,
    380 // Bottom-left
  };

  Corners dst = {
    0,
    0, // Top-left
    300,
    0, // Top-right
    300,
    300, // Bottom-right
    0,
    300 // Bottom-left
  };

  Matrix3x3 *tmat = fcv_calculate_perspective_transform(&src, &dst);

  double eps = 0.001;
  bool test_ok = true;

  if (fabs(tmat->m00 - 0.85256062) > eps) {
    printf("m00: %f\n", tmat->m00);
    test_ok = false;
  }
  if (fabs(tmat->m01 + 0.06089719) > eps) {
    printf("m01: %f\n", tmat->m01);
    test_ok = false;
  }
  if (fabs(tmat->m02 + 79.16634335) > eps) {
    printf("m02: %f\n", tmat->m02);
    test_ok = false;
  }
  if (fabs(tmat->m10 + 0.14503146) > eps) {
    printf("m10: %f\n", tmat->m10);
    test_ok = false;
  }
  if (fabs(tmat->m11 - 0.87018875) > eps) {
    printf("m11: %f\n", tmat->m11);
    test_ok = false;
  }
  if (fabs(tmat->m12 + 72.51572949) > eps) {
    printf("m12: %f\n", tmat->m12);
    test_ok = false;
  }
  if (fabs(tmat->m20 + 0.00022582) > eps) {
    printf("m20: %f\n", tmat->m20);
    test_ok = false;
  }
  if (fabs(tmat->m21 + 0.00044841) > eps) {
    printf("m21: %f\n", tmat->m21);
    test_ok = false;
  }
  if (fabs(tmat->m22 - 1) > eps) {
    printf("m22: %f\n", tmat->m22);
    test_ok = false;
  }

  if (test_ok) {
    printf("✅ Perspective transform test passed\n");
    return 0;
  }
  else {
    printf("❌ Perspective transform test failed\n");
    return 1;
  }
}

int32_t test_perspective_transform_float() {
  Corners src = {
    278.44,
    182.23, // Top-left
    1251.25,
    178.79, // Top-right
    1395.63,
    718.48, // Bottom-right
    216.56,
    770.04 // Bottom-left
  };

  Corners dst = {
    0,
    0, // Top-left
    1076.5,
    0, // Top-right
    1076.5,
    574.86, // Bottom-right
    0,
    574.86 // Bottom-left
  };

  Matrix3x3 *tmat = fcv_calculate_perspective_transform(&src, &dst);

  double eps = 0.001;
  bool test_ok = true;

  if (fabs(tmat->m00 - 1.08707) > eps) {
    printf("m00: %f\n", tmat->m00);
    test_ok = false;
  }
  if (fabs(tmat->m01 - 0.114438) > eps) {
    printf("m01: %f\n", tmat->m01);
    test_ok = false;
  }
  if (fabs(tmat->m02 + 323.538) > eps) {
    printf("m02: %f\n", tmat->m02);
    test_ok = false;
  }
  if (fabs(tmat->m10 - 0.00445981) > eps) {
    printf("m10: %f\n", tmat->m10);
    test_ok = false;
  }
  if (fabs(tmat->m11 - 1.26121) > eps) {
    printf("m11: %f\n", tmat->m11);
    test_ok = false;
  }
  if (fabs(tmat->m12 + 231.072) > eps) {
    printf("m12: %f\n", tmat->m12);
    test_ok = false;
  }
  if (fabs(tmat->m20 + 0.0000708899) > eps) {
    printf("m20: %f\n", tmat->m20);
    test_ok = false;
  }
  if (fabs(tmat->m21 - 0.000395421) > eps) {
    printf("m21: %f\n", tmat->m21);
    test_ok = false;
  }
  if (fabs(tmat->m22 - 1) > eps) {
    printf("m22: %f\n", tmat->m22);
    test_ok = false;
  }

  if (test_ok) {
    printf("✅ Perspective transform with floats test passed\n");
    return 0;
  }
  else {
    printf("❌ Perspective transform with floats test failed\n");
    return 1;
  }
}

void free_fcv_corner_peaks(CornerPeaks *peaks) {
  if (peaks) {
    free(peaks->points);
    free(peaks);
  }
}

int32_t test_fcv_foerstner_corner() {
  // Create a simple test image with a corner pattern (5x5 RGBA)
  uint32_t width = 5;
  uint32_t height = 5;

  // Create a simple corner pattern: white square on black background
  uint8_t data[100] = {
    // Row 0: all black
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    // Row 1: black, then white corner
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    // Row 2: black, then white corner
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    // Row 3: all black
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    // Row 4: all black
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255,
    0,
    0,
    0,
    255
  };

  // Convert RGBA to grayscale first
  uint8_t *gray_data = fcv_rgba_to_grayscale(width, height, data);
  if (!gray_data) {
    printf("❌ Foerstner corner test failed: grayscale conversion failed\n");
    return 1;
  }

  double sigma = 1.0;
  uint8_t *result = fcv_foerstner_corner(width, height, gray_data, sigma);

  free(gray_data);

  if (result == NULL) {
    printf("❌ Foerstner corner test failed: NULL result\n");
    return 1;
  }

  // Basic sanity checks:
  // 1. Function should return valid pointer
  // 2. Result should have 2 channels (w and q measures)
  // 3. Values should be in [0, 255] range
  bool test_ok = true;

  // Test specific corner response (corner should be at position (3,1) or (3,2))
  // The exact values depend on implementation details, so we just check for
  // reasonable response
  uint32_t corner_idx1 = (1 * width + 3) * 2; // (3,1) w measure
  uint32_t corner_idx2 = (2 * width + 3) * 2; // (3,2) w measure

  // Corner areas should have some response (not zero)
  if (result[corner_idx1] == 0 && result[corner_idx2] == 0) {
    printf("❌ Foerstner corner test failed: no corner response detected\n");
    test_ok = false;
  }

  free((void *)result);

  if (test_ok) {
    printf("✅ Foerstner corner test passed\n");
    return 0;
  }
  else {
    printf("❌ Foerstner corner test failed\n");
    return 1;
  }
}

int32_t test_fcv_corner_peaks() {
  // Test with simple synthetic corner response data (5x5 image, 2 channels)
  uint32_t width = 5;
  uint32_t height = 5;

  // Create test data: 2-channel image (w and q measures from Foerstner)
  // Channel 0 (w): corner strength, Channel 1 (q): roundness
  uint8_t data[50]; // 5x5x2 = 50 bytes
  memset(data, 0, 50);

  // Create some artificial corner responses at positions (1,1) and (3,3)
  data[(1 * width + 1) * 2 + 0] = 200; // Strong corner at (1,1)
  data[(1 * width + 1) * 2 + 1] = 150;

  data[(3 * width + 3) * 2 + 0] = 180; // Another corner at (3,3)
  data[(3 * width + 3) * 2 + 1] = 140;

  // Add some weaker responses nearby
  data[(1 * width + 2) * 2 + 0] = 100; // Weaker neighbor
  data[(2 * width + 1) * 2 + 0] = 90;  // Weaker neighbor

  // Test 1: Basic functionality
  CornerPeaks *peaks = fcv_corner_peaks(width, height, data, 1, 0.5, 0.3);

  if (!peaks) {
    printf("❌ Corner peaks test failed: NULL result\n");
    return 1;
  }

  bool test_ok = true;

  // Should find the two strong corners
  if (peaks->count != 2) {
    printf(
      "❌ Corner peaks test failed: expected 2 peaks, got %u\n",
      peaks->count
    );
    test_ok = false;
  }
  else {
    // Check if we found the expected corners
    bool found_corner1 = false, found_corner3 = false;

    for (uint32_t i = 0; i < peaks->count; i++) {
      if (peaks->points[i].x == 1.0 && peaks->points[i].y == 1.0) {
        found_corner1 = true;
      }
      if (peaks->points[i].x == 3.0 && peaks->points[i].y == 3.0) {
        found_corner3 = true;
      }
    }

    if (!found_corner1) {
      printf("❌ Corner peaks test failed: corner at (1,1) not found\n");
      test_ok = false;
    }
    if (!found_corner3) {
      printf("❌ Corner peaks test failed: corner at (3,3) not found\n");
      test_ok = false;
    }
  }

  free_fcv_corner_peaks(peaks);

  // Test 2: Minimum distance suppression
  peaks =
    fcv_corner_peaks(width, height, data, 3, 0.5, 0.3); // min_distance = 3

  if (!peaks) {
    printf("❌ Corner peaks test failed: NULL result on min_distance test\n");
    return 1;
  }

  // With min_distance=3, the corners at (1,1) and (3,3) are distance 2.83
  // apart, so they should both be kept. But if we had closer corners, one would
  // be suppressed.
  if (peaks->count > 2) {
    printf(
      "❌ Corner peaks test failed: too many peaks with min_distance=3: %u\n",
      peaks->count
    );
    test_ok = false;
  }

  free_fcv_corner_peaks(peaks);

  // Test 3: High threshold (should find no peaks)
  peaks = fcv_corner_peaks(width, height, data, 1, 0.98, 0.98);

  if (!peaks) {
    printf("❌ Corner peaks test failed: NULL result on high threshold test\n");
    return 1;
  }

  if (peaks->count != 0) {
    printf(
      "❌ Corner peaks test failed: expected 0 peaks with high threshold, got "
      "%u\n",
      peaks->count
    );
    test_ok = false;
  }

  free_fcv_corner_peaks(peaks);

  // Test 4: NULL input
  peaks = fcv_corner_peaks(width, height, NULL, 1, 0.5, 0.3);
  if (peaks != NULL) {
    printf("❌ Corner peaks test failed: should return NULL for NULL input\n");
    test_ok = false;
    free_fcv_corner_peaks(peaks);
  }

  if (test_ok) {
    printf("✅ Corner peaks test passed\n");
    return 0;
  }
  else {
    printf("❌ Corner peaks test failed\n");
    return 1;
  }
}

int32_t test_fcv_binary_closing_disk() {
  // Test 1: Basic functionality with simple binary image
  uint32_t width = 7;
  uint32_t height = 7;
  int32_t radius = 1;

  // Create test binary image with a small gap using 0/1 pattern
  int pattern[49] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1,
                     0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1,
                     0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

  uint8_t *data = create_binary_image(pattern, width, height);
  if (!data) {
    printf("❌ Binary closing disk test failed: could not create test data\n");
    return 1;
  }

  uint8_t const *result = fcv_binary_closing_disk(data, width, height, radius);

  if (!result) {
    printf("❌ Binary closing disk test failed: NULL result\n");
    free(data);
    return 1;
  }

  bool test_ok = true;

  // After closing with radius 1, the gap in the middle should be filled
  // Check some key positions that should be white after closing
  int32_t center_idx = 3 * width + 3; // Center position (3,3)
  if (result[center_idx] != 255) {
    printf("❌ Binary closing disk test failed: gap not closed at center\n");
    test_ok = false;
  }

  // Check that corners are still black
  if (result[0] != 0 || result[6] != 0 || result[42] != 0 || result[48] != 0) {
    printf("❌ Binary closing disk test failed: corners should remain black\n");
    test_ok = false;
  }

  free((void *)result);

  // Test 2: Edge case - radius 0 (should return copy of original)
  result = fcv_binary_closing_disk(data, width, height, 0);
  if (!result) {
    printf("❌ Binary closing disk test failed: NULL result for radius 0\n");
    free(data);
    return 1;
  }

  // With radius 0, image should be unchanged
  for (uint32_t i = 0; i < width * height; i++) {
    if (result[i] != data[i]) {
      printf(
        "❌ Binary closing disk test failed: radius 0 should not change image\n"
      );
      test_ok = false;
      break;
    }
  }

  free((void *)result);

  // Test 3: NULL input
  result = fcv_binary_closing_disk(NULL, width, height, radius);
  if (result != NULL) {
    printf(
      "❌ Binary closing disk test failed: should return NULL for NULL input\n"
    );
    test_ok = false;
    free((void *)result);
  }

  // Test 4: Invalid dimensions
  result = fcv_binary_closing_disk(data, 0, height, radius);
  if (result != NULL) {
    printf(
      "❌ Binary closing disk test failed: should return NULL for width 0\n"
    );
    test_ok = false;
    free((void *)result);
  }

  result = fcv_binary_closing_disk(data, width, 0, radius);
  if (result != NULL) {
    printf(
      "❌ Binary closing disk test failed: should return NULL for height 0\n"
    );
    test_ok = false;
    free((void *)result);
  }

  // Test 5: Negative radius
  result = fcv_binary_closing_disk(data, width, height, -1);
  if (result != NULL) {
    printf(
      "❌ Binary closing disk test failed: should return NULL for negative "
      "radius\n"
    );
    test_ok = false;
    free((void *)result);
  }

  // Clean up the test data
  free(data);

  if (test_ok) {
    printf("✅ Binary closing disk test passed\n");
    return 0;
  }
  else {
    printf("❌ Binary closing disk test failed\n");
    return 1;
  }
}

int32_t test_fcv_trim() {
  bool test_ok = true;

  // Test 1: Image with uniform border that can be trimmed
  {
    int32_t width = 5;
    int32_t height = 5;

    // Create 5x5 RGBA image with black border and mixed center using pattern
    char pattern[25] = {
      '0', '0', '0', '0', '0', // Row 0: all black border
      '0', '1', 'R', '1', '0', // Row 1: black border, mixed center
      '0', 'G', '1', 'G', '0', // Row 2: black border, mixed center
      '0', '1', 'B', '1', '0', // Row 3: black border, mixed center
      '0', '0', '0', '0', '0'  // Row 4: all black border
    };

    uint8_t *data = create_rgba_pattern_image(pattern, width, height);
    if (!data) {
      printf("❌ Trim test failed: could not create test data\n");
      test_ok = false;
      goto next_test;
    }

    int32_t result_width = width;
    int32_t result_height = height;

    uint8_t *result = fcv_trim(&result_width, &result_height, 4, data);

    if (!result) {
      printf("❌ Trim test failed: NULL result for uniform border\n");
      test_ok = false;
    }
    else {
      // Should trim to 3x3 (removes 1-pixel black border from all sides)
      if (result_width != 3 || result_height != 3) {
        printf(
          "❌ Trim test failed: expected 3x3, got %dx%d\n",
          result_width,
          result_height
        );
        test_ok = false;
      }

      // Check that the trimmed result has the expected mixed center content
      if (result_width == 3 && result_height == 3) {
        // Expected 3x3 center: W R W / G W G / W B W
        uint8_t expected[36] = {
          255, 255, 255, 255, 255, 0,
          0,   255, 255, 255, 255, 255, // Row 1: W R W
          0,   255, 0,   255, 255, 255,
          255, 255, 0,   255, 0,   255, // Row 2: G W G
          255, 255, 255, 255, 0,   0,
          255, 255, 255, 255, 255, 255 // Row 3: W B W
        };

        bool content_correct = true;
        for (int32_t i = 0; i < 36; i++) {
          if (result[i] != expected[i]) {
            content_correct = false;
            break;
          }
        }
        if (!content_correct) {
          printf(
            "❌ Trim test failed: trimmed content doesn't match expected\n"
          );
          test_ok = false;
        }
      }

      free(result);
    }

    free(data);
  }

next_test:

  // Test 2: Image with no uniform border (should return copy)
  {
    int32_t width = 3;
    int32_t height = 3;

    // Create 3x3 RGBA image with mixed colors, no uniform border
    uint8_t data[36] = {255, 0,   0,   255, 0,   255, 0,   255, 0,
                        0,   255, 255, 0,   255, 255, 255, 128, 128,
                        128, 255, 255, 255, 0,   255, 255, 0,   255,
                        255, 0,   128, 255, 255, 128, 0,   128, 255};

    int32_t result_width = width;
    int32_t result_height = height;

    uint8_t *result = fcv_trim(&result_width, &result_height, 4, data);

    if (!result) {
      printf("❌ Trim test failed: NULL result for mixed colors\n");
      test_ok = false;
    }
    else {
      // Dimensions should be unchanged
      if (result_width != width || result_height != height) {
        printf("❌ Trim test failed: dimensions changed when they shouldn't\n");
        test_ok = false;
      }

      // Data should be identical to input
      for (int32_t i = 0; i < width * height * 4; i++) {
        if (result[i] != data[i]) {
          printf(
            "❌ Trim test failed: data changed when no trimming should occur\n"
          );
          test_ok = false;
          break;
        }
      }

      free(result);
    }
  }

  // Test 3: NULL input
  {
    int32_t width = 5;
    int32_t height = 5;
    uint8_t *result = fcv_trim(&width, &height, 4, NULL);
    if (result != NULL) {
      printf("❌ Trim test failed: should return NULL for NULL input\n");
      test_ok = false;
      free(result);
    }
  }

  // Test 4: Invalid dimensions
  {
    uint8_t data[16] =
      {0, 0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255};
    int32_t width = 0;
    int32_t height = 2;
    uint8_t *result = fcv_trim(&width, &height, 4, data);
    if (result != NULL) {
      printf("❌ Trim test failed: should return NULL for width 0\n");
      test_ok = false;
      free(result);
    }
  }

  if (test_ok) {
    printf("✅ Trim test passed\n");
    return 0;
  }
  else {
    printf("❌ Trim test failed\n");
    return 1;
  }
}

int32_t test_fcv_histogram() {
  bool test_ok = true;

  // Test 1: Basic functionality with RGB image
  {
    uint32_t width = 2;
    uint32_t height = 2;

    // Create simple RGBA test data with known values
    uint8_t data[16] = {
      255,
      0,
      0,
      255, // Red pixel
      0,
      255,
      0,
      255, // Green pixel
      0,
      0,
      255,
      255, // Blue pixel
      128,
      128,
      128,
      255 // Gray pixel
    };

    uint32_t hist_width, hist_height;
    uint8_t *result =
      fcv_generate_histogram(width, height, 4, data, &hist_width, &hist_height);

    if (!result) {
      printf("❌ Histogram test failed: NULL result for RGB image\n");
      test_ok = false;
    }
    else {
      // Check output dimensions
      if (hist_width != 256 ||
          hist_height != 200) { // 256 width, 200 height (no margins)
        printf(
          "❌ Histogram test failed: incorrect output dimensions %dx%d\n",
          hist_width,
          hist_height
        );
        test_ok = false;
      }

      // Verify histogram image is created (should have non-zero values)
      bool has_content = false;
      for (uint32_t i = 0; i < hist_width * hist_height * 4; i += 4) {
        if (result[i] > 0 || result[i + 1] > 0 || result[i + 2] > 0) {
          has_content = true;
          break;
        }
      }

      if (!has_content) {
        printf("❌ Histogram test failed: histogram image appears empty\n");
        test_ok = false;
      }

      free(result);
    }
  }

  // Test 2: Grayscale image (all RGB channels equal)
  {
    uint32_t width = 2;
    uint32_t height = 2;

    // Create grayscale RGBA test data (all RGB channels equal)
    uint8_t data[16] = {
      0,
      0,
      0,
      255, // Black pixel
      64,
      64,
      64,
      255, // Dark gray pixel
      128,
      128,
      128,
      255, // Medium gray pixel
      255,
      255,
      255,
      255 // White pixel
    };

    uint32_t hist_width, hist_height;
    uint8_t *result =
      fcv_generate_histogram(width, height, 4, data, &hist_width, &hist_height);

    if (!result) {
      printf("❌ Histogram test failed: NULL result for grayscale image\n");
      test_ok = false;
    }
    else {
      // For grayscale, histogram should be white bars on black background
      bool has_white_content = false;
      for (uint32_t i = 0; i < hist_width * hist_height * 4; i += 4) {
        if (result[i] == 255 && result[i + 1] == 255 && result[i + 2] == 255) {
          has_white_content = true;
          break;
        }
      }

      if (!has_white_content) {
        printf(
          "❌ Histogram test failed: grayscale histogram should have white "
          "bars\n"
        );
        test_ok = false;
      }

      free(result);
    }
  }

  // Test 3: NULL input
  {
    uint32_t hist_width, hist_height;
    uint8_t *result =
      fcv_generate_histogram(10, 10, 4, NULL, &hist_width, &hist_height);
    if (result != NULL) {
      printf("❌ Histogram test failed: should return NULL for NULL input\n");
      test_ok = false;
      free(result);
    }
  }

  // Test 4: NULL output parameters
  {
    uint8_t data[16] =
      {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 128, 128, 128, 255};
    uint8_t *result = fcv_generate_histogram(2, 2, 4, data, NULL, NULL);
    if (result != NULL) {
      printf(
        "❌ Histogram test failed: should return NULL for NULL output "
        "parameters\n"
      );
      test_ok = false;
      free(result);
    }
  }

  if (test_ok) {
    printf("✅ Histogram test passed\n");
    return 0;
  }
  else {
    printf("❌ Histogram test failed\n");
    return 1;
  }
}

int32_t test_fcv_add_border() {
  printf("Testing fcv_add_border...\n");

  // Create a simple 2x2 RGBA test image (blue pixels)
  uint8_t input_data[] = {
    0,
    0,
    255,
    255, // Blue pixel (top-left)
    0,
    0,
    255,
    255, // Blue pixel (top-right)
    0,
    0,
    255,
    255, // Blue pixel (bottom-left)
    0,
    0,
    255,
    255 // Blue pixel (bottom-right)
  };

  uint32_t input_width = 2;
  uint32_t input_height = 2;
  uint32_t output_width, output_height;

  // Test 1: Add red border with width 1
  uint8_t *result = fcv_add_border(
    input_width,
    input_height,
    4,
    "FF0000",
    1,
    input_data,
    &output_width,
    &output_height
  );

  if (!result) {
    printf("❌ Border test failed: function returned NULL\n");
    return 1;
  }

  // Check output dimensions
  if (output_width != 4 || output_height != 4) {
    printf(
      "❌ Border test failed: expected 4x4, got %dx%d\n",
      output_width,
      output_height
    );
    free(result);
    return 1;
  }

  // Check corner pixels (should be red border)
  uint8_t *top_left = &result[0];
  if (top_left[0] != 255 || top_left[1] != 0 || top_left[2] != 0 ||
      top_left[3] != 255) {
    printf("❌ Border test failed: top-left pixel is not red\n");
    free(result);
    return 1;
  }

  // Check center pixels (should be original blue)
  uint8_t *center = &result[((1 * output_width) + 1) * 4]; // Position (1,1)
  if (center[0] != 0 || center[1] != 0 || center[2] != 255 ||
      center[3] != 255) {
    printf("❌ Border test failed: center pixel is not blue\n");
    free(result);
    return 1;
  }

  free(result);

  // Test 2: Border width 0 (should fail)
  result = fcv_add_border(
    input_width,
    input_height,
    4,
    "FF0000",
    0,
    input_data,
    &output_width,
    &output_height
  );

  if (result != NULL) {
    printf("❌ Border test failed: width 0 should return NULL\n");
    free(result);
    return 1;
  }

  // Test 3: NULL input (should fail)
  result = fcv_add_border(
    input_width,
    input_height,
    4,
    "FF0000",
    1,
    NULL,
    &output_width,
    &output_height
  );

  if (result != NULL) {
    printf("❌ Border test failed: NULL input should return NULL\n");
    free(result);
    return 1;
  }

  printf("✅ Border test passed\n");
  return 0;
}

int32_t test_sort_corners() {
  bool test_ok = true;

  // Test 1: Basic corner sorting with known coordinates
  {
    Point2D corners[4] = {
      {720, 956}, // bottom-right
      {332, 68},  // top-left
      {692, 76},  // top-right
      {352, 960}  // bottom-left
    };

    Point2D result[4];

    // Test with scaling (1024x1024 -> 1024x1024, no scaling)
    Corners sorted = sort_corners(1024, 1024, 1024, 1024, corners, 4, result);

    // Check the sorted corners are in the correct order: tl, tr, br, bl
    if (fabs(sorted.tl_x - 332) > 0.1 || fabs(sorted.tl_y - 68) > 0.1) {
      printf(
        "❌ Sort corners test failed: top-left incorrect (%.1f,%.1f)\n",
        sorted.tl_x,
        sorted.tl_y
      );
      test_ok = false;
    }
    if (fabs(sorted.tr_x - 692) > 0.1 || fabs(sorted.tr_y - 76) > 0.1) {
      printf(
        "❌ Sort corners test failed: top-right incorrect (%.1f,%.1f)\n",
        sorted.tr_x,
        sorted.tr_y
      );
      test_ok = false;
    }
    if (fabs(sorted.br_x - 720) > 0.1 || fabs(sorted.br_y - 956) > 0.1) {
      printf(
        "❌ Sort corners test failed: bottom-right incorrect (%.1f,%.1f)\n",
        sorted.br_x,
        sorted.br_y
      );
      test_ok = false;
    }
    if (fabs(sorted.bl_x - 352) > 0.1 || fabs(sorted.bl_y - 960) > 0.1) {
      printf(
        "❌ Sort corners test failed: bottom-left incorrect (%.1f,%.1f)\n",
        sorted.bl_x,
        sorted.bl_y
      );
      test_ok = false;
    }

    // Check the result array is also correctly sorted
    if (result[0].x != 332 || result[0].y != 68) {
      printf("❌ Sort corners test failed: result[0] top-left incorrect\n");
      test_ok = false;
    }
    if (result[1].x != 692 || result[1].y != 76) {
      printf("❌ Sort corners test failed: result[1] top-right incorrect\n");
      test_ok = false;
    }
    if (result[2].x != 720 || result[2].y != 956) {
      printf("❌ Sort corners test failed: result[2] bottom-right incorrect\n");
      test_ok = false;
    }
    if (result[3].x != 352 || result[3].y != 960) {
      printf("❌ Sort corners test failed: result[3] bottom-left incorrect\n");
      test_ok = false;
    }
  }

  // Test 2: Scaling test
  {
    Point2D corners[4] = {
      {360, 478}, // bottom-right (scaled from 720, 956)
      {166, 34},  // top-left (scaled from 332, 68)
      {346, 38},  // top-right (scaled from 692, 76)
      {176, 480}  // bottom-left (scaled from 352, 960)
    };

    Point2D result[4];

    // Test with 2x upscaling (512x512 -> 1024x1024)
    Corners sorted = sort_corners(1024, 1024, 512, 512, corners, 4, result);

    // Check the scaling is applied correctly (should be doubled)
    if (fabs(sorted.tl_x - 332) > 1.0 || fabs(sorted.tl_y - 68) > 1.0) {
      printf(
        "❌ Sort corners scaling test failed: top-left incorrect (%.1f,%.1f)\n",
        sorted.tl_x,
        sorted.tl_y
      );
      test_ok = false;
    }
    if (fabs(sorted.tr_x - 692) > 1.0 || fabs(sorted.tr_y - 76) > 1.0) {
      printf(
        "❌ Sort corners scaling test failed: top-right incorrect "
        "(%.1f,%.1f)\n",
        sorted.tr_x,
        sorted.tr_y
      );
      test_ok = false;
    }
  }

  // Test 3: Three corners case - parallelogram assumption
  {
    // Test missing bottom-right corner (provide tl, tr, bl)
    Point2D corners[3] = {
      {100, 100}, // top-left
      {200, 100}, // top-right
      {100, 200}  // bottom-left
    };
    Point2D result[4];

    Corners sorted = sort_corners(1024, 1024, 1024, 1024, corners, 3, result);

    // Expected bottom-right should be (200, 200) to complete the parallelogram
    if (fabs(sorted.tl_x - 100) > 0.1 || fabs(sorted.tl_y - 100) > 0.1) {
      printf(
        "❌ Sort corners test failed: 3 corners case - top-left incorrect "
        "(%.1f,%.1f)\n",
        sorted.tl_x,
        sorted.tl_y
      );
      test_ok = false;
    }
    if (fabs(sorted.tr_x - 200) > 0.1 || fabs(sorted.tr_y - 100) > 0.1) {
      printf(
        "❌ Sort corners test failed: 3 corners case - top-right incorrect "
        "(%.1f,%.1f)\n",
        sorted.tr_x,
        sorted.tr_y
      );
      test_ok = false;
    }
    if (fabs(sorted.bl_x - 100) > 0.1 || fabs(sorted.bl_y - 200) > 0.1) {
      printf(
        "❌ Sort corners test failed: 3 corners case - bottom-left incorrect "
        "(%.1f,%.1f)\n",
        sorted.bl_x,
        sorted.bl_y
      );
      test_ok = false;
    }
    if (fabs(sorted.br_x - 200) > 0.1 || fabs(sorted.br_y - 200) > 0.1) {
      printf(
        "❌ Sort corners test failed: 3 corners case - bottom-right incorrect "
        "(%.1f,%.1f)\n",
        sorted.br_x,
        sorted.br_y
      );
      test_ok = false;
    }
  }

  // Test 4: Three corners case - missing top-left
  {
    Point2D corners[3] = {
      {200, 100}, // top-right
      {200, 200}, // bottom-right
      {100, 200}  // bottom-left
    };
    Point2D result[4];

    Corners sorted = sort_corners(1024, 1024, 1024, 1024, corners, 3, result);

    // Expected top-left should be (100, 100) to complete the parallelogram
    if (fabs(sorted.tl_x - 100) > 0.1 || fabs(sorted.tl_y - 100) > 0.1) {
      printf(
        "❌ Sort corners test failed: 3 corners missing TL - top-left "
        "incorrect (%.1f,%.1f)\n",
        sorted.tl_x,
        sorted.tl_y
      );
      test_ok = false;
    }
  }

  // Test 5: Edge cases - insufficient corners (less than 3)
  {
    Point2D corners[2] = {{100, 100}, {200, 200}};
    Point2D result[4];

    Corners sorted = sort_corners(1024, 1024, 1024, 1024, corners, 2, result);

    // Should return all zeros
    if (sorted.tl_x != 0 || sorted.tl_y != 0 || sorted.tr_x != 0 ||
        sorted.tr_y != 0 || sorted.br_x != 0 || sorted.br_y != 0 ||
        sorted.bl_x != 0 || sorted.bl_y != 0) {
      printf(
        "❌ Sort corners test failed: insufficient corners should return "
        "zeros\n"
      );
      test_ok = false;
    }
  }

  // Test 6: More than 4 corners (should use only first 4)
  {
    Point2D corners[6] = {
      {720, 956}, // bottom-right
      {332, 68},  // top-left
      {692, 76},  // top-right
      {352, 960}, // bottom-left
      {400, 400}, // extra corner 1
      {500, 500}  // extra corner 2
    };

    Point2D result[4];

    Corners sorted = sort_corners(1024, 1024, 1024, 1024, corners, 6, result);

    // Should still sort correctly using first 4
    if (fabs(sorted.tl_x - 332) > 0.1 || fabs(sorted.tl_y - 68) > 0.1) {
      printf(
        "❌ Sort corners test failed: extra corners case - top-left incorrect\n"
      );
      test_ok = false;
    }
    if (fabs(sorted.tr_x - 692) > 0.1 || fabs(sorted.tr_y - 76) > 0.1) {
      printf(
        "❌ Sort corners test failed: extra corners case - top-right "
        "incorrect\n"
      );
      test_ok = false;
    }
  }

  // Test 7: Perfect square corners (simple geometric case)
  {
    Point2D corners[4] = {
      {100, 100}, // top-left
      {200, 100}, // top-right
      {200, 200}, // bottom-right
      {100, 200}  // bottom-left
    };

    Point2D result[4];

    Corners sorted = sort_corners(1024, 1024, 1024, 1024, corners, 4, result);

    // Verify perfect square is sorted correctly
    if (sorted.tl_x != 100 || sorted.tl_y != 100) {
      printf(
        "❌ Sort corners test failed: square case - top-left incorrect "
        "(%.1f,%.1f)\n",
        sorted.tl_x,
        sorted.tl_y
      );
      test_ok = false;
    }
    if (sorted.tr_x != 200 || sorted.tr_y != 100) {
      printf(
        "❌ Sort corners test failed: square case - top-right incorrect "
        "(%.1f,%.1f)\n",
        sorted.tr_x,
        sorted.tr_y
      );
      test_ok = false;
    }
    if (sorted.br_x != 200 || sorted.br_y != 200) {
      printf(
        "❌ Sort corners test failed: square case - bottom-right incorrect "
        "(%.1f,%.1f)\n",
        sorted.br_x,
        sorted.br_y
      );
      test_ok = false;
    }
    if (sorted.bl_x != 100 || sorted.bl_y != 200) {
      printf(
        "❌ Sort corners test failed: square case - bottom-left incorrect "
        "(%.1f,%.1f)\n",
        sorted.bl_x,
        sorted.bl_y
      );
      test_ok = false;
    }
  }

  // Test 8: More than 4 corners (pentagon case)
  {
    Point2D corners[5] = {
      {150, 50},  // top
      {250, 100}, // top-right
      {200, 200}, // bottom-right
      {100, 200}, // bottom-left
      {50, 100}   // left
    };

    Point2D result[5];

    // Note: The Corners struct still only handles first 4 corners
    // but the result array should contain all 5 sorted clockwise
    Corners sorted = sort_corners(1024, 1024, 1024, 1024, corners, 5, result);

    // Just verify the function doesn't crash and produces reasonable output
    // Top-left should still be one of the leftmost/topmost corners
    if (sorted.tl_x < 40 || sorted.tl_x > 160) {
      printf(
        "❌ Sort corners test failed: 5 corners case - unreasonable top-left "
        "x=%.1f\n",
        sorted.tl_x
      );
      test_ok = false;
    }
    if (sorted.tl_y < 40 || sorted.tl_y > 120) {
      printf(
        "❌ Sort corners test failed: 5 corners case - unreasonable top-left "
        "y=%.1f\n",
        sorted.tl_y
      );
      test_ok = false;
    }
  }

  // Test 9: Six corners (hexagon case)
  {
    Point2D corners[6] = {
      {150, 50},  // top
      {200, 75},  // top-right
      {225, 150}, // right
      {175, 200}, // bottom-right
      {125, 200}, // bottom-left
      {75, 125}   // left
    };

    Point2D result[6];

    // Test with 6 corners
    Corners sorted = sort_corners(1024, 1024, 1024, 1024, corners, 6, result);

    // Verify function doesn't crash and produces reasonable output
    if (sorted.tl_x < 70 || sorted.tl_x > 160) {
      printf(
        "❌ Sort corners test failed: 6 corners case - unreasonable top-left "
        "x=%.1f\n",
        sorted.tl_x
      );
      test_ok = false;
    }
  }

  if (test_ok) {
    printf("✅ Sort corners test passed\n");
    return 0;
  }
  else {
    printf("❌ Sort corners test failed\n");
    return 1;
  }
}

int32_t main() {
  if (!test_otsu_threshold() && !test_perspective_transform() &&
      !test_perspective_transform_float() && !test_fcv_foerstner_corner() &&
      !test_fcv_corner_peaks() && !test_fcv_binary_closing_disk() &&
      !test_fcv_trim() && !test_fcv_histogram() && !test_fcv_add_border() &&
      !test_sort_corners()) {
    printf("✅ All tests passed\n");
    return 0;
  }
  else {
    printf("❌ Some tests failed\n");
    return 1;
  }
}
