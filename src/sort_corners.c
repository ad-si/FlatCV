#include <math.h>
#include <stdbool.h>
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
  double theta = atan2(cart.y, cart.x);

  // Normalize theta: Convert to degrees and apply normalization
  double theta_deg = theta * 180.0 / M_PI;
  if (theta_deg < 0) {
    polar.theta = -theta_deg;
  }
  else {
    polar.theta = -(theta_deg - 360.0);
  }

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
  if (num_corners < 3) {
    // Not enough corners, return zeros
    Corners empty_corners = {0, 0, 0, 0, 0, 0, 0, 0};
    return empty_corners;
  }

  // Allocate memory for working corners (max of num_corners or 4 for 3-corner
  // case)
  uint32_t max_corners = (num_corners >= 4) ? num_corners : 4;
  Point2D *working_corners = malloc(max_corners * sizeof(Point2D));
  if (!working_corners) {
    Corners empty_corners = {0, 0, 0, 0, 0, 0, 0, 0};
    return empty_corners;
  }

  uint32_t corners_to_process;

  if (num_corners == 3) {
    // Copy the 3 corners we have
    working_corners[0] = corners[0];
    working_corners[1] = corners[1];
    working_corners[2] = corners[2];

    // Calculate the fourth corner to complete a parallelogram
    // For 3 corners A, B, C, we need to determine which corner is missing
    // and calculate it using vector addition

    // First, sort the 3 corners by position to understand the layout
    Point2D sorted[3] = {corners[0], corners[1], corners[2]};

    // Find bounding box of the 3 corners
    double min_x = sorted[0].x, max_x = sorted[0].x;
    double min_y = sorted[0].y, max_y = sorted[0].y;
    for (int i = 1; i < 3; i++) {
      if (sorted[i].x < min_x) {
        min_x = sorted[i].x;
      }
      if (sorted[i].x > max_x) {
        max_x = sorted[i].x;
      }
      if (sorted[i].y < min_y) {
        min_y = sorted[i].y;
      }
      if (sorted[i].y > max_y) {
        max_y = sorted[i].y;
      }
    }

    // Determine which corner is missing by finding which position in the
    // rectangle is empty
    bool has_tl = false, has_tr = false, has_bl = false, has_br = false;
    for (int i = 0; i < 3; i++) {
      double x = sorted[i].x;
      double y = sorted[i].y;

      // Use small tolerance for floating point comparison
      double tolerance = 1.0;

      if (fabs(x - min_x) < tolerance && fabs(y - min_y) < tolerance) {
        has_tl = true;
      }
      else if (fabs(x - max_x) < tolerance && fabs(y - min_y) < tolerance) {
        has_tr = true;
      }
      else if (fabs(x - min_x) < tolerance && fabs(y - max_y) < tolerance) {
        has_bl = true;
      }
      else if (fabs(x - max_x) < tolerance && fabs(y - max_y) < tolerance) {
        has_br = true;
      }
    }

    // Calculate the missing corner
    Point2D missing_corner;
    if (!has_tl) {
      missing_corner.x = min_x;
      missing_corner.y = min_y;
    }
    else if (!has_tr) {
      missing_corner.x = max_x;
      missing_corner.y = min_y;
    }
    else if (!has_bl) {
      missing_corner.x = min_x;
      missing_corner.y = max_y;
    }
    else if (!has_br) {
      missing_corner.x = max_x;
      missing_corner.y = max_y;
    }
    else {
      // Fallback: use parallelogram completion
      // For a parallelogram ABCD where we have A, B, C and need D:
      // D = A + C - B (assuming B and D are diagonal)
      // Try all combinations and pick the best one
      Point2D candidates[3];
      candidates[0].x = corners[0].x + corners[1].x - corners[2].x;
      candidates[0].y = corners[0].y + corners[1].y - corners[2].y;
      candidates[1].x = corners[0].x + corners[2].x - corners[1].x;
      candidates[1].y = corners[0].y + corners[2].y - corners[1].y;
      candidates[2].x = corners[1].x + corners[2].x - corners[0].x;
      candidates[2].y = corners[1].y + corners[2].y - corners[0].y;

      // Choose the candidate that gives the most reasonable result
      missing_corner = candidates[2]; // Default to the last one
      double best_score = -1;

      for (int i = 0; i < 3; i++) {
        double score = 0;
        if (candidates[i].x >= 0) {
          score += 1;
        }
        if (candidates[i].y >= 0) {
          score += 1;
        }

        // Check if it's not a duplicate
        bool is_duplicate = false;
        for (int j = 0; j < 3; j++) {
          if (fabs(candidates[i].x - corners[j].x) < 0.1 &&
              fabs(candidates[i].y - corners[j].y) < 0.1) {
            is_duplicate = true;
            break;
          }
        }
        if (!is_duplicate) {
          score += 5;
        }

        if (score > best_score) {
          best_score = score;
          missing_corner = candidates[i];
        }
      }
    }

    working_corners[3] = missing_corner;
    corners_to_process = 4;
  }
  else {
    // Copy all existing corners
    for (uint32_t i = 0; i < num_corners; i++) {
      working_corners[i] = corners[i];
    }
    corners_to_process = num_corners;
  }

  // Clockwise corner sorting starting from top-left corner
  // Works with any number of corners >= 3

  // Step 1: Find the center point (centroid)
  double center_x = 0.0, center_y = 0.0;
  for (uint32_t i = 0; i < corners_to_process; i++) {
    center_x += working_corners[i].x;
    center_y += working_corners[i].y;
  }
  center_x /= corners_to_process;
  center_y /= corners_to_process;

  // Step 2: Find the top-left corner (minimum x+y sum)
  uint32_t top_left_idx = 0;
  double min_sum = working_corners[0].x + working_corners[0].y;
  for (uint32_t i = 1; i < corners_to_process; i++) {
    double sum = working_corners[i].x + working_corners[i].y;
    if (sum < min_sum) {
      min_sum = sum;
      top_left_idx = i;
    }
  }

  // Step 3: Create array with indices and calculate angles relative to center
  typedef struct {
    uint32_t index;
    double angle;
    double distance;
  } CornerInfo;

  CornerInfo *corner_info = malloc(corners_to_process * sizeof(CornerInfo));
  if (!corner_info) {
    free(working_corners);
    Corners empty_corners = {0, 0, 0, 0, 0, 0, 0, 0};
    return empty_corners;
  }

  for (uint32_t i = 0; i < corners_to_process; i++) {
    corner_info[i].index = i;
    corner_info[i].distance = sqrt(
      (working_corners[i].x - center_x) * (working_corners[i].x - center_x) +
      (working_corners[i].y - center_y) * (working_corners[i].y - center_y)
    );

    // Calculate angle from center to corner
    double dx = working_corners[i].x - center_x;
    double dy = working_corners[i].y - center_y;
    corner_info[i].angle = atan2(dy, dx);

    // Normalize angle to [0, 2π) range
    if (corner_info[i].angle < 0) {
      corner_info[i].angle += 2.0 * M_PI;
    }
  }

  // Step 4: Calculate reference angle from center to top-left corner
  double top_left_angle = corner_info[top_left_idx].angle;

  // Step 5: Adjust all angles relative to top-left corner and sort clockwise
  for (uint32_t i = 0; i < corners_to_process; i++) {
    // Adjust angle relative to top-left corner
    corner_info[i].angle = corner_info[i].angle - top_left_angle;

    // Normalize to [0, 2π) range
    while (corner_info[i].angle < 0) {
      corner_info[i].angle += 2.0 * M_PI;
    }
    while (corner_info[i].angle >= 2.0 * M_PI) {
      corner_info[i].angle -= 2.0 * M_PI;
    }
  }

  // Step 6: Sort corners by angle (clockwise from top-left)
  for (uint32_t i = 0; i < corners_to_process - 1; i++) {
    for (uint32_t j = 0; j < corners_to_process - 1 - i; j++) {
      bool should_swap = false;

      if (corner_info[j].angle > corner_info[j + 1].angle) {
        should_swap = true;
      }
      else if (fabs(corner_info[j].angle - corner_info[j + 1].angle) < 1e-6) {
        // If angles are very close, sort by distance from center (closer first)
        if (corner_info[j].distance > corner_info[j + 1].distance) {
          should_swap = true;
        }
      }

      if (should_swap) {
        CornerInfo temp = corner_info[j];
        corner_info[j] = corner_info[j + 1];
        corner_info[j + 1] = temp;
      }
    }
  }

  // Step 7: Copy sorted corners to result
  for (uint32_t i = 0; i < corners_to_process; i++) {
    result[i] = working_corners[corner_info[i].index];
  }

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

  // Clean up allocated memory
  free(corner_info);
  free(working_corners);

  return sorted_corners_result;
}
