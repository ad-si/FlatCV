#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "1_types.h"
#include "watershed_segmentation.h"
#else
#include "flatcv.h"
#endif

typedef struct {
  int32_t x, y;
  int32_t label;
} QueueItem;

typedef struct {
  QueueItem *items;
  int32_t front;
  int32_t rear;
  int32_t size;
  int32_t capacity;
} Queue;

static Queue *create_queue(int32_t capacity) {
  Queue *q = malloc(sizeof(Queue));
  if (!q) {
    return NULL;
  }

  q->items = malloc(capacity * sizeof(QueueItem));
  if (!q->items) {
    free(q);
    return NULL;
  }

  q->front = 0;
  q->rear = -1;
  q->size = 0;
  q->capacity = capacity;
  return q;
}

static void destroy_queue(Queue *q) {
  if (q) {
    free(q->items);
    free(q);
  }
}

static int32_t is_queue_empty(Queue *q) { return q->size == 0; }

static int32_t enqueue(Queue *q, int32_t x, int32_t y, int32_t label) {
  if (q->size >= q->capacity) {
    return 0;
  }

  q->rear = (q->rear + 1) % q->capacity;
  q->items[q->rear].x = x;
  q->items[q->rear].y = y;
  q->items[q->rear].label = label;
  q->size++;
  return 1;
}

static QueueItem dequeue(Queue *q) {
  QueueItem item = {-1, -1, -1};
  if (q->size == 0) {
    return item;
  }

  item = q->items[q->front];
  q->front = (q->front + 1) % q->capacity;
  q->size--;
  return item;
}

/**
 * Watershed segmentation
 * using (x, y) coordinate markers with elevation-based flooding
 *
 * Implements the watershed transform for image segmentation by treating the
 * grayscale image as an elevation map. Water floods from the marker points,
 * and watershed lines form where different regions would meet. Lower
 * intensity values represent valleys where water accumulates, and higher
 * values represent hills/ridges.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param grayscale_data Pointer to the input grayscale image data.
 * @param markers Array of Point2D markers with x,y coordinates.
 * @param num_markers Number of markers in the array.
 * @return Pointer to the segmented image data.
 */
uint8_t *fcv_watershed_segmentation(
  uint32_t width,
  uint32_t height,
  uint8_t const *const grayscale_data,
  Point2D *markers,
  uint32_t num_markers,
  bool create_boundaries
) {
  if (!grayscale_data || !markers || num_markers == 0) {
    return NULL;
  }

  // Validate all marker positions
  for (uint32_t m = 0; m < num_markers; m++) {
    int32_t marker_x = (int32_t)markers[m].x;
    int32_t marker_y = (int32_t)markers[m].y;

    // Check bounds - if any marker is invalid, fail the entire operation
    if (marker_x < 0 || marker_x >= (int32_t)width || marker_y < 0 ||
        marker_y >= (int32_t)height) {
      return NULL;
    }
  }

  uint32_t img_length_px = width * height;

  // Create output data
  uint8_t *output_data = malloc(img_length_px * 4);
  if (!output_data) {
    return NULL;
  }

  // Initialize labels array (-1 = unvisited, 0+ = region labels)
  int32_t *labels = malloc(img_length_px * sizeof(int32_t));
  if (!labels) {
    free(output_data);
    return NULL;
  }

  // Initialize all pixels as unvisited
  for (uint32_t i = 0; i < img_length_px; i++) {
    labels[i] = -1;
  }

  // Create queue for flood fill
  Queue *queue = create_queue(img_length_px);
  if (!queue) {
    free(output_data);
    free(labels);
    return NULL;
  }

  // Initialize markers and add to queue
  for (uint32_t m = 0; m < num_markers; m++) {
    int32_t marker_x = (int32_t)markers[m].x;
    int32_t marker_y = (int32_t)markers[m].y;
    int32_t idx = marker_y * width + marker_x;
    int32_t label = (int32_t)m; // Label starts from 0
    labels[idx] = label;
    enqueue(queue, marker_x, marker_y, label);
  }

  // Level-wise watershed algorithm - process pixels in elevation order
  int32_t dx[] = {-1, 1, 0, 0};
  int32_t dy[] = {0, 0, -1, 1};

  // Process each elevation level from 0 to 255
  for (int32_t current_level = 0; current_level <= 255; current_level++) {
    // Single-step expansion: each region grows by exactly one pixel layer per
    // iteration
    bool changed = true;
    while (changed) {
      changed = false;

      // Find all pixels at current level that should be added in this step
      for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
          int32_t idx = y * width + x;

          // Skip if already labeled
          if (labels[idx] != -1) {
            continue;
          }

          // Allow water to flow downhill:
          // Process pixels at or below current level.
          // This enables proper watershed behavior
          // where water flows from high to low elevation.
          bool can_flood = (grayscale_data[idx] <= current_level);

          if (!can_flood) {
            continue;
          }

          // Check if this pixel neighbors any labeled pixel
          int32_t neighbor_label = -1;
          bool multiple_labels = false;

          for (int32_t d = 0; d < 4; d++) {
            int32_t nx = x + dx[d];
            int32_t ny = y + dy[d];

            if (nx >= 0 && nx < (int32_t)width && ny >= 0 &&
                ny < (int32_t)height) {
              int32_t neighbor_idx = ny * width + nx;
              if (labels[neighbor_idx] != -1) {
                if (neighbor_label == -1) {
                  neighbor_label = labels[neighbor_idx];
                }
                else if (neighbor_label != labels[neighbor_idx]) {
                  multiple_labels = true;
                  break;
                }
              }
            }
          }

          // If found exactly one labeled neighbor, mark for expansion
          // If multiple labels and boundaries disabled, use first found label
          if (neighbor_label != -1 &&
              (!multiple_labels || !create_boundaries)) {
            enqueue(queue, x, y, neighbor_label);
            changed = true;
          }
        }
      }

      // Apply all expansions from this step simultaneously
      while (!is_queue_empty(queue)) {
        QueueItem item = dequeue(queue);
        int32_t idx = item.y * width + item.x;
        labels[idx] = item.label;
      }
    }
  }

  destroy_queue(queue);

  // Generate distinct colors for each region
  uint8_t colors[10][3] = {
    {255, 0, 0},     // red
    {0, 255, 0},     // green
    {0, 0, 255},     // blue
    {255, 255, 0},   // yellow
    {255, 0, 255},   // magenta
    {0, 255, 255},   // cyan
    {255, 128, 0},   // orange
    {128, 0, 255},   // purple
    {255, 192, 203}, // pink
    {128, 128, 128}  // gray
  };

  // Create output image
  for (uint32_t i = 0; i < img_length_px; i++) {
    uint32_t rgba_idx = i * 4;
    int32_t label = labels[i];

    if (label == -1) {
      // Unassigned pixels -> background (black)
      output_data[rgba_idx] = 0;     // R
      output_data[rgba_idx + 1] = 0; // G
      output_data[rgba_idx + 2] = 0; // B
    }
    else if (label >= 0 && label < 10) {
      // Assigned to a region
      output_data[rgba_idx] = colors[label][0];     // R
      output_data[rgba_idx + 1] = colors[label][1]; // G
      output_data[rgba_idx + 2] = colors[label][2]; // B
    }
    else {
      // Fallback for more than 10 regions
      output_data[rgba_idx] = 128;     // R
      output_data[rgba_idx + 1] = 128; // G
      output_data[rgba_idx + 2] = 128; // B
    }
    output_data[rgba_idx + 3] = 255; // A
  }

  free(labels);

  return output_data;
}
