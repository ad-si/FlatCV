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

typedef struct {
  int32_t x, y;
} FloodFillQueueItem;

typedef struct {
  FloodFillQueueItem *items;
  int32_t front;
  int32_t rear;
  int32_t size;
  int32_t capacity;
} FloodFillQueue;

static FloodFillQueue *create_floodfill_queue(size_t capacity) {
  if (capacity == 0 || capacity > SIZE_MAX / sizeof(FloodFillQueueItem) ||
      capacity > INT32_MAX) {
    return NULL;
  }

  FloodFillQueue *q = malloc(sizeof(FloodFillQueue));
  if (!q) {
    return NULL;
  }

  q->items = malloc(capacity * sizeof(FloodFillQueueItem));
  if (!q->items) {
    free(q);
    return NULL;
  }

  q->front = 0;
  q->rear = -1;
  q->size = 0;
  q->capacity = (int32_t)capacity;
  return q;
}

static void destroy_floodfill_queue(FloodFillQueue *q) {
  if (q) {
    free(q->items);
    free(q);
  }
}

static int32_t is_floodfill_queue_empty(FloodFillQueue *q) {
  return q->size == 0;
}

static int32_t enqueue_floodfill(FloodFillQueue *q, int32_t x, int32_t y) {
  if (q->size >= q->capacity) {
    return 0;
  }

  q->rear = (q->rear + 1) % q->capacity;
  q->items[q->rear].x = x;
  q->items[q->rear].y = y;
  q->size++;
  return 1;
}

static FloodFillQueueItem dequeue_floodfill(FloodFillQueue *q) {
  FloodFillQueueItem item = {-1, -1};
  if (q->size == 0) {
    return item;
  }

  item = q->items[q->front];
  q->front = (q->front + 1) % q->capacity;
  q->size--;
  return item;
}

/**
 * Flood fill segmentation
 * using a seeded breadth-first traversal with threshold-based inclusion
 *
 * Expands from the marker point across 4-connected neighbors whose grayscale
 * values stay within the given threshold of the seed value.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param grayscale_data Pointer to the input grayscale image data.
 * @param marker Seed from which to start flood fill.
 * @param threshold Maximum absolute grayscale difference from the seed value.
 * @param new_value Replacement value written to filled pixels.
 * @return Pointer to the filled image data.
 */
uint8_t *fcv_flood_fill(
  uint32_t width,
  uint32_t height,
  uint8_t const *const grayscale_data,
  Point2D const *marker,
  uint8_t threshold,
  uint8_t new_value
) {
  if (!grayscale_data || !marker || width == 0 || height == 0) {
    return NULL;
  }

  if (!isfinite(marker->x) || !isfinite(marker->y)) {
    return NULL;
  }
  int32_t marker_x = (int32_t)marker->x;
  int32_t marker_y = (int32_t)marker->y;
  if (marker_x < 0 || marker_x >= (int32_t)width || marker_y < 0 ||
      marker_y >= (int32_t)height) {
    return NULL;
  }

  if (width > SIZE_MAX / height) {
    return NULL;
  }
  size_t img_length_px = (size_t)width * height;

  if (img_length_px > SIZE_MAX / sizeof(bool)) {
    return NULL;
  }

  uint8_t *output_data = malloc(img_length_px);
  if (!output_data) {
    return NULL;
  }
  memcpy(output_data, grayscale_data, img_length_px);

  bool *visited = calloc(img_length_px, sizeof(bool));
  if (!visited) {
    free(output_data);
    return NULL;
  }

  FloodFillQueue *queue = create_floodfill_queue(img_length_px);
  if (!queue) {
    free(output_data);
    free(visited);
    return NULL;
  }

  size_t start_idx = (size_t)marker_y * width + (uint32_t)marker_x;
  uint8_t target_value = grayscale_data[start_idx];

  if (!enqueue_floodfill(queue, marker_x, marker_y)) {
    destroy_floodfill_queue(queue);
    free(visited);
    free(output_data);
    return NULL;
  }
  visited[start_idx] = true;
  // 4-connected flood fill; diagonals are intentionally excluded.
  int32_t dx[] = {-1, 1, 0, 0};
  int32_t dy[] = {0, 0, -1, 1};

  while (!is_floodfill_queue_empty(queue)) {
    FloodFillQueueItem item = dequeue_floodfill(queue);
    size_t idx = (size_t)item.y * width + (uint32_t)item.x;
    output_data[idx] = new_value;

    for (int32_t d = 0; d < 4; d++) {
      int32_t nx = item.x + dx[d];
      int32_t ny = item.y + dy[d];

      if (nx < 0 || nx >= (int32_t)width || ny < 0 || ny >= (int32_t)height) {
        continue;
      }

      size_t neighbor_idx = (size_t)ny * width + (uint32_t)nx;
      int32_t diff = (int32_t)grayscale_data[neighbor_idx] - target_value;
      if (diff < 0) {
        diff = -diff;
      }

      if (visited[neighbor_idx] || diff > threshold) {
        continue;
      }
      visited[neighbor_idx] = true;
      if (!enqueue_floodfill(queue, nx, ny)) {
        destroy_floodfill_queue(queue);
        free(visited);
        free(output_data);
        return NULL;
      }
    }
  }
  destroy_floodfill_queue(queue);
  free(visited);
  return output_data;
}
