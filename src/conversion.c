#include <assert.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "conversion.h"
#include "draw.h"
#include "parse_hex_color.h"
#include "perspectivetransform.h"
#include "rgba_to_grayscale.h"
#include "single_to_multichannel.h"
#include "sobel_edge_detection.h"
#else
#include "flatcv.h"
#endif

/**
 * Convert raw RGBA row-major top-to-bottom image data
 * to RGBA row-major top-to-bottom grayscale image data.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param data Point32_ter to the pixel data.
 * @return Point32_ter to the grayscale image data.
 */
uint8_t *grayscale(uint32_t width, uint32_t height, uint8_t const *const data) {
  uint32_t img_length_byte = width * height * 4;
  uint8_t *grayscale_data = malloc(img_length_byte);

  if (!grayscale_data) { // Memory allocation failed
    return NULL;
  }

  // Process each pixel row by row
  for (uint32_t i = 0; i < width * height; i++) {
    uint32_t rgba_index = i * 4;

    uint8_t r = data[rgba_index];
    uint8_t g = data[rgba_index + 1];
    uint8_t b = data[rgba_index + 2];

    uint8_t gray = (r * R_WEIGHT + g * G_WEIGHT + b * B_WEIGHT) >> 8;

    grayscale_data[rgba_index] = gray;
    grayscale_data[rgba_index + 1] = gray;
    grayscale_data[rgba_index + 2] = gray;
    grayscale_data[rgba_index + 3] = 255;
  }

  return grayscale_data;
}

/**
 * Convert raw RGBA row-major top-to-bottom image data
 * to RGBA row-major top-to-bottom grayscale image data
 * with a stretched contrast range.
 * Set the 1.5625 % darkest pixels to 0 and the 1.5625 % brightest to 255.
 * Uses this specific value for speed: x * 1.5625 % = x >> 6
 * The rest of the pixel values are linearly scaled to the range [0, 255].

 * @param width Width of the image.
 * @param height Height of the image.
 * @param data Point32_ter to the pixel data.
 * @return Point32_ter to the grayscale image data.
 */
uint8_t *
grayscale_stretch(uint32_t width, uint32_t height, uint8_t const *const data) {
  uint32_t img_length_byte = width * height * 4;
  uint8_t *grayscale_data = malloc(img_length_byte);

  if (!grayscale_data) { // Memory allocation failed
    return NULL;
  }

  uint32_t img_length_px = width * height;
  // Ignore 1.5625 % of the pixels
  uint32_t num_pixels_to_ignore = img_length_px >> 6;

  uint8_t *gray_values = malloc(img_length_px);
  if (!gray_values) { // Memory allocation failed
    free(grayscale_data);
    return NULL;
  }

  // Process each pixel row by row to get grayscale values
  for (uint32_t i = 0; i < img_length_px; i++) {
    uint32_t rgba_index = i * 4;

    uint8_t r = data[rgba_index];
    uint8_t g = data[rgba_index + 1];
    uint8_t b = data[rgba_index + 2];

    gray_values[i] = (r * R_WEIGHT + g * G_WEIGHT + b * B_WEIGHT) >> 8;
  }

  // Use counting sort to find the 1.5625% darkest and brightest pixels
  uint32_t histogram[256] = {0};
  for (uint32_t i = 0; i < img_length_px; i++) {
    histogram[gray_values[i]]++;
  }

  uint32_t cumulative_count = 0;
  uint8_t min_val = 0;
  for (uint32_t i = 0; i < 256; i++) {
    cumulative_count += histogram[i];
    if (cumulative_count > num_pixels_to_ignore) {
      min_val = i;
      break;
    }
  }

  cumulative_count = 0;
  uint8_t max_val = 255;
  for (int32_t i = 255; i >= 0; i--) {
    cumulative_count += histogram[i];
    if (cumulative_count > num_pixels_to_ignore) {
      max_val = i;
      break;
    }
  }

  free(gray_values);

  uint8_t range = max_val - min_val;

  // Process each pixel row by row
  for (uint32_t i = 0; i < img_length_px; i++) {
    uint32_t rgba_index = i * 4;

    uint8_t r = data[rgba_index];
    uint8_t g = data[rgba_index + 1];
    uint8_t b = data[rgba_index + 2];

    uint8_t gray = (r * R_WEIGHT + g * G_WEIGHT + b * B_WEIGHT) >> 8;

    if (gray < min_val) {
      gray = 0;
    }
    else if (gray > max_val) {
      gray = 255;
    }
    else {
      gray = (gray - min_val) * 255 / range;
    }

    grayscale_data[rgba_index] = gray;
    grayscale_data[rgba_index + 1] = gray;
    grayscale_data[rgba_index + 2] = gray;
    grayscale_data[rgba_index + 3] = 255;
  }

  return grayscale_data;
}

/**
 * Apply a global threshold to the image data.
 *
 * @param img_length_px Length of the image data in pixels.
 * @param data Point32_ter to the image data.
 * @param threshold Threshold value.
 *
 */
void apply_global_threshold(
  uint32_t img_length_px,
  uint8_t *data,
  uint8_t threshold
) {
  for (uint32_t i = 0; i < img_length_px; i++) {
    data[i] = data[i] > threshold ? 255 : 0;
  }
}

/**
 * Applies two thresholds to the image data by blackening pixels
 * below the lower threshold and whitening pixels above the upper threshold.
 * Pixels between the two thresholds are scaled to the range [0, 255].
 *
 * @param img_length_px Length of the image data in pixels.
 * @param data Point32_ter to the image data.
 * @param lower_threshold Every pixel below this value will be blackened.
 * @param upper_threshold Every pixel above this value will be whitened.
 *
 */
void apply_double_threshold(
  uint32_t img_length_px,
  uint8_t *data,
  uint8_t lower_threshold,
  uint8_t upper_threshold
) {
  for (uint32_t i = 0; i < img_length_px; i++) {
    if (data[i] < lower_threshold) {
      data[i] = 0;
    }
    else if (data[i] > upper_threshold) {
      data[i] = 255;
    }
    else {
      data[i] =
        (data[i] - lower_threshold) * 255 / (upper_threshold - lower_threshold);
    }
  }
}

/**
 * Apply Otsu's thresholding algorithm to the image data.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param use_double_threshold Whether to use double thresholding.
 * @param data Point32_ter to the pixel data.
 * @return Point32_ter to the monochrome image data.
 */
uint8_t *otsu_threshold_rgba(
  uint32_t width,
  uint32_t height,
  bool use_double_threshold,
  uint8_t const *const data
) {
  uint8_t *grayscale_img = rgba_to_grayscale(width, height, data);
  uint32_t img_length_px = width * height;

  uint32_t histogram[256] = {0};
  for (uint32_t i = 0; i < img_length_px; i++) {
    histogram[grayscale_img[i]]++;
  }

  float histogram_norm[256] = {0};
  for (uint32_t i = 0; i < 256; i++) {
    histogram_norm[i] = (float)histogram[i] / img_length_px;
  }

  float global_mean = 0.0;

  for (uint32_t i = 0; i < 256; i++) {
    global_mean += i * histogram_norm[i];
  }

  float cumulative_sum = 0.0;
  float cumulative_mean = 0.0;
  float max_variance = 0.0;
  int32_t optimal_threshold = 0;

  for (uint32_t i = 0; i < 256; i++) {
    cumulative_sum += histogram_norm[i];
    cumulative_mean += i * histogram_norm[i];

    if (cumulative_sum == 0 || cumulative_sum == 1) {
      continue;
    }

    float mean1 = cumulative_mean / cumulative_sum;
    float mean2 = (global_mean - cumulative_mean) / (1 - cumulative_sum);

    float class_variance =
      cumulative_sum * (1 - cumulative_sum) * (mean1 - mean2) * (mean1 - mean2);

    if (class_variance > max_variance) {
      max_variance = class_variance;
      optimal_threshold = i;
    }
  }

  const int32_t threshold_range_offset = 16;

  if (use_double_threshold) {
    apply_double_threshold(
      img_length_px,
      grayscale_img,
      optimal_threshold - threshold_range_offset,
      optimal_threshold + threshold_range_offset
    );
  }
  else {
    apply_global_threshold(img_length_px, grayscale_img, optimal_threshold);
  }

  uint8_t *monochrome_data =
    single_to_multichannel(width, height, grayscale_img);

  free(grayscale_img);

  return monochrome_data;
}

/**
 * Apply gaussian blur to the image data.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param data Point32_ter to the pixel data.
 * @return Point32_ter to the blurred image data.
 */
uint8_t *apply_gaussian_blur(
  uint32_t width,
  uint32_t height,
  double radius,
  uint8_t const *const data
) {
  uint32_t img_length_px = width * height;
  if (radius == 0) {
    return memcpy(malloc(width * height * 4), data, width * height * 4);
  }

  uint8_t *blurred_data = malloc(img_length_px * 4);

  if (!blurred_data) { // Memory allocation failed
    return NULL;
  }

  uint32_t kernel_size = 2 * radius + 1;
  float *kernel = malloc(kernel_size * sizeof(float));

  if (!kernel) { // Memory allocation failed
    free(blurred_data);
    return NULL;
  }

  float sigma = radius / 3.0;
  float sigma_sq = sigma * sigma;
  float two_sigma_sq = 2 * sigma_sq;
  float sqrt_two_pi_sigma = sqrt(2 * M_PI) * sigma;

  for (uint32_t i = 0; i < kernel_size; i++) {
    int32_t x = i - radius;
    kernel[i] = exp(-(x * x) / two_sigma_sq) / sqrt_two_pi_sigma;
  }

  // Apply the kernel in the horizontal direction
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      float r_sum = 0.0;
      float g_sum = 0.0;
      float b_sum = 0.0;
      float weight_sum = 0.0;

      for (int32_t k = -radius; k <= radius; k++) {
        int32_t x_offset = x + k;
        if (x_offset < 0 || (uint32_t)x_offset >= width) {
          continue;
        }

        uint32_t img_index = y * width + x_offset;
        uint32_t img_rgba_index = img_index * 4;

        float weight = kernel[k + (int32_t)radius];
        weight_sum += weight;

        r_sum += data[img_rgba_index] * weight;
        g_sum += data[img_rgba_index + 1] * weight;
        b_sum += data[img_rgba_index + 2] * weight;
      }

      uint32_t rgba_index = (y * width + x) * 4;
      blurred_data[rgba_index] = r_sum / weight_sum;
      blurred_data[rgba_index + 1] = g_sum / weight_sum;
      blurred_data[rgba_index + 2] = b_sum / weight_sum;
      blurred_data[rgba_index + 3] = 255;
    }
  }

  // Create temporary buffer for vertical pass to avoid reading from buffer
  // being written to
  uint8_t *temp_data = malloc(img_length_px * 4);
  if (!temp_data) {
    free(blurred_data);
    free(kernel);
    return NULL;
  }

  // Copy horizontal blur result to temp buffer
  memcpy(temp_data, blurred_data, img_length_px * 4);

  // Apply the kernel in the vertical direction
  for (uint32_t x = 0; x < width; x++) {
    for (uint32_t y = 0; y < height; y++) {
      float r_sum = 0.0;
      float g_sum = 0.0;
      float b_sum = 0.0;
      float weight_sum = 0.0;

      for (int32_t k = -radius; k <= radius; k++) {
        int32_t y_offset = y + k;
        if (y_offset < 0 || (uint32_t)y_offset >= height) {
          continue;
        }

        uint32_t img_index = y_offset * width + x;
        uint32_t img_rgba_index = img_index * 4;

        float weight = kernel[k + (int32_t)radius];
        weight_sum += weight;

        r_sum += temp_data[img_rgba_index] * weight;
        g_sum += temp_data[img_rgba_index + 1] * weight;
        b_sum += temp_data[img_rgba_index + 2] * weight;
      }

      uint32_t rgba_index = (y * width + x) * 4;
      blurred_data[rgba_index] = r_sum / weight_sum;
      blurred_data[rgba_index + 1] = g_sum / weight_sum;
      blurred_data[rgba_index + 2] = b_sum / weight_sum;
      blurred_data[rgba_index + 3] = 255;
    }
  }

  free(temp_data);

  free(kernel);

  return blurred_data;
}

#include <time.h>

/**
 * Convert image to anti-aliased black and white.
 * 1. Convert the image to grayscale.
 * 2. Subtract blurred image from the original image to get the high
 * frequencies.
 * 3. Apply OTSU's threshold to get the optimal threshold.
 * 4. Apply the threshold + offset to get the anti-aliased image.
 *
 * @param width Width of the image.
 * @param height Height of the image.
 * @param data Point32_ter to the pixel data.
 * @return Point32_ter to the blurred image data.
 */
uint8_t *bw_smart(
  uint32_t width,
  uint32_t height,
  bool use_double_threshold,
  uint8_t const *const data
) {
  uint8_t *grayscale_data = grayscale(width, height, data);

  // Calculate blur radius dependent on image size
  // (Empirical formula after testing)
  double blurRadius = (sqrt((double)width * (double)height)) * 0.1;

  uint8_t *blurred_data =
    apply_gaussian_blur(width, height, blurRadius, grayscale_data);

  uint32_t img_length_px = width * height;
  uint8_t *high_freq_data = malloc(img_length_px * 4);

  if (!high_freq_data) { // Memory allocation failed
    free((void *)grayscale_data);
    free((void *)blurred_data);
    return NULL;
  }

  // Subtract blurred image from the original image to get the high frequencies
  // and invert the high frequencies to get a white background.
  for (uint32_t i = 0; i < img_length_px; i++) {
    uint32_t rgba_idx = i * 4;
    int32_t high_freq_val =
      127 + grayscale_data[rgba_idx] - blurred_data[rgba_idx];

    // Clamp the value to [0, 255] to prevent overflow
    if (high_freq_val < 0) {
      high_freq_val = 0;
    }
    else if (high_freq_val > 255) {
      high_freq_val = 255;
    }

    high_freq_data[rgba_idx] = high_freq_val;     // R
    high_freq_data[rgba_idx + 1] = high_freq_val; // G
    high_freq_data[rgba_idx + 2] = high_freq_val; // B
    high_freq_data[rgba_idx + 3] = 255;           // A
  }

  free((void *)grayscale_data);
  free((void *)blurred_data);

  uint8_t *final_data =
    otsu_threshold_rgba(width, height, use_double_threshold, high_freq_data);

  free(high_freq_data);

  return final_data;
}

/**
 * Resize an image by given resize factors using bilinear int32_terpolation.
 *
 * @param width Width of the input image.
 * @param height Height of the input image.
 * @param resize_x Horizontal resize factor (e.g., 2.0 for 2x, 0.5 for half).
 * @param resize_y Vertical resize factor (e.g., 2.0 for 2x, 0.5 for half).
 * @param out_width Point32_ter to store the output image width.
 * @param out_height Point32_ter to store the output image height.
 * @param data Point32_ter to the input pixel data.
 * @return Point32_ter to the resized image data.
 */
uint8_t *resize(
  uint32_t width,
  uint32_t height,
  double resize_x,
  double resize_y,
  uint32_t *out_width,
  uint32_t *out_height,
  uint8_t const *const data
) {
  if (resize_x <= 0.0 || resize_y <= 0.0) {
    return NULL;
  }

  *out_width = (uint32_t)(width * resize_x);
  *out_height = (uint32_t)(height * resize_y);

  if (*out_width == 0 || *out_height == 0) {
    return NULL;
  }

  uint32_t out_img_length = *out_width * *out_height * 4;
  uint8_t *resized_data = malloc(out_img_length);

  if (!resized_data) {
    return NULL;
  }

  for (uint32_t out_y = 0; out_y < *out_height; out_y++) {
    for (uint32_t out_x = 0; out_x < *out_width; out_x++) {
      if (resize_x < 1.0 || resize_y < 1.0) {
        double src_x = (out_x + 0.5) / resize_x - 0.5;
        double src_y = (out_y + 0.5) / resize_y - 0.5;

        double filter_size_x = 1.0 / resize_x;
        double filter_size_y = 1.0 / resize_y;

        double x_start = src_x - filter_size_x * 0.5;
        double y_start = src_y - filter_size_y * 0.5;
        double x_end = src_x + filter_size_x * 0.5;
        double y_end = src_y + filter_size_y * 0.5;

        int32_t ix_start = (int32_t)floor(x_start);
        int32_t iy_start = (int32_t)floor(y_start);
        int32_t ix_end = (int32_t)ceil(x_end);
        int32_t iy_end = (int32_t)ceil(y_end);

        if (ix_start < 0) {
          ix_start = 0;
        }
        if (iy_start < 0) {
          iy_start = 0;
        }
        if (ix_end > (int32_t)width) {
          ix_end = width;
        }
        if (iy_end > (int32_t)height) {
          iy_end = height;
        }

        double r_sum = 0.0, g_sum = 0.0, b_sum = 0.0;
        double total_weight = 0.0;

        for (int32_t sy = iy_start; sy < iy_end; sy++) {
          for (int32_t sx = ix_start; sx < ix_end; sx++) {
            double left = sx;
            double right = sx + 1;
            double top = sy;
            double bottom = sy + 1;

            double overlap_left = left > x_start ? left : x_start;
            double overlap_right = right < x_end ? right : x_end;
            double overlap_top = top > y_start ? top : y_start;
            double overlap_bottom = bottom < y_end ? bottom : y_end;

            if (overlap_right > overlap_left && overlap_bottom > overlap_top) {
              double weight =
                (overlap_right - overlap_left) * (overlap_bottom - overlap_top);
              total_weight += weight;

              uint32_t src_idx = (sy * width + sx) * 4;
              r_sum += data[src_idx] * weight;
              g_sum += data[src_idx + 1] * weight;
              b_sum += data[src_idx + 2] * weight;
            }
          }
        }

        if (total_weight > 0.0) {
          resized_data[(out_y * *out_width + out_x) * 4] =
            (uint8_t)(r_sum / total_weight + 0.5);
          resized_data[(out_y * *out_width + out_x) * 4 + 1] =
            (uint8_t)(g_sum / total_weight + 0.5);
          resized_data[(out_y * *out_width + out_x) * 4 + 2] =
            (uint8_t)(b_sum / total_weight + 0.5);
        }
        else {
          resized_data[(out_y * *out_width + out_x) * 4] = 0;
          resized_data[(out_y * *out_width + out_x) * 4 + 1] = 0;
          resized_data[(out_y * *out_width + out_x) * 4 + 2] = 0;
        }
        resized_data[(out_y * *out_width + out_x) * 4 + 3] = 255;
      }
      else {
        double src_x = (out_x + 0.5) / resize_x - 0.5;
        double src_y = (out_y + 0.5) / resize_y - 0.5;

        int32_t x0 = (int32_t)floor(src_x);
        int32_t y0 = (int32_t)floor(src_y);
        int32_t x1 = x0 + 1;
        int32_t y1 = y0 + 1;

        if (x0 < 0) {
          x0 = 0;
        }
        if (y0 < 0) {
          y0 = 0;
        }
        if (x1 >= (int32_t)width) {
          x1 = width - 1;
        }
        if (y1 >= (int32_t)height) {
          y1 = height - 1;
        }

        double dx = src_x - x0;
        double dy = src_y - y0;

        if (dx < 0) {
          dx = 0;
        }
        if (dy < 0) {
          dy = 0;
        }
        if (dx > 1) {
          dx = 1;
        }
        if (dy > 1) {
          dy = 1;
        }

        for (int32_t c = 0; c < 3; c++) {
          uint8_t p00 = data[(y0 * width + x0) * 4 + c];
          uint8_t p01 = data[(y0 * width + x1) * 4 + c];
          uint8_t p10 = data[(y1 * width + x0) * 4 + c];
          uint8_t p11 = data[(y1 * width + x1) * 4 + c];

          double int32_terpolated = p00 * (1 - dx) * (1 - dy) +
                                    p01 * dx * (1 - dy) + p10 * (1 - dx) * dy +
                                    p11 * dx * dy;

          resized_data[(out_y * *out_width + out_x) * 4 + c] =
            (uint8_t)(int32_terpolated + 0.5);
        }

        resized_data[(out_y * *out_width + out_x) * 4 + 3] = 255;
      }
    }
  }

  return resized_data;
}
