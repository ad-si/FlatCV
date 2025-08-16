#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef FLATCV_AMALGAMATION
#include "conversion.h"
#include "perspectivetransform.h"
#else
#include "flatcv.h"
#endif

/** Implementation of the Foerstner corner measure response image.
 * Expected input is a grayscale image.
 */
unsigned char const *const foerstner_corner(
  unsigned int width,
  unsigned int height,
  unsigned char const *const gray_data,
  double sigma
) {
  if (!gray_data || width == 0 || height == 0) {
    return NULL;
  }

  // Allocate memory for gradient images
  double *grad_x = calloc(width * height, sizeof(double));
  double *grad_y = calloc(width * height, sizeof(double));

  if (!grad_x || !grad_y) {
    free(grad_x);
    free(grad_y);
    return NULL;
  }

  // Compute image gradients using Sobel-like operators
  for (unsigned int y = 1; y < height - 1; y++) {
    for (unsigned int x = 1; x < width - 1; x++) {
      unsigned int idx = y * width + x;

      // Sobel X kernel: [-1 0 1; -2 0 2; -1 0 1]
      double gx = 0.0;
      gx += -1.0 * gray_data[(y - 1) * width + (x - 1)];
      gx += 1.0 * gray_data[(y - 1) * width + (x + 1)];
      gx += -2.0 * gray_data[y * width + (x - 1)];
      gx += 2.0 * gray_data[y * width + (x + 1)];
      gx += -1.0 * gray_data[(y + 1) * width + (x - 1)];
      gx += 1.0 * gray_data[(y + 1) * width + (x + 1)];

      // Sobel Y kernel: [-1 -2 -1; 0 0 0; 1 2 1]
      double gy = 0.0;
      gy += -1.0 * gray_data[(y - 1) * width + (x - 1)];
      gy += -2.0 * gray_data[(y - 1) * width + x];
      gy += -1.0 * gray_data[(y - 1) * width + (x + 1)];
      gy += 1.0 * gray_data[(y + 1) * width + (x - 1)];
      gy += 2.0 * gray_data[(y + 1) * width + x];
      gy += 1.0 * gray_data[(y + 1) * width + (x + 1)];

      grad_x[idx] = gx / 8.0; // Normalize
      grad_y[idx] = gy / 8.0;
    }
  }

  // Compute gradient products for structure tensor
  double *Axx = malloc(width * height * sizeof(double));
  double *Axy = malloc(width * height * sizeof(double));
  double *Ayy = malloc(width * height * sizeof(double));

  if (!Axx || !Axy || !Ayy) {
    free(grad_x);
    free(grad_y);
    free(Axx);
    free(Axy);
    free(Ayy);
    return NULL;
  }

  for (unsigned int i = 0; i < width * height; i++) {
    Axx[i] = grad_x[i] * grad_x[i];
    Axy[i] = grad_x[i] * grad_y[i];
    Ayy[i] = grad_y[i] * grad_y[i];
  }

  // Apply Gaussian smoothing to structure tensor elements
  // For simplicity, use a basic box filter approximation when sigma is small
  int kernel_size = (int)(3 * sigma) | 1; // Ensure odd size
  if (kernel_size < 3) {
    kernel_size = 3;
  }
  int half_kernel = kernel_size / 2;

  double *Axx_smooth = calloc(width * height, sizeof(double));
  double *Axy_smooth = calloc(width * height, sizeof(double));
  double *Ayy_smooth = calloc(width * height, sizeof(double));

  if (!Axx_smooth || !Axy_smooth || !Ayy_smooth) {
    free(grad_x);
    free(grad_y);
    free(Axx);
    free(Axy);
    free(Ayy);
    free(Axx_smooth);
    free(Axy_smooth);
    free(Ayy_smooth);
    return NULL;
  }

  // Simple box filter smoothing
  for (unsigned int y = half_kernel; y < height - half_kernel; y++) {
    for (unsigned int x = half_kernel; x < width - half_kernel; x++) {
      unsigned int idx = y * width + x;
      double sum_xx = 0.0, sum_xy = 0.0, sum_yy = 0.0;
      int count = 0;

      for (int ky = -half_kernel; ky <= half_kernel; ky++) {
        for (int kx = -half_kernel; kx <= half_kernel; kx++) {
          int sample_idx = (y + ky) * width + (x + kx);
          sum_xx += Axx[sample_idx];
          sum_xy += Axy[sample_idx];
          sum_yy += Ayy[sample_idx];
          count++;
        }
      }

      Axx_smooth[idx] = sum_xx / count;
      Axy_smooth[idx] = sum_xy / count;
      Ayy_smooth[idx] = sum_yy / count;
    }
  }

  // Compute Foerstner measures w and q
  unsigned char *result = malloc(width * height * 2); // 2 channels: w, q
  if (!result) {
    free(grad_x);
    free(grad_y);
    free(Axx);
    free(Axy);
    free(Ayy);
    free(Axx_smooth);
    free(Axy_smooth);
    free(Ayy_smooth);
    return NULL;
  }

  double max_w = 0.0, max_q = 0.0;
  double *w_values = malloc(width * height * sizeof(double));
  double *q_values = malloc(width * height * sizeof(double));
  if (!w_values || !q_values) {
    free(result);
    free(w_values);
    free(q_values);
    free(grad_x);
    free(grad_y);
    free(Axx);
    free(Axy);
    free(Ayy);
    free(Axx_smooth);
    free(Axy_smooth);
    free(Ayy_smooth);
    return NULL;
  }

  // First pass: compute w and q values and find maximum for normalization
  for (unsigned int i = 0; i < width * height; i++) {
    double det_A =
      Axx_smooth[i] * Ayy_smooth[i] - Axy_smooth[i] * Axy_smooth[i];
    double trace_A = Axx_smooth[i] + Ayy_smooth[i];

    double w = 0.0, q = 0.0;

    if (fabs(trace_A) > 1e-10) { // Avoid division by zero
      w = det_A / trace_A;
      q = 4.0 * det_A / (trace_A * trace_A);
    }

    w_values[i] = fmax(0.0, w); // Ensure non-negative
    q_values[i] = fmax(0.0, q);

    if (w_values[i] > max_w) {
      max_w = w_values[i];
    }
    if (q_values[i] > max_q) {
      max_q = q_values[i];
    }
  }

  // Second pass: normalize and convert to bytes
  for (unsigned int i = 0; i < width * height; i++) {
    unsigned char w_byte = 0, q_byte = 0;

    if (max_w > 0.0) {
      w_byte = (unsigned char)(255.0 * w_values[i] / max_w);
    }
    if (max_q > 0.0) {
      q_byte = (unsigned char)(255.0 * q_values[i] / max_q);
    }

    result[i * 2] = w_byte;     // w measure
    result[i * 2 + 1] = q_byte; // q measure
  }

  free(w_values);
  free(q_values);

  // Cleanup
  free(grad_x);
  free(grad_y);
  free(Axx);
  free(Axy);
  free(Ayy);
  free(Axx_smooth);
  free(Axy_smooth);
  free(Ayy_smooth);

  return result;
}
