#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "1_types.h"
#include "flood_fill.h"
#else
#include "flatcv.h"
#endif

/**
 * Flood fill segmentation
 * using a seeded breadth-first traversal with threshold-based inclusion
 *
 * Starting from each marker, expands across neighboring pixels whose
 * grayscale values stay within the given threshold of that marker's value.
 * Every reached pixel is set to `new_value`.
 * Each marker is filled independently (relative to its own seed value),
 * so the result is the union of all individual fills.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param grayscale_data Pointer to the input grayscale image data.
 * @param markers Seeds from which to start the flood fill.
 * @param num_markers Number of seeds.
 * @param threshold Maximum absolute grayscale difference from the seed value.
 * @param new_value Replacement value written to filled pixels.
 * @param connectivity 4 (edge neighbors) or 8 (edge and diagonal neighbors).
 * @return Pointer to the filled image data,
 *   or NULL on invalid input or allocation failure.
 */
uint8_t *fcv_flood_fill(
  uint32_t width,
  uint32_t height,
  uint8_t const *const grayscale_data,
  Point2D const *markers,
  uint32_t num_markers,
  uint8_t threshold,
  uint8_t new_value,
  uint8_t connectivity
) {
  if (!grayscale_data || !markers || num_markers == 0 || width == 0 ||
      height == 0 || width > INT32_MAX || height > INT32_MAX ||
      (connectivity != 4 && connectivity != 8)) {
    return NULL;
  }

  // Range-check as double before casting: out-of-range float-to-int
  // conversion is undefined behavior, and truncation would map e.g. -0.5 to 0.
  for (uint32_t m = 0; m < num_markers; m++) {
    double x = markers[m].x;
    double y = markers[m].y;
    if (!isfinite(x) || !isfinite(y) || x < 0 || y < 0 || x >= (double)width ||
        y >= (double)height) {
      return NULL;
    }
  }

  if (width > SIZE_MAX / height) {
    return NULL;
  }
  size_t img_length_px = (size_t)width * height;
  if (img_length_px > SIZE_MAX / sizeof(size_t)) {
    return NULL;
  }

  uint8_t *output_data = malloc(img_length_px);
  bool *visited = calloc(img_length_px, sizeof(bool));
  // Each pixel is enqueued at most once per seed,
  // so a linear (non-wrapping) queue of image size suffices.
  // After a seed's traversal, it also holds exactly the visited pixels.
  size_t *queue = malloc(img_length_px * sizeof(size_t));
  if (!output_data || !visited || !queue) {
    free(output_data);
    free(visited);
    free(queue);
    return NULL;
  }
  memcpy(output_data, grayscale_data, img_length_px);

  // First 4 entries are edge neighbors, last 4 are diagonals
  int32_t const dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
  int32_t const dy[] = {0, 0, -1, 1, -1, -1, 1, 1};

  for (uint32_t m = 0; m < num_markers; m++) {
    uint32_t seed_x = (uint32_t)markers[m].x;
    uint32_t seed_y = (uint32_t)markers[m].y;
    size_t seed_idx = (size_t)seed_y * width + seed_x;
    uint8_t target_value = grayscale_data[seed_idx];

    size_t head = 0;
    size_t tail = 0;
    queue[tail++] = seed_idx;
    visited[seed_idx] = true;

    while (head < tail) {
      size_t idx = queue[head++];
      output_data[idx] = new_value;

      int32_t x = (int32_t)(idx % width);
      int32_t y = (int32_t)(idx / width);

      for (uint8_t d = 0; d < connectivity; d++) {
        int32_t nx = x + dx[d];
        int32_t ny = y + dy[d];
        if (nx < 0 || nx >= (int32_t)width || ny < 0 || ny >= (int32_t)height) {
          continue;
        }

        size_t neighbor_idx = (size_t)ny * width + (size_t)nx;
        if (visited[neighbor_idx]) {
          continue;
        }

        int32_t diff = (int32_t)grayscale_data[neighbor_idx] - target_value;
        if (diff < 0) {
          diff = -diff;
        }
        if (diff > threshold) {
          continue;
        }

        visited[neighbor_idx] = true;
        queue[tail++] = neighbor_idx;
      }
    }

    // Reset only the pixels visited by this seed,
    // so the next seed can traverse them relative to its own value
    for (size_t i = 0; i < tail; i++) {
      visited[queue[i]] = false;
    }
  }

  free(queue);
  free(visited);
  return output_data;
}
