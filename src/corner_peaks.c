#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "1_types.h"
#include "conversion.h"
#include "corner_peaks.h"
#else
#include "flatcv.h"
#endif

static bool is_local_maximum(
  uint8_t const *data,
  uint32_t width,
  uint32_t height,
  uint32_t x,
  uint32_t y,
  uint32_t channel
) {
  if (x == 0 || y == 0 || x >= width - 1 || y >= height - 1) {
    return false;
  }

  uint32_t center_idx = (y * width + x) * 2 + channel;
  uint8_t center_val = data[center_idx];

  if (center_val == 0) {
    return false;
  }

  for (int32_t dy = -1; dy <= 1; dy++) {
    for (int32_t dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) {
        continue;
      }

      uint32_t neighbor_idx = ((y + dy) * width + (x + dx)) * 2 + channel;
      if (data[neighbor_idx] > center_val) {
        return false;
      }
    }
  }

  return true;
}

static double euclidean_distance(Point2D a, Point2D b) {
  double dx = a.x - b.x;
  double dy = a.y - b.y;
  return sqrt(dx * dx + dy * dy);
}

/** Detect corner peaks in the image.
 * This function finds local maxima in the corner response image
 * and filters them based on specified thresholds.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param data Point32_ter to the 2 channel (w and q) corner response data.
 * @param min_distance Minimum distance between peaks.
 * @param accuracy_thresh Threshold for accuracy measure (w).
 * @param roundness_thresh Threshold for roundness measure (q).
 * @return Point32_ter to CornerPeaks structure containing detected peaks.
 */
CornerPeaks *corner_peaks(
  uint32_t width,
  uint32_t height,
  uint8_t const *data,
  uint32_t min_distance,
  double accuracy_thresh,
  double roundness_thresh
) {
  if (!data) {
    return NULL;
  }

  Point2D *candidates = malloc(sizeof(Point2D) * width * height);
  if (!candidates) {
    return NULL;
  }

  uint32_t candidate_count = 0;

  for (uint32_t y = 1; y < height - 1; y++) {
    for (uint32_t x = 1; x < width - 1; x++) {
      uint32_t idx = (y * width + x) * 2;

      // Convert normalized values (0-255) back to 0-1 range
      double w = data[idx] / 255.0;     // accuracy measure
      double q = data[idx + 1] / 255.0; // roundness measure

      if (q > roundness_thresh && w > accuracy_thresh &&
          is_local_maximum(data, width, height, x, y, 0)) {
        candidates[candidate_count].x = (double)x;
        candidates[candidate_count].y = (double)y;
        candidate_count++;
      }
    }
  }

  if (candidate_count == 0) {
    free(candidates);
    CornerPeaks *result = malloc(sizeof(CornerPeaks));
    if (result) {
      result->points = NULL;
      result->count = 0;
    }
    return result;
  }

  bool *rejected = calloc(candidate_count, sizeof(bool));
  if (!rejected) {
    free(candidates);
    return NULL;
  }

  for (uint32_t i = 0; i < candidate_count; i++) {
    if (rejected[i]) {
      continue;
    }

    for (uint32_t j = i + 1; j < candidate_count; j++) {
      if (rejected[j]) {
        continue;
      }

      if (euclidean_distance(candidates[i], candidates[j]) < min_distance) {
        uint32_t i_idx =
          ((uint32_t)candidates[i].y * width + (uint32_t)candidates[i].x) * 2;
        uint32_t j_idx =
          ((uint32_t)candidates[j].y * width + (uint32_t)candidates[j].x) * 2;

        if (data[i_idx] >= data[j_idx]) {
          rejected[j] = true;
        }
        else {
          rejected[i] = true;
          break;
        }
      }
    }
  }

  uint32_t final_count = 0;
  for (uint32_t i = 0; i < candidate_count; i++) {
    if (!rejected[i]) {
      final_count++;
    }
  }

  Point2D *final_points = malloc(sizeof(Point2D) * final_count);
  if (!final_points) {
    free(candidates);
    free(rejected);
    return NULL;
  }

  uint32_t idx = 0;
  for (uint32_t i = 0; i < candidate_count; i++) {
    if (!rejected[i]) {
      final_points[idx] = candidates[i];
      idx++;
    }
  }

  free(candidates);
  free(rejected);

  CornerPeaks *result = malloc(sizeof(CornerPeaks));
  if (!result) {
    free(final_points);
    return NULL;
  }

  result->points = final_points;
  result->count = final_count;

  return result;
}
