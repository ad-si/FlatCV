#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <stdint.h>

/**
 * Generate a histogram visualization image from input image data.
 * For grayscale images, creates a single histogram.
 * For RGB(A) images, creates overlapping histograms for each channel.
 *
 * @param width Width of the input image.
 * @param height Height of the input image.
 * @param channels Number of channels in the input image (1, 3, or 4).
 * @param data Pointer to the input pixel data.
 * @param out_width Pointer to store the output histogram width.
 * @param out_height Pointer to store the output histogram height.
 * @return Pointer to the histogram image data (RGBA format).
 */
uint8_t *fcv_generate_histogram(
  uint32_t width,
  uint32_t height,
  uint32_t channels,
  uint8_t const *const data,
  uint32_t *out_width,
  uint32_t *out_height
);

#endif // HISTOGRAM_H
