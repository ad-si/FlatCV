#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flatcv.h"

int32_t test_amalgamation_basic() {
  uint32_t width = 4;
  uint32_t height = 4;
  uint8_t data[64] = {1, 1, 1, 255, 2, 2, 2, 255, 9, 9, 9, 255, 8, 8, 8, 255,
                      2, 2, 2, 255, 1, 1, 1, 255, 9, 9, 9, 255, 7, 7, 7, 255,
                      2, 2, 2, 255, 0, 0, 0, 255, 8, 8, 8, 255, 2, 2, 2, 255,
                      0, 0, 0, 255, 2, 2, 2, 255, 9, 9, 9, 255, 8, 8, 8, 255};

  uint8_t const *const monochrome_data =
    fcv_otsu_threshold_rgba(width, height, false, data);

  if (!monochrome_data) {
    printf("❌ Amalgamation test failed: NULL result from function\n");
    return 1;
  }

  free((void *)monochrome_data);
  printf("✅ Amalgamation basic test passed\n");
  return 0;
}

int32_t main() {
  printf("Testing amalgamated FlatCV library...\n");

  if (!test_amalgamation_basic()) {
    printf("✅ Amalgamation test passed\n");
    return 0;
  }
  else {
    printf("❌ Amalgamation test failed\n");
    return 1;
  }
}