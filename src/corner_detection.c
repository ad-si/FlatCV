#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "binary_closing_disk.h"
#include "conversion.h"
#include "convert_to_binary.h"
#include "corner_peaks.h"
#include "draw.h"
#include "foerstner_corner.h"
#include "parse_hex_color.h"
#include "perspectivetransform.h"
#include "sobel_edge_detection.h"
#include "sort_corners.h"
#include "watershed_segmentation.h"

#ifdef DEBUG_LOGGING
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif
#else
#include "flatcv.h"
#endif

/** Count number of distinct colors in RGBA image
 * 1. Convert to grayscale
 * 2. Scale to 256x256 (save scale ratio for x and y)
 * 3. Blur image
 * 4. Create elevation map with Sobel
 * 5. Flatten elevation map at center seed to avoid being trapped in a local
 * minimum
 * 6. Add black border to flood from all directions at once
 * 7. `watershed(image=elevation_map, markers=markers)`
 *     Set center as marker for foreground basin and border for background basin
 * 8. Check region count equals 2
 * 9. Smooth result with binary closing
 * 10. Use Foerstner corner detector
 *     (Harris detector corners are shifted inwards)
 * 11. Sort corners
 * 12. TODO: Select 4 corners with the largest angle while maintaining their
 * order
 * 13. Normalize corners based on scale ratio
 */
int32_t count_colors(const uint8_t *image, int32_t width, int32_t height) {
  assert(image != NULL);
  assert(width > 0);
  assert(height > 0);

  int32_t color_count = 0;
  int32_t *color_map = (int32_t *)calloc(256 * 256 * 256, sizeof(int32_t));
  if (!color_map) {
    fprintf(stderr, "Error: Failed to allocate memory for color map\n");
    return -1;
  }

  for (int32_t i = 0; i < width * height * 4; i += 4) {
    uint8_t r = image[i];
    uint8_t g = image[i + 1];
    uint8_t b = image[i + 2];
    uint8_t a = image[i + 3];

    // Skip fully transparent pixels
    if (a == 0) {
      continue;
    }

    // Create a unique color key
    int32_t color_key = (r << 16) | (g << 8) | b;

    // Increment the count for this color
    if (color_map[color_key] == 0) {
      color_count++;
    }
    color_map[color_key]++;
  }

  free(color_map);
  return color_count;
}

typedef struct {
  int32_t width;
  int32_t height;
  int32_t channels;
  const uint8_t *data;
} Image;

#ifdef DEBUG_LOGGING
void write_debug_img(Image img, const char *filename) {
  int32_t result = stbi_write_png(
    filename,
    img.width,
    img.height,
    img.channels,
    img.data,
    img.width * img.channels
  );
  if (result == 0) {
    fprintf(stderr, "Error: Failed to write debug image %s\n", filename);
  }
}
#endif

/**
 * Detect corners in the input image.
 */
Corners
fcv_detect_corners(const uint8_t *image, int32_t width, int32_t height) {
  assert(image != NULL);
  assert(width > 0);
  assert(height > 0);

  Corners default_corners = {
    .tl_x = 0,
    .tl_y = 0,
    .tr_x = width - 1,
    .tr_y = 0,
    .br_x = width - 1,
    .br_y = height - 1,
    .bl_x = 0,
    .bl_y = height - 1
  };

  // 1. Convert to grayscale
  uint8_t const *grayscale_image = fcv_grayscale(width, height, image);
  if (!grayscale_image) {
    fprintf(stderr, "Error: Failed to convert image to grayscale\n");
    return default_corners;
  }

  // 2. Resize image to 256x256
  uint32_t out_width = 256;
  uint32_t out_height = 256;
  uint8_t const *resized_image = fcv_resize(
    width,
    height,
    (double)out_width / width,
    (double)out_height / height,
    &out_width,
    &out_height,
    grayscale_image
  );
  free((void *)grayscale_image);
  if (!resized_image) {
    fprintf(stderr, "Error: Failed to resize image\n");
    exit(EXIT_FAILURE);
  }

#ifdef DEBUG_LOGGING
  Image out_img = {
    .width = out_width,
    .height = out_height,
    .channels = 4,
    .data = resized_image
  };
  write_debug_img(out_img, "temp_1_resized_image.png");
#endif

  // 3. Apply Gaussian blur
  uint8_t const *blurred_image =
    fcv_apply_gaussian_blur(out_width, out_height, 3.0, resized_image);
#ifndef DEBUG_LOGGING
  free((void *)resized_image);
#endif
  if (!blurred_image) {
    fprintf(stderr, "Error: Failed to apply Gaussian blur\n");
    exit(EXIT_FAILURE);
  }

  // 4. Create elevation map with Sobel edge detection
  uint8_t *elevation_map = (uint8_t *)
    fcv_sobel_edge_detection(out_width, out_height, 4, blurred_image);
  free((void *)blurred_image);
  if (!elevation_map) {
    fprintf(stderr, "Error: Failed to create elevation map with Sobel\n");
    exit(EXIT_FAILURE);
  }

  // 5. Flatten elevation map at center to not get trapped in a local minimum
  fcv_draw_disk(
    out_width,
    out_height,
    1, // Single channel grayscale
    "000000",
    24,
    out_width / 2.0,
    out_height / 2.0,
    elevation_map
  );

  // 6. Add black border to flood from all directions at once
  uint32_t bordered_width, bordered_height;
  uint8_t *bordered_elevation_map = fcv_add_border(
    out_width,
    out_height,
    1,        // Single channel grayscale
    "000000", // Black border
    1,        // Border width of 1 pixel
    elevation_map,
    &bordered_width,
    &bordered_height
  );

  free(elevation_map);
  if (!bordered_elevation_map) {
    fprintf(stderr, "Error: Failed to add black border to elevation map\n");
    exit(EXIT_FAILURE);
  }

#ifdef DEBUG_LOGGING
  out_img.data = bordered_elevation_map;
  out_img.channels = 1; // Grayscale
  out_img.width = bordered_width;
  out_img.height = bordered_height;
  write_debug_img(out_img, "temp_2_elevation_map.png");
#endif

  // 7. Perform watershed segmentation
  int32_t num_markers = 2;
  Point2D *markers = malloc(num_markers * sizeof(Point2D));
  if (!markers) {
    fprintf(stderr, "Error: Failed to allocate memory for markers\n");
    free((void *)bordered_elevation_map);
    exit(EXIT_FAILURE);
  }
  // Set center as foreground marker and upper left corner as background marker
  // Adjust for the added border (border width = 1)
  markers[0] = (Point2D){.x = bordered_width / 2.0, .y = bordered_height / 2.0};
  markers[1] = (Point2D){.x = 0, .y = 0};

  uint8_t *segmented_image_wide = fcv_watershed_segmentation(
    bordered_width,
    bordered_height,
    bordered_elevation_map,
    markers,
    num_markers,
    false // No boundaries
  );
  free((void *)bordered_elevation_map);
  free((void *)markers);

  // Remove 1 pixel border from segmented image
  uint8_t *segmented_image = malloc(out_width * out_height * 4);
  if (!segmented_image) {
    fprintf(stderr, "Error: Failed to allocate memory for segmented image\n");
    free((void *)segmented_image_wide);
    exit(EXIT_FAILURE);
  }
  for (uint32_t y = 1; y < bordered_height - 1; y++) {
    memcpy(
      &segmented_image[(y - 1) * out_width * 4],
      &segmented_image_wide[(y * bordered_width + 1) * 4],
      out_width * 4
    );
  }

#ifdef DEBUG_LOGGING
  out_img.width = out_width;
  out_img.height = out_height;
  out_img.channels = 4;
  out_img.data = segmented_image;
  write_debug_img(out_img, "temp_3_watershed.png");
#endif

  // 8. Check if the image has exactly 2 regions (foreground and background)
  int32_t region_count = count_colors(segmented_image, out_width, out_height);
  if (region_count != 2) {
    fprintf(stderr, "Error: Expected 2 regions, found %d\n", region_count);
    free((void *)segmented_image);
    exit(EXIT_FAILURE);
  }

  // Convert red pixel (marker 1 - foreground) to white
  // and green pixel (marker 2 - background) to black
  uint8_t *segmented_binary = fcv_convert_to_binary(
    segmented_image,
    out_width,
    out_height,
    "FF0000", // red -> white
    "00FF00"  // green -> black
  );
  free((void *)segmented_image);

#ifdef DEBUG_LOGGING
  out_img.data = segmented_binary;
  out_img.channels = 1;
  write_debug_img(out_img, "temp_4_watershed_binary.png");
#endif

  // 9. Smooth the result
  uint8_t *segmented_closed = fcv_binary_closing_disk(
    segmented_binary,
    out_width,
    out_height,
    12 // Closing radius
  );
  free((void *)segmented_binary);
  if (!segmented_closed) {
    fprintf(stderr, "Error: Failed to perform binary closing\n");
    exit(EXIT_FAILURE);
  }

#ifdef DEBUG_LOGGING
  out_img.data = segmented_closed;
  write_debug_img(out_img, "temp_5_segmented_closed.png");
#endif

  // 10. Find corners in the closed image
  uint8_t *corner_response = fcv_foerstner_corner(
    out_width,
    out_height,
    segmented_closed,
    1.5 // Sigma for Gaussian smoothing
  );
  free((void *)segmented_closed);
  if (!corner_response) {
    fprintf(stderr, "Error: Failed to compute corner response\n");
    exit(EXIT_FAILURE);
  }

  // Extract `w` channel (error ellipse size) for visualization
  uint8_t *w_channel = malloc(out_width * out_height);
  if (!w_channel) {
    fprintf(stderr, "Error: Failed to allocate memory for w channel\n");
    free((void *)corner_response);
    exit(EXIT_FAILURE);
  }

  for (uint32_t i = 0; i < out_width * out_height; i++) {
    w_channel[i] = corner_response[i * 2]; // Extract `w` channel
  }

#ifdef DEBUG_LOGGING
  out_img.data = w_channel;
  write_debug_img(out_img, "temp_6_corner_response.png");
#endif
  free(w_channel);

  // 10. Find corner peaks using thresholds
  CornerPeaks *peaks = fcv_corner_peaks(
    out_width,
    out_height,
    corner_response,
    16,  // Minimum distance between peaks
    0.5, // accuracy_thresh
    0.3  // roundness_thresh
  );
  free((void *)corner_response);
  if (!peaks) {
    fprintf(stderr, "Error: Failed to find corner peaks\n");
    exit(EXIT_FAILURE);
  }

  Point2D *sorted_result = malloc(peaks->count * sizeof(Point2D));
  Corners sorted_corners = sort_corners(
    width,
    height,
    out_width,
    out_height,
    peaks->points,
    peaks->count,
    sorted_result
  );
  free(sorted_result);

#ifdef DEBUG_LOGGING
  // Print peaks for debugging
  printf("Peaks:\n");
  printf("%.0f,%.0f\n", sorted_corners.tl_x, sorted_corners.tl_y);
  printf("%.0f,%.0f\n", sorted_corners.tr_x, sorted_corners.tr_y);
  printf("%.0f,%.0f\n", sorted_corners.br_x, sorted_corners.br_y);
  printf("%.0f,%.0f\n", sorted_corners.bl_x, sorted_corners.bl_y);
  // Draw corner peaks on the grayscale and resized image
  for (uint32_t i = 0; i < peaks->count; i++) {
    // Print peak coordinates for debugging
    printf(
      "Peak %u: (%.1f, %.1f)\n",
      i,
      peaks->points[i].x,
      peaks->points[i].y
    );
    fcv_draw_circle(
      out_width,
      out_height,
      4,        // GRAY as RGBA (each value is the same)
      "FF0000", // Red color for corners
      2,        // Radius
      peaks->points[i].x,
      peaks->points[i].y,
      (uint8_t *)resized_image
    );
  }

  // Draw sorted corners with different colors to distinguish from peaks
  // Scale the sorted corners back to the resized image dimensions for drawing
  double scale_x = (double)out_width / width;
  double scale_y = (double)out_height / height;

  // Draw top-left corner in blue
  fcv_draw_circle(
    out_width,
    out_height,
    4,
    "0000FF", // Blue color for top-left
    3,        // Slightly larger radius to distinguish from peaks
    sorted_corners.tl_x * scale_x,
    sorted_corners.tl_y * scale_y,
    (uint8_t *)resized_image
  );

  // Draw top-right corner in green
  fcv_draw_circle(
    out_width,
    out_height,
    4,
    "00FF00", // Green color for top-right
    3,
    sorted_corners.tr_x * scale_x,
    sorted_corners.tr_y * scale_y,
    (uint8_t *)resized_image
  );

  // Draw bottom-right corner in yellow
  fcv_draw_circle(
    out_width,
    out_height,
    4,
    "FFFF00", // Yellow color for bottom-right
    3,
    sorted_corners.br_x * scale_x,
    sorted_corners.br_y * scale_y,
    (uint8_t *)resized_image
  );

  // Draw bottom-left corner in magenta
  fcv_draw_circle(
    out_width,
    out_height,
    4,
    "FF00FF", // Magenta color for bottom-left
    3,
    sorted_corners.bl_x * scale_x,
    sorted_corners.bl_y * scale_y,
    (uint8_t *)resized_image
  );

  Image corners_img = {
    .width = out_width,
    .height = out_height,
    .channels = 4,
    .data = resized_image
  };
  write_debug_img(corners_img, "temp_7_corners.png");
  free((void *)resized_image);
#endif

  free(peaks);
  return sorted_corners;
}
