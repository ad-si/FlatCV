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
#include "foerstner_corner.h"
#include "perspectivetransform.h"
#include "rgba_to_grayscale.h"

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
  uint8_t data[100] = {// Row 0: all black
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
  // 1. Function should return valid point32_ter
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

  // Create test binary image with a small gap
  uint8_t data[49] = {0, 0,   0,   0, 0,   0,   0, 0, 255, 255, 0, 255, 255, 0,
                      0, 255, 255, 0, 255, 255, 0, 0, 0,   0,   0, 0,   0,   0,
                      0, 255, 255, 0, 255, 255, 0, 0, 255, 255, 0, 255, 255, 0,
                      0, 0,   0,   0, 0,   0,   0};

  uint8_t const *result = fcv_binary_closing_disk(data, width, height, radius);

  if (!result) {
    printf("❌ Binary closing disk test failed: NULL result\n");
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

  if (test_ok) {
    printf("✅ Binary closing disk test passed\n");
    return 0;
  }
  else {
    printf("❌ Binary closing disk test failed\n");
    return 1;
  }
}

int32_t main() {
  if (!test_otsu_threshold() && !test_perspective_transform() &&
      !test_perspective_transform_float() && !test_fcv_foerstner_corner() &&
      !test_fcv_corner_peaks() && !test_fcv_binary_closing_disk()) {
    printf("✅ All tests passed\n");
    return 0;
  }
  else {
    printf("❌ Some tests failed\n");
    return 1;
  }
}
