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
#include "watershed_segmentation.h"
#else
#include "flatcv.h"
#endif

// #define DEBUG_LOGGING

/** Count number of distinct colors in RGBA image
 * 1. Convert to grayscale
 * 2. Scale to 256x256 (save scale ratio for x and y)
 * 3. Blur image
 * 4. Create elevation map with Sobel
 * 5. Flatten elevation map at center seed to avoid being trapped in a local
 * minimum
 * 6. `watershed(image=elevation_map, markers=markers)`
 *     Set center as marker for foreground basin and border for background basin
 * 7. Check region count equals 2
 * 8. Smooth result
 *     1. TODO: Add border to avoid connection with image boundaries
 *     1. Perform binary closing
 *     1. TODO: Remove border
 * 9. Use Foerstner corner detector (Harris detector corners are shifted
 * inwards)
 * 10. TODO: Sort corners
 * 11. TODO: Select 4 corners with the largest angle while maintaining their
 * order
 * 12. Normalize corners based on scale ratio
 */
int count_colors(const unsigned char *image, int width, int height) {
  assert(image != NULL);
  assert(width > 0);
  assert(height > 0);

  int color_count = 0;
  int *color_map = (int *)calloc(256 * 256 * 256, sizeof(int));
  if (!color_map) {
    fprintf(stderr, "Error: Failed to allocate memory for color map\n");
    return -1;
  }

  for (int i = 0; i < width * height * 4; i += 4) {
    unsigned char r = image[i];
    unsigned char g = image[i + 1];
    unsigned char b = image[i + 2];
    unsigned char a = image[i + 3];

    // Skip fully transparent pixels
    if (a == 0) {
      continue;
    }

    // Create a unique color key
    int color_key = (r << 16) | (g << 8) | b;

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
  int width;
  int height;
  int channels;
  const unsigned char *data;
} Image;

#ifdef DEBUG_LOGGING
void write_debug_img(Image img, const char *filename) {
  int result = stbi_write_png(
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
Corners detect_corners(const unsigned char *image, int width, int height) {
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
  unsigned char const *grayscale_image = grayscale(width, height, image);
  if (!grayscale_image) {
    fprintf(stderr, "Error: Failed to convert image to grayscale\n");
    return default_corners;
  }

  // 2. Resize image to 256x256
  unsigned int out_width = 256;
  unsigned int out_height = 256;
  unsigned char const *resized_image = resize(
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
  write_debug_img(out_img, "temp_2_resized_image.png");
#endif

  // 3. Apply Gaussian blur
  unsigned char const *blurred_image =
    apply_gaussian_blur(out_width, out_height, 3.0, resized_image);
  // Don't free resized_image here, as it is used for debugging later
  if (!blurred_image) {
    fprintf(stderr, "Error: Failed to apply Gaussian blur\n");
    exit(EXIT_FAILURE);
  }

  // 5. Create elevation map with Sobel edge detection
  unsigned char *elevation_map = (unsigned char *)
    sobel_edge_detection(out_width, out_height, 4, blurred_image);
  free((void *)blurred_image);
  if (!elevation_map) {
    fprintf(stderr, "Error: Failed to create elevation map with Sobel\n");
    exit(EXIT_FAILURE);
  }

  // 6. Flatten elevation map at center to not get trapped in a local minimum
  draw_disk(
    out_width,
    out_height,
    1, // Single channel grayscale
    "000000",
    24,
    out_width / 2.0,
    out_height / 2.0,
    elevation_map
  );

#ifdef DEBUG_LOGGING
  out_img.data = elevation_map;
  out_img.channels = 1; // Grayscale
  write_debug_img(out_img, "temp_6_elevation_map.png");
#endif

  // 7. Perform watershed segmentation
  int num_markers = 2;
  Point2D *markers = malloc(num_markers * sizeof(Point2D));
  if (!markers) {
    fprintf(stderr, "Error: Failed to allocate memory for markers\n");
    free((void *)elevation_map);
    exit(EXIT_FAILURE);
  }
  // Set center as foreground marker and upper left corner as background marker
  markers[0] = (Point2D){.x = out_width / 2.0, .y = out_height / 2.0};
  markers[1] = (Point2D){.x = 0, .y = 0};

  unsigned char const *const segmented_image = watershed_segmentation(
    out_width,
    out_height,
    elevation_map,
    markers,
    num_markers,
    false // No boundaries
  );
  free((void *)elevation_map);
  free((void *)markers);

#ifdef DEBUG_LOGGING
  out_img.data = segmented_image;
  out_img.channels = 4;
  write_debug_img(out_img, "temp_7_watershed.png");
#endif

  // Check if the image has exactly 2 regions (foreground and background)
  int region_count = count_colors(segmented_image, out_width, out_height);
  if (region_count != 2) {
    fprintf(stderr, "Error: Expected 2 regions, found %d\n", region_count);
    free((void *)segmented_image);
    exit(EXIT_FAILURE);
  }

  // Convert red pixel (marker 1 - foreground) to white
  // and green pixel (marker 2 - background) to black
  unsigned char const *const segmented_binary = convert_to_binary(
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
  write_debug_img(out_img, "temp_7_watershed_binary.png");
#endif

  // 8. Smooth the result
  unsigned char const *const segmented_closed = binary_closing_disk(
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
  write_debug_img(out_img, "temp_8_segmented_closed.png");
#endif

  // 9. Find corners in the closed image
  unsigned char const *const corner_response = foerstner_corner(
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
  unsigned char *w_channel = malloc(out_width * out_height);
  if (!w_channel) {
    fprintf(stderr, "Error: Failed to allocate memory for w channel\n");
    free((void *)corner_response);
    exit(EXIT_FAILURE);
  }

  for (unsigned int i = 0; i < out_width * out_height; i++) {
    w_channel[i] = corner_response[i * 2]; // Extract `w` channel
  }

#ifdef DEBUG_LOGGING
  out_img.data = w_channel;
  write_debug_img(out_img, "temp_9_corner_response.png");
#endif
  free(w_channel);

  // 10. Find corner peaks using thresholds
  CornerPeaks *peaks = corner_peaks(
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

#ifdef DEBUG_LOGGING
  // Draw corner peaks on the resized image
  for (int i = 0; i < peaks->count; i++) {
    draw_circle(
      out_width,
      out_height,
      4,        // RGBA
      "FF0000", // Red color for corners
      2,        // Radius
      peaks->points[i].x,
      peaks->points[i].y,
      (unsigned char *)resized_image
    );
  }

  out_img.data = resized_image;
  out_img.channels = 4;
  write_debug_img(out_img, "temp_10_corners.png");
#endif
  free((void *)resized_image);

  Corners corners = {
    .tl_x = peaks->points[0].x,
    .tl_y = peaks->points[0].y,
    .tr_x = peaks->points[1].x,
    .tr_y = peaks->points[1].y,
    .br_x = peaks->points[2].x,
    .br_y = peaks->points[2].y,
    .bl_x = peaks->points[3].x,
    .bl_y = peaks->points[3].y
  };

  return corners;
}
