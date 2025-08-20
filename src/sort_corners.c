#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef FLATCV_AMALGAMATION
#include "1_types.h"
#else
#include "flatcv.h"
#endif

typedef struct {
  double r;     // radius
  double theta; // angle
} PolarPoint;

// Helper function to convert Cartesian to polar coordinates
PolarPoint cartesian_to_polar(Point2D cart) {
  PolarPoint polar;
  polar.r = sqrt(cart.x * cart.x + cart.y * cart.y);
  polar.theta = atan2(cart.y, cart.x);
  return polar;
}

// Comparison function for qsort (sorting by angle in descending order)
int compare_polar_angles(const void *a, const void *b) {
  const PolarPoint *pa = (const PolarPoint *)a;
  const PolarPoint *pb = (const PolarPoint *)b;

  if (pa->theta < pb->theta) {
    return 1; // Descending order
  }
  if (pa->theta > pb->theta) {
    return -1;
  }
  return 0;
}

// Helper structure to keep track of original indices during sorting
typedef struct {
  PolarPoint polar;
  uint32_t original_index;
} IndexedPolarPoint;

int compare_indexed_polar_angles(const void *a, const void *b) {
  const IndexedPolarPoint *pa = (const IndexedPolarPoint *)a;
  const IndexedPolarPoint *pb = (const IndexedPolarPoint *)b;

  if (pa->polar.theta < pb->polar.theta) {
    return 1; // Descending order
  }
  if (pa->polar.theta > pb->polar.theta) {
    return -1;
  }
  return 0;
}

Corners sort_corners(
  uint32_t width,
  uint32_t height,
  uint32_t out_width,
  uint32_t out_height,
  Point2D *corners,
  uint32_t num_corners,
  Point2D *result
) {
  // Simple geometric approach to identify corners
  // Assume we have exactly 4 corners for a document
  if (num_corners != 4) {
    // Fallback: just use the first 4 corners if more are provided
    num_corners = 4;
  }

  // Find corner positions based on coordinate sums and differences
  Point2D top_left = corners[0];
  Point2D top_right = corners[0];
  Point2D bottom_left = corners[0];
  Point2D bottom_right = corners[0];

  for (uint32_t i = 0; i < num_corners; i++) {
    // Top-left: minimum sum (x + y)
    if (corners[i].x + corners[i].y < top_left.x + top_left.y) {
      top_left = corners[i];
    }
    // Top-right: minimum difference (y - x)
    if (corners[i].y - corners[i].x < top_right.y - top_right.x) {
      top_right = corners[i];
    }
    // Bottom-left: maximum difference (y - x)
    if (corners[i].y - corners[i].x > bottom_left.y - bottom_left.x) {
      bottom_left = corners[i];
    }
    // Bottom-right: maximum sum (x + y)
    if (corners[i].x + corners[i].y > bottom_right.x + bottom_right.y) {
      bottom_right = corners[i];
    }
  }

  // Store results in order: top-left, top-right, bottom-right, bottom-left
  result[0] = top_left;
  result[1] = top_right;
  result[2] = bottom_right;
  result[3] = bottom_left;

  // Scale corners back to original image dimensions
  double scale_x = (double)width / out_width;
  double scale_y = (double)height / out_height;

  // Return corners struct using the sorted result
  Corners sorted_corners_result = {
    .tl_x = result[0].x * scale_x,
    .tl_y = result[0].y * scale_y,
    .tr_x = result[1].x * scale_x,
    .tr_y = result[1].y * scale_y,
    .br_x = result[2].x * scale_x,
    .br_y = result[2].y * scale_y,
    .bl_x = result[3].x * scale_x,
    .bl_y = result[3].y * scale_y
  };

  return sorted_corners_result;
}
