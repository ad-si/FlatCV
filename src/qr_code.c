#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "qr_code.h"
#else
#include "flatcv.h"
#endif

#define MAX_QR_CODES 10
#define MAX_CANDIDATES 200
#define MAX_TRIPLES 10

/* ---- Types ---- */

typedef struct {
  float x, y, module_size;
} FinderPattern;

/* ---- Adaptive binarization ----
   Hybrid thresholding: combines a global Otsu threshold with a local-variance
   check via a mean/square integral image. In high-contrast windows we
   threshold against the local mean (handles uneven illumination on e-ink
   displays and perspective-lit photos); in low-contrast windows we fall back
   to the global Otsu threshold (preserves large uniform dark/light regions
   that a pure local mean would invert).
   Output buffer: 0 for dark, 255 for light. */
static int compute_otsu_threshold(uint8_t const *gray, int w, int h) {
  size_t n = (size_t)w * (size_t)h;
  uint32_t hist[256] = {0};
  for (size_t i = 0; i < n; i++) {
    hist[gray[i]]++;
  }
  double total = (double)n;
  double sum_all = 0.0;
  for (int i = 0; i < 256; i++) {
    sum_all += (double)i * hist[i];
  }
  double w_bg = 0.0, sum_bg = 0.0;
  double max_var = 0.0;
  int best_t = 128;
  for (int t = 0; t < 256; t++) {
    w_bg += hist[t];
    if (w_bg == 0.0 || w_bg == total) {
      continue;
    }
    sum_bg += (double)t * hist[t];
    double w_fg = total - w_bg;
    double mean_bg = sum_bg / w_bg;
    double mean_fg = (sum_all - sum_bg) / w_fg;
    double diff = mean_bg - mean_fg;
    double var_between = w_bg * w_fg * diff * diff;
    if (var_between > max_var) {
      max_var = var_between;
      best_t = t;
    }
  }
  return best_t;
}

static uint8_t *binarize_global_otsu(uint8_t const *gray, int w, int h) {
  if (w <= 0 || h <= 0) {
    return NULL;
  }
  size_t n = (size_t)w * (size_t)h;
  uint8_t *bin = malloc(n);
  if (!bin) {
    return NULL;
  }
  int t = compute_otsu_threshold(gray, w, h);
  for (size_t i = 0; i < n; i++) {
    bin[i] = (gray[i] < t) ? 0 : 255;
  }
  return bin;
}

static uint8_t *binarize_adaptive(uint8_t const *gray, int w, int h) {
  if (w <= 0 || h <= 0) {
    return NULL;
  }
  size_t n = (size_t)w * (size_t)h;
  uint8_t *bin = malloc(n);
  if (!bin) {
    return NULL;
  }
  size_t iw = (size_t)w + 1;
  uint64_t *ii_sum = calloc(iw * (size_t)(h + 1), sizeof(uint64_t));
  uint64_t *ii_sq = calloc(iw * (size_t)(h + 1), sizeof(uint64_t));
  if (!ii_sum || !ii_sq) {
    free(bin);
    free(ii_sum);
    free(ii_sq);
    return NULL;
  }
  for (int y = 0; y < h; y++) {
    uint64_t row_sum = 0;
    uint64_t row_sq = 0;
    for (int x = 0; x < w; x++) {
      uint8_t v = gray[y * w + x];
      row_sum += v;
      row_sq += (uint64_t)v * v;
      size_t idx = (size_t)(y + 1) * iw + (size_t)(x + 1);
      ii_sum[idx] = ii_sum[idx - iw] + row_sum;
      ii_sq[idx] = ii_sq[idx - iw] + row_sq;
    }
  }
  int global_t = compute_otsu_threshold(gray, w, h);
  int block = (w < h ? w : h) / 8;
  if (block < 15) {
    block = 15;
  }
  if (block > 101) {
    block = 101;
  }
  if ((block & 1) == 0) {
    block++;
  }
  int half = block / 2;
  /* Below min_std the window is treated as uniform and the global threshold
     decides the pixel class. This keeps large uniform dark regions (e.g.
     finder-pattern centers, dark backgrounds) from being inverted by a local
     mean that sits right on the pixel value. */
  const double min_std = 24.0;
  for (int y = 0; y < h; y++) {
    int y0 = y - half;
    if (y0 < 0) {
      y0 = 0;
    }
    int y1 = y + half;
    if (y1 >= h) {
      y1 = h - 1;
    }
    for (int x = 0; x < w; x++) {
      int x0 = x - half;
      if (x0 < 0) {
        x0 = 0;
      }
      int x1 = x + half;
      if (x1 >= w) {
        x1 = w - 1;
      }
      double count = (double)((y1 - y0 + 1) * (x1 - x0 + 1));
      size_t a = (size_t)(y1 + 1) * iw + (size_t)(x1 + 1);
      size_t b = (size_t)y0 * iw + (size_t)(x1 + 1);
      size_t c = (size_t)(y1 + 1) * iw + (size_t)x0;
      size_t d = (size_t)y0 * iw + (size_t)x0;
      double sum = (double)(ii_sum[a] - ii_sum[b] - ii_sum[c] + ii_sum[d]);
      double sqsum = (double)(ii_sq[a] - ii_sq[b] - ii_sq[c] + ii_sq[d]);
      double mean = sum / count;
      double var = sqsum / count - mean * mean;
      if (var < 0.0) {
        var = 0.0;
      }
      double std = sqrt(var);
      uint8_t gv = gray[y * w + x];
      int dark;
      if (std < min_std) {
        dark = (gv < global_t);
      }
      else {
        dark = ((double)gv < mean);
      }
      bin[y * w + x] = dark ? 0 : 255;
    }
  }
  free(ii_sum);
  free(ii_sq);
  return bin;
}

/* ---- Sauvola binarization ----
   T = mean * (1 + k * (std / R - 1)), k=0.34, R=128. Scales the threshold
   by local std, which catches faded/low-contrast modules that the fixed
   min_std fallback in binarize_adaptive misclassifies. Block deliberately
   smaller than binarize_adaptive's so it stays responsive when modules are
   only 2-3 px wide. */
static uint8_t *binarize_sauvola(uint8_t const *gray, int w, int h) {
  if (w <= 0 || h <= 0) {
    return NULL;
  }
  size_t n = (size_t)w * (size_t)h;
  uint8_t *bin = malloc(n);
  if (!bin) {
    return NULL;
  }
  size_t iw = (size_t)w + 1;
  uint64_t *ii_sum = calloc(iw * (size_t)(h + 1), sizeof(uint64_t));
  uint64_t *ii_sq = calloc(iw * (size_t)(h + 1), sizeof(uint64_t));
  if (!ii_sum || !ii_sq) {
    free(bin);
    free(ii_sum);
    free(ii_sq);
    return NULL;
  }
  for (int y = 0; y < h; y++) {
    uint64_t row_sum = 0;
    uint64_t row_sq = 0;
    for (int x = 0; x < w; x++) {
      uint8_t v = gray[y * w + x];
      row_sum += v;
      row_sq += (uint64_t)v * v;
      size_t idx = (size_t)(y + 1) * iw + (size_t)(x + 1);
      ii_sum[idx] = ii_sum[idx - iw] + row_sum;
      ii_sq[idx] = ii_sq[idx - iw] + row_sq;
    }
  }
  int block = (w < h ? w : h) / 12;
  if (block < 11) {
    block = 11;
  }
  if (block > 81) {
    block = 81;
  }
  if ((block & 1) == 0) {
    block++;
  }
  int half = block / 2;
  const double k = 0.34;
  const double R = 128.0;
  for (int y = 0; y < h; y++) {
    int y0 = y - half;
    if (y0 < 0) {
      y0 = 0;
    }
    int y1 = y + half;
    if (y1 >= h) {
      y1 = h - 1;
    }
    for (int x = 0; x < w; x++) {
      int x0 = x - half;
      if (x0 < 0) {
        x0 = 0;
      }
      int x1 = x + half;
      if (x1 >= w) {
        x1 = w - 1;
      }
      double count = (double)((y1 - y0 + 1) * (x1 - x0 + 1));
      size_t a = (size_t)(y1 + 1) * iw + (size_t)(x1 + 1);
      size_t b = (size_t)y0 * iw + (size_t)(x1 + 1);
      size_t c = (size_t)(y1 + 1) * iw + (size_t)x0;
      size_t d = (size_t)y0 * iw + (size_t)x0;
      double sum = (double)(ii_sum[a] - ii_sum[b] - ii_sum[c] + ii_sum[d]);
      double sqsum = (double)(ii_sq[a] - ii_sq[b] - ii_sq[c] + ii_sq[d]);
      double mean = sum / count;
      double var = sqsum / count - mean * mean;
      if (var < 0.0) {
        var = 0.0;
      }
      double std = sqrt(var);
      double thresh = mean * (1.0 + k * (std / R - 1.0));
      uint8_t gv = gray[y * w + x];
      bin[y * w + x] = ((double)gv < thresh) ? 0 : 255;
    }
  }
  free(ii_sum);
  free(ii_sq);
  return bin;
}

/* ---- Hybrid binarization (zxing-cpp port) ----
   Port of zxing-cpp's HybridBinarizer (core/src/HybridBinarizer.cpp, NEW
   algorithm). Works in 8x8 blocks and computes a per-block threshold of
   (min+max)/2 only when the dynamic range exceeds 24; low-contrast blocks
   are marked "unknown" and inherit their threshold from a 5x5 smoothing
   over the *known* neighbours, followed by raster flood-fill of any
   surviving zeros. This is substantially more robust than binarize_adaptive
   on low-contrast pastel palettes: flatcv's adaptive falls back to the
   global Otsu threshold whenever local std<24, which misclassifies entire
   interior regions; zxing's scheme instead propagates an edge-of-QR
   threshold inward.
   Output: 0 = dark, 255 = light. Returns NULL if image smaller than the
   5x5 window (40 px) — caller should fall back to global Otsu. */
#define HB_BLOCK_SIZE 8
#define HB_WINDOW_BLOCKS 5
#define HB_WINDOW_SIZE (HB_BLOCK_SIZE * HB_WINDOW_BLOCKS)
#define HB_MIN_DYNAMIC_RANGE 24

static uint8_t *binarize_hybrid(uint8_t const *gray, int w, int h) {
  if (w < HB_WINDOW_SIZE || h < HB_WINDOW_SIZE) {
    return NULL;
  }
  int sub_w = (w + HB_BLOCK_SIZE - 1) / HB_BLOCK_SIZE;
  int sub_h = (h + HB_BLOCK_SIZE - 1) / HB_BLOCK_SIZE;
  size_t sub_n = (size_t)sub_w * (size_t)sub_h;
  uint8_t *raw = malloc(sub_n);
  uint8_t *smoothed = malloc(sub_n);
  uint8_t *bin = malloc((size_t)w * (size_t)h);
  if (!raw || !smoothed || !bin) {
    free(raw);
    free(smoothed);
    free(bin);
    return NULL;
  }

  /* Per-block threshold: (max+min)/2 if range exceeds min_dynamic_range,
     else 0 (unknown — filled in by smoothing). Last row/col of blocks is
     clamped so the 8x8 window stays fully inside the image. */
  for (int by = 0; by < sub_h; by++) {
    int y0 = by * HB_BLOCK_SIZE;
    if (y0 > h - HB_BLOCK_SIZE) {
      y0 = h - HB_BLOCK_SIZE;
    }
    for (int bx = 0; bx < sub_w; bx++) {
      int x0 = bx * HB_BLOCK_SIZE;
      if (x0 > w - HB_BLOCK_SIZE) {
        x0 = w - HB_BLOCK_SIZE;
      }
      uint8_t mn = 255, mx = 0;
      for (int yy = 0; yy < HB_BLOCK_SIZE; yy++) {
        uint8_t const *row = gray + (size_t)(y0 + yy) * (size_t)w + x0;
        for (int xx = 0; xx < HB_BLOCK_SIZE; xx++) {
          uint8_t v = row[xx];
          if (v < mn) {
            mn = v;
          }
          if (v > mx) {
            mx = v;
          }
        }
      }
      raw[(size_t)by * sub_w + bx] =
        (mx - mn > HB_MIN_DYNAMIC_RANGE) ? (uint8_t)(((int)mx + mn) / 2) : 0;
    }
  }

  /* 5x5 gaussian-like smoothing over non-zero thresholds (center counted
     twice). Low-contrast blocks with no contrasty neighbour remain 0. */
  const int R = HB_WINDOW_BLOCKS / 2;
  for (int by = 0; by < sub_h; by++) {
    for (int bx = 0; bx < sub_w; bx++) {
      int left = bx;
      if (left < R) {
        left = R;
      }
      else if (left > sub_w - R - 1) {
        left = sub_w - R - 1;
      }
      int top = by;
      if (top < R) {
        top = R;
      }
      else if (top > sub_h - R - 1) {
        top = sub_h - R - 1;
      }
      int center = raw[(size_t)by * sub_w + bx];
      int sum = center * 2;
      int n = (center > 0) ? 2 : 0;
      for (int dy = -R; dy <= R; dy++) {
        for (int dx = -R; dx <= R; dx++) {
          int t = raw[(size_t)(top + dy) * sub_w + (left + dx)];
          sum += t;
          if (t > 0) {
            n++;
          }
        }
      }
      smoothed[(size_t)by * sub_w + bx] = (n > 0) ? (uint8_t)(sum / n) : 0;
    }
  }
  free(raw);

  /* Raster flood-fill of any remaining zero-threshold blocks. Propagates
     the nearest-preceding non-zero threshold forward; runs of trailing
     zeros inherit from the last non-zero value seen. */
  int last = -1;
  for (size_t i = 0; i < sub_n; i++) {
    if (smoothed[i]) {
      if ((int)i != last + 1) {
        uint8_t fill = smoothed[i];
        for (int j = last + 1; j < (int)i; j++) {
          smoothed[j] = fill;
        }
      }
      last = (int)i;
    }
  }
  if (last < (int)sub_n - 1) {
    uint8_t fill = (last >= 0) ? smoothed[last] : 128;
    for (int j = last + 1; j < (int)sub_n; j++) {
      smoothed[j] = fill;
    }
  }

  /* Apply per-block threshold to every pixel. Pixels in the rightmost/
     bottom partial block simply use that block's threshold — block-index
     lookup via integer division is fine even when (w % BLOCK_SIZE) != 0
     because sub_w/sub_h were computed with ceil(). */
  for (int y = 0; y < h; y++) {
    int by = y / HB_BLOCK_SIZE;
    if (by >= sub_h) {
      by = sub_h - 1;
    }
    uint8_t const *row = gray + (size_t)y * w;
    uint8_t *out = bin + (size_t)y * w;
    for (int x = 0; x < w; x++) {
      int bx = x / HB_BLOCK_SIZE;
      if (bx >= sub_w) {
        bx = sub_w - 1;
      }
      uint8_t t = smoothed[(size_t)by * sub_w + bx];
      out[x] = (row[x] <= t) ? 0 : 255;
    }
  }
  free(smoothed);
  return bin;
}

/* ---- Unsharp mask on grayscale ----
   3-tap separable Gaussian blur, then sharp = clamp(2*g - blurred). The
   small kernel matches level_4/5 blur radii (0.8-1.8 px). Returns a freshly
   allocated grayscale buffer. */
static uint8_t *unsharp_mask_gray(uint8_t const *gray, int w, int h) {
  if (w <= 2 || h <= 2) {
    return NULL;
  }
  size_t n = (size_t)w * (size_t)h;
  uint8_t *blurred = malloc(n);
  uint8_t *tmp = malloc(n);
  uint8_t *sharp = malloc(n);
  if (!blurred || !tmp || !sharp) {
    free(blurred);
    free(tmp);
    free(sharp);
    return NULL;
  }
  /* Horizontal pass [1 2 1] / 4 into tmp. */
  for (int y = 0; y < h; y++) {
    uint8_t const *row = gray + (size_t)y * w;
    uint8_t *out_row = tmp + (size_t)y * w;
    out_row[0] = row[0];
    out_row[w - 1] = row[w - 1];
    for (int x = 1; x < w - 1; x++) {
      int v = row[x - 1] + 2 * row[x] + row[x + 1];
      out_row[x] = (uint8_t)(v >> 2);
    }
  }
  /* Vertical pass [1 2 1] / 4 from tmp into blurred. */
  for (int x = 0; x < w; x++) {
    blurred[x] = tmp[x];
    blurred[(size_t)(h - 1) * w + x] = tmp[(size_t)(h - 1) * w + x];
  }
  for (int y = 1; y < h - 1; y++) {
    for (int x = 0; x < w; x++) {
      int v = tmp[(size_t)(y - 1) * w + x] + 2 * tmp[(size_t)y * w + x] +
              tmp[(size_t)(y + 1) * w + x];
      blurred[(size_t)y * w + x] = (uint8_t)(v >> 2);
    }
  }
  /* sharp = clamp(2*gray - blurred). */
  for (size_t i = 0; i < n; i++) {
    int v = 2 * (int)gray[i] - (int)blurred[i];
    if (v < 0) {
      v = 0;
    }
    if (v > 255) {
      v = 255;
    }
    sharp[i] = (uint8_t)v;
  }
  free(blurred);
  free(tmp);
  return sharp;
}

/* ---- N-x bilinear upsample on grayscale ----
   Returns a freshly allocated (N*w x N*h) buffer. Larger N gives bigger
   effective module sizes (helping finder ratio checks and subpixel
   refinement) at quadratic memory + time cost. */
static uint8_t *upsample_nx_bilinear(uint8_t const *gray, int w, int h, int n) {
  if (w <= 0 || h <= 0 || n < 2) {
    return NULL;
  }
  int w2 = w * n;
  int h2 = h * n;
  double inv = 1.0 / (double)n;
  uint8_t *out = malloc((size_t)w2 * (size_t)h2);
  if (!out) {
    return NULL;
  }
  for (int y = 0; y < h2; y++) {
    double sy = ((double)y + 0.5) * inv - 0.5;
    int y0 = (int)floor(sy);
    int y1 = y0 + 1;
    double fy = sy - y0;
    if (y0 < 0) {
      y0 = 0;
    }
    if (y1 < 0) {
      y1 = 0;
    }
    if (y0 > h - 1) {
      y0 = h - 1;
    }
    if (y1 > h - 1) {
      y1 = h - 1;
    }
    for (int x = 0; x < w2; x++) {
      double sx = ((double)x + 0.5) * inv - 0.5;
      int x0 = (int)floor(sx);
      int x1 = x0 + 1;
      double fx = sx - x0;
      if (x0 < 0) {
        x0 = 0;
      }
      if (x1 < 0) {
        x1 = 0;
      }
      if (x0 > w - 1) {
        x0 = w - 1;
      }
      if (x1 > w - 1) {
        x1 = w - 1;
      }
      double v00 = gray[y0 * w + x0];
      double v01 = gray[y0 * w + x1];
      double v10 = gray[y1 * w + x0];
      double v11 = gray[y1 * w + x1];
      double v0 = v00 * (1.0 - fx) + v01 * fx;
      double v1 = v10 * (1.0 - fx) + v11 * fx;
      double v = v0 * (1.0 - fy) + v1 * fy;
      if (v < 0.0) {
        v = 0.0;
      }
      if (v > 255.0) {
        v = 255.0;
      }
      out[y * w2 + x] = (uint8_t)(v + 0.5);
    }
  }
  return out;
}

/* ---- 2x grayscale downsample (box average) ----
   Each output pixel is the mean of a 2x2 input block. Mirrors zxing-cpp's
   LumImagePyramid step. Anti-aliased QR-edge pixels get averaged with
   interior pixels, producing new intermediate luma values within an 8x8
   block that raise its dynamic range above the hybrid binariser's
   MIN_DYNAMIC_RANGE=24 threshold. Essential for pastel palettes where
   module-vs-background contrast at full resolution is below that gate
   everywhere in the image. */
static uint8_t *downsample_2x_box(uint8_t const *gray, int w, int h) {
  if (w < 2 || h < 2) {
    return NULL;
  }
  int nw = w / 2;
  int nh = h / 2;
  uint8_t *out = malloc((size_t)nw * (size_t)nh);
  if (!out) {
    return NULL;
  }
  for (int y = 0; y < nh; y++) {
    int sy = y * 2;
    uint8_t const *r0 = gray + (size_t)sy * w;
    uint8_t const *r1 = gray + (size_t)(sy + 1) * w;
    uint8_t *dst = out + (size_t)y * nw;
    for (int x = 0; x < nw; x++) {
      int sx = x * 2;
      int sum = r0[sx] + r0[sx + 1] + r1[sx] + r1[sx + 1];
      dst[x] = (uint8_t)(sum >> 2);
    }
  }
  return out;
}

/* ---- 3x3 grayscale median filter ----
   Removes salt-and-pepper noise without softening edges (unlike Gaussian
   blur). Targets level_5's heavy random noise (sigma 4-9) that injects
   isolated black/white pixels into otherwise-uniform module interiors. */
static uint8_t *median_filter_3x3(uint8_t const *gray, int w, int h) {
  if (w <= 2 || h <= 2) {
    return NULL;
  }
  uint8_t *out = malloc((size_t)w * h);
  if (!out) {
    return NULL;
  }
  /* Copy the 1-pixel border unchanged. */
  for (int x = 0; x < w; x++) {
    out[x] = gray[x];
    out[(size_t)(h - 1) * w + x] = gray[(size_t)(h - 1) * w + x];
  }
  for (int y = 1; y < h - 1; y++) {
    out[y * w] = gray[y * w];
    out[y * w + (w - 1)] = gray[y * w + (w - 1)];
  }
  for (int y = 1; y < h - 1; y++) {
    for (int x = 1; x < w - 1; x++) {
      uint8_t v[9];
      v[0] = gray[(y - 1) * w + (x - 1)];
      v[1] = gray[(y - 1) * w + x];
      v[2] = gray[(y - 1) * w + (x + 1)];
      v[3] = gray[y * w + (x - 1)];
      v[4] = gray[y * w + x];
      v[5] = gray[y * w + (x + 1)];
      v[6] = gray[(y + 1) * w + (x - 1)];
      v[7] = gray[(y + 1) * w + x];
      v[8] = gray[(y + 1) * w + (x + 1)];
      /* Insertion sort to find the median (5th element of 9). */
      for (int i = 1; i < 9; i++) {
        uint8_t tmp = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > tmp) {
          v[j + 1] = v[j];
          j--;
        }
        v[j + 1] = tmp;
      }
      out[y * w + x] = v[4];
    }
  }
  return out;
}

/* ---- Inverted polarity ----
   QR spec allows light-on-dark modules (level_13 corpus); flatcv's finder
   and sampling code assume dark-on-light. Retrying with 255 - v lets the
   same pipeline decode both polarities without any per-stage branching. */
static uint8_t *invert_gray(uint8_t const *gray, int w, int h) {
  if (w <= 0 || h <= 0) {
    return NULL;
  }
  size_t n = (size_t)w * (size_t)h;
  uint8_t *out = malloc(n);
  if (!out) {
    return NULL;
  }
  for (size_t i = 0; i < n; i++) {
    out[i] = (uint8_t)(255 - gray[i]);
  }
  return out;
}

/* ---- Pixel access ---- */

static int is_dark(uint8_t const *px, int w, int h, int x, int y) {
  if (x < 0 || x >= w || y < 0 || y >= h) {
    return 0;
  }
  return px[y * w + x] < 128;
}

/* ---- Finder pattern ratio check (1:1:3:1:1) ---- */

static int check_ratio(const int s[5]) {
  int total = 0;
  for (int i = 0; i < 5; i++) {
    if (s[i] == 0) {
      return 0;
    }
    total += s[i];
  }
  if (total < 7) {
    return 0;
  }
  float m = (float)total / 7.0f;
  float v = m * 0.7f;
  return (
    fabsf(s[0] - m) <= v && fabsf(s[1] - m) <= v &&
    fabsf(s[2] - 3 * m) <= 3 * v && fabsf(s[3] - m) <= v && fabsf(s[4] - m) <= v
  );
}

/* ---- Cross-check along an arbitrary angle ----
   Walk along direction (cos_a, sin_a) through (cx, cy), counting the
   1:1:3:1:1 dark-light-dark-light-dark run pattern.
   Returns module_size if valid, -1 otherwise. Writes refined center. */
static float cross_check_angle(
  uint8_t const *px,
  int w,
  int h,
  float cx,
  float cy,
  float cos_a,
  float sin_a,
  int exp_total,
  float *out_cx,
  float *out_cy
) {
  int max_walk = (w > h ? w : h);

  /* Walk negative: expect dark(center), light, dark at boundary */
  int neg_dark_center = 0;
  int neg_light = 0;
  int neg_dark_outer = 0;
  int phase = 0;

  for (int step = 1; step <= max_walk && phase < 3; step++) {
    float fx = cx - step * cos_a;
    float fy = cy - step * sin_a;
    int ix = (int)(fx + 0.5f), iy = (int)(fy + 0.5f);
    if (ix < 0 || ix >= w || iy < 0 || iy >= h) {
      break;
    }
    int dark = is_dark(px, w, h, ix, iy);
    if (phase == 0) {
      if (dark) {
        neg_dark_center++;
      }
      else {
        neg_light = 1;
        phase = 1;
      }
    }
    else if (phase == 1) {
      if (!dark) {
        neg_light++;
      }
      else {
        neg_dark_outer = 1;
        phase = 2;
      }
    }
    else {
      if (dark) {
        neg_dark_outer++;
      }
      else {
        phase = 3;
      }
    }
  }

  /* Walk positive: expect dark(center), light, dark at boundary */
  int pos_dark_center = 0;
  int pos_light = 0;
  int pos_dark_outer = 0;
  phase = 0;

  for (int step = 1; step <= max_walk && phase < 3; step++) {
    float fx = cx + step * cos_a;
    float fy = cy + step * sin_a;
    int ix = (int)(fx + 0.5f), iy = (int)(fy + 0.5f);
    if (ix < 0 || ix >= w || iy < 0 || iy >= h) {
      break;
    }
    int dark = is_dark(px, w, h, ix, iy);
    if (phase == 0) {
      if (dark) {
        pos_dark_center++;
      }
      else {
        pos_light = 1;
        phase = 1;
      }
    }
    else if (phase == 1) {
      if (!dark) {
        pos_light++;
      }
      else {
        pos_dark_outer = 1;
        phase = 2;
      }
    }
    else {
      if (dark) {
        pos_dark_outer++;
      }
      else {
        phase = 3;
      }
    }
  }

  int s[5];
  s[0] = neg_dark_outer;
  s[1] = neg_light;
  s[2] = neg_dark_center + 1 + pos_dark_center;
  s[3] = pos_light;
  s[4] = pos_dark_outer;

  int total = s[0] + s[1] + s[2] + s[3] + s[4];
  if (exp_total > 0 && abs(total - exp_total) > exp_total) {
    return -1;
  }
  if (!check_ratio(s)) {
    return -1;
  }

  float offset = (float)(pos_dark_center - neg_dark_center) / 2.0f;
  *out_cx = cx + offset * cos_a;
  *out_cy = cy + offset * sin_a;

  return (float)total / 7.0f;
}

/* ---- Radial cross-check at multiple angles ---- */

static int cross_check_radial(
  uint8_t const *px,
  int w,
  int h,
  float cx,
  float cy,
  float exp_ms,
  float *out_cx,
  float *out_cy,
  float *out_ms
) {
  /* 8 angles: 0, 22.5, 45, 67.5, 90, 112.5, 135, 157.5 degrees */
  static const float angles[8] = {
    0.0f,
    0.3927f,
    0.7854f,
    1.1781f,
    1.5708f,
    1.9635f,
    2.3562f,
    2.7489f,
  };
  int exp_total = (int)(exp_ms * 7.0f + 0.5f);
  float rcx_arr[8], rcy_arr[8], rms_arr[8];
  int passed[8];
  int npass = 0;

  for (int a = 0; a < 8; a++) {
    float ca = cosf(angles[a]), sa = sinf(angles[a]);
    rms_arr[a] = cross_check_angle(
      px,
      w,
      h,
      cx,
      cy,
      ca,
      sa,
      exp_total,
      &rcx_arr[a],
      &rcy_arr[a]
    );
    passed[a] = (rms_arr[a] > 0);
    if (passed[a]) {
      npass++;
    }
  }

  if (npass < 4) {
    return 0;
  }

  int best_pair = -1;
  float best_pair_ms_match = 1e9f;
  for (int a = 0; a < 4; a++) {
    int b = a + 4;
    if (passed[a] && passed[b]) {
      float ms_err = fabsf(rms_arr[a] - exp_ms) + fabsf(rms_arr[b] - exp_ms);
      if (best_pair < 0 || ms_err < best_pair_ms_match) {
        best_pair = a;
        best_pair_ms_match = ms_err;
      }
    }
  }

  if (best_pair >= 0) {
    int a = best_pair, b = best_pair + 4;
    float ca = cosf(angles[a]), sa = sinf(angles[a]);
    float off_a = (rcx_arr[a] - cx) * ca + (rcy_arr[a] - cy) * sa;
    float cb = cosf(angles[b]), sb = sinf(angles[b]);
    float off_b = (rcx_arr[b] - cx) * cb + (rcy_arr[b] - cy) * sb;
    *out_cx = cx + off_a * ca + off_b * cb;
    *out_cy = cy + off_a * sa + off_b * sb;
    *out_ms = (rms_arr[a] + rms_arr[b]) / 2.0f;
  }
  else {
    float sum_cx = 0, sum_cy = 0, sum_ms = 0;
    for (int a = 0; a < 8; a++) {
      if (passed[a]) {
        sum_cx += rcx_arr[a];
        sum_cy += rcy_arr[a];
        sum_ms += rms_arr[a];
      }
    }
    *out_cx = sum_cx / npass;
    *out_cy = sum_cy / npass;
    *out_ms = sum_ms / npass;
  }
  return 1;
}

/* ---- Merge a candidate into the list ---- */

static int merge_candidate(
  FinderPattern *cands,
  int nc,
  int max_cands,
  float cx,
  float cy,
  float ms
) {
  float merge_r = ms * 1.5f;
  for (int c = 0; c < nc; c++) {
    float dx = cands[c].x - cx;
    float dy = cands[c].y - cy;
    if (sqrtf(dx * dx + dy * dy) < merge_r) {
      cands[c].x = (cands[c].x + cx) / 2;
      cands[c].y = (cands[c].y + cy) / 2;
      cands[c].module_size = (cands[c].module_size + ms) / 2;
      return nc;
    }
  }
  if (nc < max_cands) {
    cands[nc].x = cx;
    cands[nc].y = cy;
    cands[nc].module_size = ms;
    return nc + 1;
  }
  return nc;
}

/* ---- Scan a single line of pixels for finder patterns ---- */

typedef struct {
  uint8_t const *px;
  int w, h;
  int x0, y0, dx, dy, len;
} ScanLine;

static int get_dark_at(const ScanLine *sl, int step) {
  int x = sl->x0 + step * sl->dx;
  int y = sl->y0 + step * sl->dy;
  return is_dark(sl->px, sl->w, sl->h, x, y);
}

static void scan_one_line(
  const ScanLine *sl,
  FinderPattern *cands,
  int *nc,
  int max_cands
) {
  int s[5] = {0};
  int st = 0;

  for (int i = 0; i < sl->len; i++) {
    int dark = get_dark_at(sl, i);
    if (dark) {
      if (st == 1 || st == 3) {
        st++;
      }
      s[st]++;
    }
    else {
      if (st == 0 || st == 2) {
        st++;
        s[st]++;
      }
      else if (st == 4) {
        if (check_ratio(s)) {
          int total = s[0] + s[1] + s[2] + s[3] + s[4];
          float center_step = (float)(i - s[4] - s[3]) - (float)s[2] / 2.0f;
          float cx = sl->x0 + center_step * sl->dx;
          float cy = sl->y0 + center_step * sl->dy;
          float ms = (float)total / 7.0f;

          float rcx, rcy, rms;
          if (cross_check_radial(
                sl->px,
                sl->w,
                sl->h,
                cx,
                cy,
                ms,
                &rcx,
                &rcy,
                &rms
              )) {
            *nc = merge_candidate(cands, *nc, max_cands, rcx, rcy, rms);
          }
        }
        s[0] = s[2];
        s[1] = s[3];
        s[2] = s[4];
        s[3] = 1;
        s[4] = 0;
        st = 3;
      }
      else {
        s[st]++;
      }
    }
  }
}

/* ---- Find finder patterns with multi-direction scanning ---- */

static int
find_finders(uint8_t const *px, int w, int h, FinderPattern *out, int max_out) {
  FinderPattern cands[MAX_CANDIDATES];
  int nc = 0;
  ScanLine sl;
  sl.px = px;
  sl.w = w;
  sl.h = h;

  /* Direction 1: Horizontal */
  sl.dx = 1;
  sl.dy = 0;
  for (int y = 0; y < h && nc < MAX_CANDIDATES; y++) {
    sl.x0 = 0;
    sl.y0 = y;
    sl.len = w;
    scan_one_line(&sl, cands, &nc, MAX_CANDIDATES);
  }

  /* Direction 2: Vertical */
  sl.dx = 0;
  sl.dy = 1;
  for (int x = 0; x < w && nc < MAX_CANDIDATES; x++) {
    sl.x0 = x;
    sl.y0 = 0;
    sl.len = h;
    scan_one_line(&sl, cands, &nc, MAX_CANDIDATES);
  }

  /* Direction 3: Diagonal 45° */
  sl.dx = 1;
  sl.dy = 1;
  for (int y = 0; y < h && nc < MAX_CANDIDATES; y++) {
    sl.x0 = 0;
    sl.y0 = y;
    sl.len = (w < h - y) ? w : h - y;
    scan_one_line(&sl, cands, &nc, MAX_CANDIDATES);
  }
  for (int x = 1; x < w && nc < MAX_CANDIDATES; x++) {
    sl.x0 = x;
    sl.y0 = 0;
    sl.len = (w - x < h) ? w - x : h;
    scan_one_line(&sl, cands, &nc, MAX_CANDIDATES);
  }

  /* Direction 4: Diagonal 135° */
  sl.dx = 1;
  sl.dy = -1;
  for (int y = 0; y < h && nc < MAX_CANDIDATES; y++) {
    sl.x0 = 0;
    sl.y0 = y;
    sl.len = (w < y + 1) ? w : y + 1;
    scan_one_line(&sl, cands, &nc, MAX_CANDIDATES);
  }
  for (int x = 1; x < w && nc < MAX_CANDIDATES; x++) {
    sl.x0 = x;
    sl.y0 = h - 1;
    sl.len = (w - x < h) ? w - x : h;
    scan_one_line(&sl, cands, &nc, MAX_CANDIDATES);
  }

  int n = (nc < max_out) ? nc : max_out;
  for (int i = 0; i < n; i++) {
    out[i] = cands[i];
  }
  return n;
}

/* ---- Subpixel finder center refinement ----
   Once a candidate center is detected, refine it by computing the weighted
   centroid of dark pixels in the inner 3x3-module dark square. This fixes
   the ±2-3 pixel systematic bias from integer-pixel 1D scans, which
   otherwise throws off grid sampling under perspective distortion. */
static void
refine_finder_center(uint8_t const *gray, int w, int h, FinderPattern *fp) {
  float ms = fp->module_size;
  if (ms < 1.0f) {
    return;
  }
  /* Search in a 3-module window centered on the current estimate so the
     centroid is dominated by the inner 3x3 dark core and not the outer
     7x7 dark ring (which would pull the result off-center). */
  int r = (int)(ms * 1.5f);
  int x0 = (int)(fp->x + 0.5f) - r;
  int x1 = (int)(fp->x + 0.5f) + r;
  int y0 = (int)(fp->y + 0.5f) - r;
  int y1 = (int)(fp->y + 0.5f) + r;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 >= w) {
    x1 = w - 1;
  }
  if (y1 >= h) {
    y1 = h - 1;
  }
  if (x1 <= x0 || y1 <= y0) {
    return;
  }
  /* Compute local min/max to threshold adaptively within this tiny window. */
  uint8_t lo = 255, hi = 0;
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      uint8_t v = gray[y * w + x];
      if (v < lo) {
        lo = v;
      }
      if (v > hi) {
        hi = v;
      }
    }
  }
  if (hi <= lo + 10) {
    return; /* no contrast — leave center alone */
  }
  int t = (lo + hi) / 2;
  double sx = 0, sy = 0, sw = 0;
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      uint8_t v = gray[y * w + x];
      if (v < t) {
        double weight = (double)(t - v);
        sx += (double)x * weight;
        sy += (double)y * weight;
        sw += weight;
      }
    }
  }
  if (sw > 0) {
    fp->x = (float)(sx / sw);
    fp->y = (float)(sy / sw);
  }
}

/* ---- Identify TL, TR, BL ---- */

static float fp_dist(FinderPattern a, FinderPattern b) {
  float dx = a.x - b.x, dy = a.y - b.y;
  return sqrtf(dx * dx + dy * dy);
}

static void identify_triple(
  FinderPattern a,
  FinderPattern b,
  FinderPattern c,
  FinderPattern *tl,
  FinderPattern *tr,
  FinderPattern *bl
) {
  *tl = a;
  float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  if (cross > 0) {
    *tr = b;
    *bl = c;
  }
  else {
    *tr = c;
    *bl = b;
  }
}

/* ---- Timing pattern verification ----
   Walk the candidate timing row between two finder centers, offset 3
   modules perpendicular toward the third finder (finder center is at
   module row 3, timing pattern at module row 6). A real QR timing row
   shows alternating dark/light modules at module-size pitch; incidental
   1:1:3:1:1 "finder" triples in random data do not. Returns transitions
   per step in [0,1]; ≥ 0.75 reliably indicates a timing pattern (random
   data averages 0.5 with 95% CI up to ~0.67). */
static float timing_score_pair(
  uint8_t const *px,
  int w,
  int h,
  FinderPattern a,
  FinderPattern b,
  FinderPattern c_toward
) {
  float ms = (a.module_size + b.module_size) / 2.0f;
  if (ms < 1.0f) {
    return 0;
  }
  float ux = b.x - a.x, uy = b.y - a.y;
  float u_len = sqrtf(ux * ux + uy * uy);
  if (u_len < 10.0f * ms) {
    return 0;
  }
  ux /= u_len;
  uy /= u_len;
  float perp_x = -uy, perp_y = ux;
  float to_c_x = c_toward.x - a.x, to_c_y = c_toward.y - a.y;
  if (perp_x * to_c_x + perp_y * to_c_y < 0) {
    perp_x = -perp_x;
    perp_y = -perp_y;
  }
  /* Finder center is the center of module (3,3); timing pattern runs along
     the center of module (·,6). Offset from finder-center line to timing
     line is 6 − 3 = 3 modules. */
  float offset_x = 3.0f * ms * perp_x;
  float offset_y = 3.0f * ms * perp_y;
  float sx = a.x + 4.0f * ms * ux + offset_x;
  float sy = a.y + 4.0f * ms * uy + offset_y;
  float ex = b.x - 4.0f * ms * ux + offset_x;
  float ey = b.y - 4.0f * ms * uy + offset_y;
  float walk_len = sqrtf((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy));
  int n_steps = (int)(walk_len / ms + 0.5f);
  if (n_steps < 6 || n_steps > 165) {
    return 0;
  }
  int prev_dark = -1;
  int transitions = 0;
  int samples = 0;
  for (int i = 0; i <= n_steps; i++) {
    float t = (float)i / (float)n_steps;
    float fx = sx + t * (ex - sx);
    float fy = sy + t * (ey - sy);
    int ix = (int)(fx + 0.5f), iy = (int)(fy + 0.5f);
    if (ix < 0 || ix >= w || iy < 0 || iy >= h) {
      continue;
    }
    int dark = is_dark(px, w, h, ix, iy);
    samples++;
    if (prev_dark != -1 && dark != prev_dark) {
      transitions++;
    }
    prev_dark = dark;
  }
  if (samples < 5) {
    return 0;
  }
  return (float)transitions / (float)(samples - 1);
}

static int select_and_rank(
  uint8_t const *px,
  int w,
  int h,
  FinderPattern *fps,
  int nfp,
  FinderPattern triples[][3],
  int max_triples
) {
  if (nfp < 3) {
    return 0;
  }

  /* Count ms-cluster partners for each candidate: how many other candidates
     have module_size within 15% of mine. Real finder triples are three
     near-identical-ms blobs; scoring-by-ms alone lets wide-run false
     positives in dense data regions push the real triple out of the
     search window on cluttered images (e.g. receipt QR next to a
     barcode). Clustered candidates get promoted so the triple survives. */
  int cluster_count[MAX_CANDIDATES];
  for (int i = 0; i < nfp; i++) {
    int count = 0;
    for (int j = 0; j < nfp; j++) {
      if (j == i) {
        continue;
      }
      float m = (fps[i].module_size > 0.001f) ? fps[i].module_size : 0.001f;
      float ratio = fabsf(fps[i].module_size - fps[j].module_size) / m;
      if (ratio < 0.15f) {
        count++;
      }
    }
    cluster_count[i] = count;
  }

  /* Selection sort by (clustered desc, module_size desc). Candidates with
     ≥ 2 ms-partners come first; within each group, larger ms wins. */
  for (int i = 0; i < nfp - 1; i++) {
    int best = i;
    for (int j = i + 1; j < nfp; j++) {
      int j_cl = cluster_count[j] >= 2;
      int b_cl = cluster_count[best] >= 2;
      int replace = 0;
      if (j_cl && !b_cl) {
        replace = 1;
      }
      else if (j_cl == b_cl && fps[j].module_size > fps[best].module_size) {
        replace = 1;
      }
      if (replace) {
        best = j;
      }
    }
    if (best != i) {
      FinderPattern tmp = fps[i];
      fps[i] = fps[best];
      fps[best] = tmp;
      int tc = cluster_count[i];
      cluster_count[i] = cluster_count[best];
      cluster_count[best] = tc;
    }
  }

  typedef struct {
    float score;
    int i, j, k;
  } Scored;
  int max_scored = max_triples * 4;
  if (max_scored > 200) {
    max_scored = 200;
  }
  Scored *scored = malloc(max_scored * sizeof(Scored));
  if (!scored) {
    return 0;
  }
  int ns = 0;

  /* Consider all clustered candidates (capped at 80 for O(n³) budget), or at
     least 30 if few candidates cluster — preserves original behaviour on
     noise-only scenes while accommodating crowded ones. */
  int num_clustered = 0;
  for (int i = 0; i < nfp; i++) {
    if (cluster_count[i] >= 2) {
      num_clustered++;
    }
  }
  int limit = num_clustered;
  if (limit < 30) {
    limit = 30;
  }
  if (limit > nfp) {
    limit = nfp;
  }
  if (limit > 80) {
    limit = 80;
  }
  for (int i = 0; i < limit; i++) {
    for (int j = i + 1; j < limit; j++) {
      for (int k = j + 1; k < limit; k++) {
        float ms_avg =
          (fps[i].module_size + fps[j].module_size + fps[k].module_size) / 3.0f;
        float ms_var = (fabsf(fps[i].module_size - ms_avg) +
                        fabsf(fps[j].module_size - ms_avg) +
                        fabsf(fps[k].module_size - ms_avg)) /
                       ms_avg;

        float d01 = fp_dist(fps[i], fps[j]);
        float d02 = fp_dist(fps[i], fps[k]);
        float d12 = fp_dist(fps[j], fps[k]);

        float sides[3] = {d01, d02, d12};
        for (int a = 0; a < 2; a++) {
          for (int b = a + 1; b < 3; b++) {
            if (sides[a] > sides[b]) {
              float t = sides[a];
              sides[a] = sides[b];
              sides[b] = t;
            }
          }
        }

        float side_ratio = (sides[1] > 0) ? sides[0] / sides[1] : 0;
        float expected_hyp = sqrtf(sides[0] * sides[0] + sides[1] * sides[1]);
        float hyp_err = (expected_hyp > 0)
                          ? fabsf(sides[2] - expected_hyp) / expected_hyp
                          : 1;

        /* Real QR finder centers sit at module (3.5, 3.5), (sz-3.5, 3.5),
           (3.5, sz-3.5) — so the short side divided by module size should be
           qr_size - 7, at least 14 for v1. Triples formed from incidental
           1:1:3:1:1 patterns in adjacent data modules have small
           sides/ms ratios; saturate the bonus at 14 so real finders outrank
           them without giving oversized triples unbounded advantage. */
        float modules_between = (ms_avg > 0) ? sides[0] / ms_avg : 0;
        /* QR spec: finder-center distance = (qr_size − 7) × module_size, with
           qr_size ∈ [21, 177]. So the longer leg cannot plausibly exceed
           ~170 modules; leave ~10% slack for perspective and scan noise. A
           triple spanning an entire receipt/page at some spurious module size
           blows past this even when its module sizes happen to cluster. */
        float modules_between_long = (ms_avg > 0) ? sides[1] / ms_avg : 0;
        if (modules_between_long > 190.0f) {
          continue;
        }
        float scale_bonus = modules_between / 14.0f;
        if (scale_bonus > 1.0f) {
          scale_bonus = 1.0f;
        }
        /* ms_var dominates: real QR finders have near-identical module sizes;
           triples whose "finders" are false positives at different scales
           get rejected hard. Discount hyp_err so moderate perspective
           distortion doesn't kill the true triple. */
        float score = side_ratio - ms_var * 5.0f - hyp_err * 1.0f + scale_bonus;

        /* Timing-pattern bonus: real finder triples have two pairs whose
           inter-finder line runs along a QR timing row of alternating
           dark/light modules. False-positive triples in dense document/
           receipt text rarely hit this, so the bonus of +1 per timing
           pair lets perspective-distorted real triples outrank clean but
           incidental right-triangle triples. Gate on threshold 0.75 —
           random data ratio is ≤ 0.67 with 95% confidence. */
        int timing_pairs = 0;
        float t_ij = timing_score_pair(px, w, h, fps[i], fps[j], fps[k]);
        float t_ik = timing_score_pair(px, w, h, fps[i], fps[k], fps[j]);
        float t_jk = timing_score_pair(px, w, h, fps[j], fps[k], fps[i]);
        if (t_ij >= 0.75f) {
          timing_pairs++;
        }
        if (t_ik >= 0.75f) {
          timing_pairs++;
        }
        if (t_jk >= 0.75f) {
          timing_pairs++;
        }
        score += (float)timing_pairs;

        if (ns < max_scored) {
          scored[ns++] = (Scored){score, i, j, k};
        }
        else {
          int worst = 0;
          for (int s = 1; s < ns; s++) {
            if (scored[s].score < scored[worst].score) {
              worst = s;
            }
          }
          if (score > scored[worst].score) {
            scored[worst] = (Scored){score, i, j, k};
          }
        }
      }
    }
  }

  for (int i = 0; i < ns - 1; i++) {
    for (int j = i + 1; j < ns; j++) {
      if (scored[j].score > scored[i].score) {
        Scored t = scored[i];
        scored[i] = scored[j];
        scored[j] = t;
      }
    }
  }

  int out_count = (ns < max_triples) ? ns : max_triples;
  for (int t = 0; t < out_count; t++) {
    FinderPattern triple[3] =
      {fps[scored[t].i], fps[scored[t].j], fps[scored[t].k]};
    float d01 = fp_dist(triple[0], triple[1]);
    float d02 = fp_dist(triple[0], triple[2]);
    float d12 = fp_dist(triple[1], triple[2]);
    FinderPattern a, b, c;
    if (d01 >= d02 && d01 >= d12) {
      a = triple[2];
      b = triple[0];
      c = triple[1];
    }
    else if (d02 >= d01 && d02 >= d12) {
      a = triple[1];
      b = triple[0];
      c = triple[2];
    }
    else {
      a = triple[0];
      b = triple[1];
      c = triple[2];
    }
    identify_triple(a, b, c, &triples[t][0], &triples[t][1], &triples[t][2]);
  }

  free(scored);
  return out_count;
}

/* ---- Homography (perspective) ---- */

/* Solve an 8x8 linear system for a perspective transform that maps 4 source
   points to 4 destination points. Returns 1 on success, 0 if singular. */
static int compute_homography(
  const double src[4][2],
  const double dst[4][2],
  double H[9]
) {
  double A[8][9];
  memset(A, 0, sizeof(A));
  for (int i = 0; i < 4; i++) {
    double sx = src[i][0], sy = src[i][1];
    double dx = dst[i][0], dy = dst[i][1];
    A[2 * i][0] = sx;
    A[2 * i][1] = sy;
    A[2 * i][2] = 1.0;
    A[2 * i][6] = -sx * dx;
    A[2 * i][7] = -sy * dx;
    A[2 * i][8] = dx;
    A[2 * i + 1][3] = sx;
    A[2 * i + 1][4] = sy;
    A[2 * i + 1][5] = 1.0;
    A[2 * i + 1][6] = -sx * dy;
    A[2 * i + 1][7] = -sy * dy;
    A[2 * i + 1][8] = dy;
  }
  for (int i = 0; i < 8; i++) {
    int max_row = i;
    double max_val = fabs(A[i][i]);
    for (int k = i + 1; k < 8; k++) {
      if (fabs(A[k][i]) > max_val) {
        max_val = fabs(A[k][i]);
        max_row = k;
      }
    }
    if (max_val < 1e-10) {
      return 0;
    }
    if (max_row != i) {
      for (int j = 0; j <= 8; j++) {
        double t = A[i][j];
        A[i][j] = A[max_row][j];
        A[max_row][j] = t;
      }
    }
    for (int j = i + 1; j < 8; j++) {
      double f = A[j][i] / A[i][i];
      for (int k = i; k <= 8; k++) {
        A[j][k] -= f * A[i][k];
      }
    }
  }
  for (int i = 7; i >= 0; i--) {
    if (fabs(A[i][i]) < 1e-10) {
      return 0;
    }
    double s = A[i][8];
    for (int j = i + 1; j < 8; j++) {
      s -= A[i][j] * H[j];
    }
    H[i] = s / A[i][i];
    if (isnan(H[i]) || isinf(H[i])) {
      return 0;
    }
  }
  H[8] = 1.0;
  return 1;
}

static void apply_homography(
  const double H[9],
  double mx,
  double my,
  float *out_x,
  float *out_y
) {
  double w = H[6] * mx + H[7] * my + H[8];
  if (fabs(w) < 1e-10) {
    *out_x = 0;
    *out_y = 0;
    return;
  }
  *out_x = (float)((H[0] * mx + H[1] * my + H[2]) / w);
  *out_y = (float)((H[3] * mx + H[4] * my + H[5]) / w);
}

/* Solve the linear system Mh = v where M is 8x8 symmetric (A^T A) and v is
   8x1 (A^T b), by Gaussian elimination with partial pivoting. Writes the
   result into the first 8 slots of H and sets H[8] = 1. Returns 1 on
   success, 0 on singular / ill-conditioned input. */
static int solve_8x8(double M[8][8], const double v[8], double H[9]) {
  double A[8][9];
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      A[i][j] = M[i][j];
    }
    A[i][8] = v[i];
  }
  for (int i = 0; i < 8; i++) {
    int max_row = i;
    double max_val = fabs(A[i][i]);
    for (int k = i + 1; k < 8; k++) {
      if (fabs(A[k][i]) > max_val) {
        max_val = fabs(A[k][i]);
        max_row = k;
      }
    }
    if (max_val < 1e-12) {
      return 0;
    }
    if (max_row != i) {
      for (int j = 0; j <= 8; j++) {
        double t = A[i][j];
        A[i][j] = A[max_row][j];
        A[max_row][j] = t;
      }
    }
    for (int k = i + 1; k < 8; k++) {
      double f = A[k][i] / A[i][i];
      for (int j = i; j <= 8; j++) {
        A[k][j] -= f * A[i][j];
      }
    }
  }
  for (int i = 7; i >= 0; i--) {
    if (fabs(A[i][i]) < 1e-12) {
      return 0;
    }
    double s = A[i][8];
    for (int j = i + 1; j < 8; j++) {
      s -= A[i][j] * H[j];
    }
    H[i] = s / A[i][i];
    if (isnan(H[i]) || isinf(H[i])) {
      return 0;
    }
  }
  H[8] = 1.0;
  return 1;
}

/* Solve an over-determined homography system from N≥4 correspondences via
   normal equations: the 2N×8 constraint matrix A is folded into an 8×8
   symmetric M = A^T A and an 8×1 vector v = A^T b, and the 8×8 system is
   solved directly. No Hartley normalisation — QR decoding stays within a
   few hundred pixels so the coordinate magnitudes don't blow out double
   precision here. Returns 1 on success, 0 on singular input. */
static int compute_homography_ls(
  const double src[][2],
  const double dst[][2],
  int n,
  double H[9]
) {
  if (n < 4) {
    return 0;
  }
  double M[8][8];
  double v[8];
  memset(M, 0, sizeof(M));
  memset(v, 0, sizeof(v));

  for (int i = 0; i < n; i++) {
    double X = src[i][0], Y = src[i][1];
    double x = dst[i][0], y = dst[i][1];

    /* x-equation row: [X, Y, 1, 0, 0, 0, -x*X, -x*Y] with RHS = x */
    double rx[8] = {X, Y, 1, 0, 0, 0, -x * X, -x * Y};
    for (int a = 0; a < 8; a++) {
      for (int b = 0; b < 8; b++) {
        M[a][b] += rx[a] * rx[b];
      }
      v[a] += rx[a] * x;
    }

    /* y-equation row: [0, 0, 0, X, Y, 1, -y*X, -y*Y] with RHS = y */
    double ry[8] = {0, 0, 0, X, Y, 1, -y * X, -y * Y};
    for (int a = 0; a < 8; a++) {
      for (int b = 0; b < 8; b++) {
        M[a][b] += ry[a] * ry[b];
      }
      v[a] += ry[a] * y;
    }
  }

  return solve_8x8(M, v, H);
}

/* Solve an NxN linear system M x = v by Gaussian elimination with partial
   pivoting. M is destroyed. Result written into x. Returns 1 on success,
   0 on singular matrix. */
static int solve_nxn(double *M, double *v, int n, double *x) {
  /* Augment [M | v] into A: A is n × (n+1), row-major. */
  int stride = n + 1;
  double *A = (double *)malloc((size_t)n * (size_t)stride * sizeof(double));
  if (!A) {
    return 0;
  }
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      A[i * stride + j] = M[i * n + j];
    }
    A[i * stride + n] = v[i];
  }
  for (int i = 0; i < n; i++) {
    int max_row = i;
    double max_val = fabs(A[i * stride + i]);
    for (int k = i + 1; k < n; k++) {
      double av = fabs(A[k * stride + i]);
      if (av > max_val) {
        max_val = av;
        max_row = k;
      }
    }
    if (max_val < 1e-12) {
      free(A);
      return 0;
    }
    if (max_row != i) {
      for (int j = 0; j <= n; j++) {
        double t = A[i * stride + j];
        A[i * stride + j] = A[max_row * stride + j];
        A[max_row * stride + j] = t;
      }
    }
    for (int k = i + 1; k < n; k++) {
      double f = A[k * stride + i] / A[i * stride + i];
      for (int j = i; j <= n; j++) {
        A[k * stride + j] -= f * A[i * stride + j];
      }
    }
  }
  for (int i = n - 1; i >= 0; i--) {
    if (fabs(A[i * stride + i]) < 1e-12) {
      free(A);
      return 0;
    }
    double s = A[i * stride + n];
    for (int j = i + 1; j < n; j++) {
      s -= A[i * stride + j] * x[j];
    }
    x[i] = s / A[i * stride + i];
    if (isnan(x[i]) || isinf(x[i])) {
      free(A);
      return 0;
    }
  }
  free(A);
  return 1;
}

/* Fit a quadratic warp (u, v) -> (x, y) using N correspondences. The warp
   is a bivariate polynomial of degree 2 per output coordinate, i.e.
       x(u,v) = a0 + a1·u + a2·v + a3·u·v + a4·u² + a5·v²
       y(u,v) = b0 + b1·u + b2·v + b3·u·v + b4·u² + b5·v²
   — 12 parameters fit via independent 6-unknown normal equations per
   coordinate. Needs ≥ 6 observations and they must span a 2-D region
   (all points on one line gives a singular matrix). Writes 12 floats
   into coeffs: indices 0..5 are the x-coefficients in the order above,
   6..11 are the y-coefficients. Returns 1 on success, 0 on failure.

   A quadratic warp can model paper curl and moderate lens/fisheye
   distortion that a single 8-parameter homography can't. It's not
   perspective-correct for truly flat surfaces, but on real-world
   photographs of QR codes on non-planar surfaces (receipts, folded
   paper, curved displays) it typically gives sub-pixel accuracy where
   the homography drifts by 1–2 modules across the data area. */
static int fit_quadratic_warp(
  const double src[][2],
  const double dst[][2],
  int n,
  double coeffs[12]
) {
  if (n < 6) {
    return 0;
  }
  double Mx[36], vx[6];
  double My[36], vy[6];
  memset(Mx, 0, sizeof(Mx));
  memset(My, 0, sizeof(My));
  memset(vx, 0, sizeof(vx));
  memset(vy, 0, sizeof(vy));
  for (int i = 0; i < n; i++) {
    double u = src[i][0], v = src[i][1];
    double x = dst[i][0], y = dst[i][1];
    double row[6] = {1.0, u, v, u * v, u * u, v * v};
    for (int a = 0; a < 6; a++) {
      for (int b = 0; b < 6; b++) {
        Mx[a * 6 + b] += row[a] * row[b];
        My[a * 6 + b] += row[a] * row[b];
      }
      vx[a] += row[a] * x;
      vy[a] += row[a] * y;
    }
  }
  double cx[6] = {0}, cy[6] = {0};
  if (!solve_nxn(Mx, vx, 6, cx) || !solve_nxn(My, vy, 6, cy)) {
    return 0;
  }
  for (int i = 0; i < 6; i++) {
    coeffs[i] = cx[i];
    coeffs[i + 6] = cy[i];
  }
  return 1;
}

/* Apply a quadratic warp to map a (u, v) module-space point to a (px, py)
   pixel-space point. Matches the coefficient layout of fit_quadratic_warp. */
static void apply_quadratic_warp(
  const double coeffs[12],
  double u,
  double v,
  double *px,
  double *py
) {
  *px = coeffs[0] + coeffs[1] * u + coeffs[2] * v + coeffs[3] * u * v +
        coeffs[4] * u * u + coeffs[5] * v * v;
  *py = coeffs[6] + coeffs[7] * u + coeffs[8] * v + coeffs[9] * u * v +
        coeffs[10] * u * u + coeffs[11] * v * v;
}

/* Light-pixel analogue of find_dark_centroid_local — centroid of *light*
   pixels in a local window. Used for known-light QR cells (e.g. the light
   timing modules, the light 3x3 interior of the finder pattern) to add
   anchor points between the dark modules the primary observer finds. */
static int find_light_centroid_local(
  uint8_t const *gray,
  int w,
  int h,
  double px_pred,
  double py_pred,
  float search_r,
  double *px_obs,
  double *py_obs
) {
  int r = (int)(search_r + 0.5f);
  if (r < 2) {
    r = 2;
  }
  int cx = (int)(px_pred + 0.5);
  int cy = (int)(py_pred + 0.5);
  int x0 = cx - r;
  int y0 = cy - r;
  int x1 = cx + r;
  int y1 = cy + r;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 >= w) {
    x1 = w - 1;
  }
  if (y1 >= h) {
    y1 = h - 1;
  }
  if (x1 - x0 < 2 || y1 - y0 < 2) {
    return 0;
  }
  uint8_t lo = 255, hi = 0;
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      uint8_t g = gray[y * w + x];
      if (g < lo) {
        lo = g;
      }
      if (g > hi) {
        hi = g;
      }
    }
  }
  if (hi - lo < 20) {
    return 0;
  }
  int t = (lo + hi) / 2;
  double sx = 0, sy = 0, sw = 0;
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      uint8_t g = gray[y * w + x];
      if (g >= t) {
        double weight = (double)(g - t);
        sx += (double)x * weight;
        sy += (double)y * weight;
        sw += weight;
      }
    }
  }
  if (sw <= 0) {
    return 0;
  }
  *px_obs = sx / sw;
  *py_obs = sy / sw;
  return 1;
}

/* Observe the pixel-space centroid of dark pixels in a local window around
   a predicted position, using an adaptive local threshold. Used to ground-
   truth the position of a known-dark QR module (timing pattern dark
   module, alignment center, etc.) against the pixel buffer so we can
   refine a homography that was built from far-apart control points.
   Returns 1 on success; 0 if the window is uniform (low contrast ⇒
   unreliable). */
static int find_dark_centroid_local(
  uint8_t const *gray,
  int w,
  int h,
  double px_pred,
  double py_pred,
  float search_r,
  double *px_obs,
  double *py_obs
) {
  int r = (int)(search_r + 0.5f);
  if (r < 2) {
    r = 2;
  }
  int cx = (int)(px_pred + 0.5);
  int cy = (int)(py_pred + 0.5);
  int x0 = cx - r;
  int y0 = cy - r;
  int x1 = cx + r;
  int y1 = cy + r;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 >= w) {
    x1 = w - 1;
  }
  if (y1 >= h) {
    y1 = h - 1;
  }
  if (x1 - x0 < 2 || y1 - y0 < 2) {
    return 0;
  }
  uint8_t lo = 255, hi = 0;
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      uint8_t g = gray[y * w + x];
      if (g < lo) {
        lo = g;
      }
      if (g > hi) {
        hi = g;
      }
    }
  }
  if (hi - lo < 20) {
    return 0;
  }
  int t = (lo + hi) / 2;
  double sx = 0, sy = 0, sw = 0;
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      uint8_t g = gray[y * w + x];
      if (g < t) {
        double weight = (double)(t - g);
        sx += (double)x * weight;
        sy += (double)y * weight;
        sw += weight;
      }
    }
  }
  if (sw <= 0) {
    return 0;
  }
  *px_obs = sx / sw;
  *py_obs = sy / sw;
  return 1;
}

/* Forward declarations — defined later alongside the homography decode path. */
static char *decode_grid(
  uint8_t *grid,
  uint8_t const *conf,
  int qr_size,
  int version,
  int *out_fmt_dist,
  int *out_rs_cost
);
static float timing_error_rate(uint8_t const *grid, int qr_size);
static float finder_error_rate(uint8_t const *grid, int qr_size);

/* Sample the QR module grid using a quadratic warp instead of a homography.
   Behaves like sample_grid_homography_offset_mode's non-bilinear path but
   with a polynomial mapping that can bend to follow paper curl. The 5-
   point sub-sample voting is preserved for tolerance against small
   within-module drift. Returns a heap buffer of size qr_size*qr_size;
   caller frees. */
static uint8_t *sample_grid_quadratic(
  uint8_t const *px,
  int w,
  int h,
  const double coeffs[12],
  int qr_size
) {
  int total = qr_size * qr_size;
  uint8_t *grid = calloc(total, 1);
  if (!grid) {
    return NULL;
  }
  static const double offs[5][2] = {
    {0.5, 0.5},
    {0.35, 0.35},
    {0.65, 0.35},
    {0.35, 0.65},
    {0.65, 0.65},
  };
  for (int r = 0; r < qr_size; r++) {
    for (int c = 0; c < qr_size; c++) {
      int votes = 0;
      for (int o = 0; o < 5; o++) {
        double px_d, py_d;
        apply_quadratic_warp(
          coeffs,
          c + offs[o][0],
          r + offs[o][1],
          &px_d,
          &py_d
        );
        int ix = (int)(px_d + 0.5);
        int iy = (int)(py_d + 0.5);
        if (is_dark(px, w, h, ix, iy)) {
          votes++;
        }
      }
      grid[r * qr_size + c] = (votes >= 3) ? 1 : 0;
    }
  }
  return grid;
}

/* Fit a quadratic warp from 3 finder centers + alignment + all observable
   timing-module centroids, then sample the grid with it and try to
   decode. Returns a heap-allocated decoded string (caller frees) or
   NULL if sampling/decode fails. */
static char *try_decode_quadratic(
  uint8_t const *bin,
  uint8_t const *gray,
  int w,
  int h,
  int qr_size,
  int version,
  FinderPattern tl,
  FinderPattern tr,
  FinderPattern bl,
  int have_alignment,
  double alignment_x,
  double alignment_y,
  const double H_init[9],
  float ms,
  int *out_fmt_dist,
  int *out_rs_cost
) {
  enum { MAX_CORR = 80 };
  double src[MAX_CORR][2];
  double dst[MAX_CORR][2];
  int n = 0;
  src[n][0] = 3.5;
  src[n][1] = 3.5;
  dst[n][0] = tl.x;
  dst[n][1] = tl.y;
  n++;
  src[n][0] = qr_size - 3.5;
  src[n][1] = 3.5;
  dst[n][0] = tr.x;
  dst[n][1] = tr.y;
  n++;
  src[n][0] = 3.5;
  src[n][1] = qr_size - 3.5;
  dst[n][0] = bl.x;
  dst[n][1] = bl.y;
  n++;
  if (have_alignment) {
    src[n][0] = qr_size - 6.5;
    src[n][1] = qr_size - 6.5;
    dst[n][0] = alignment_x;
    dst[n][1] = alignment_y;
    n++;
  }
  float search_r = ms * 0.5f;
  /* Horizontal timing pattern: dark at even cols, light at odd cols. Sample
     both so the quadratic warp has fine-grained constraints along row 6. */
  for (int col = 7; col <= qr_size - 8; col++) {
    if (n >= MAX_CORR) {
      break;
    }
    double mx = (double)col + 0.5;
    double my = 6.5;
    double pw = H_init[6] * mx + H_init[7] * my + H_init[8];
    if (fabs(pw) < 1e-10) {
      continue;
    }
    double px_pred = (H_init[0] * mx + H_init[1] * my + H_init[2]) / pw;
    double py_pred = (H_init[3] * mx + H_init[4] * my + H_init[5]) / pw;
    double px_obs, py_obs;
    int ok_c;
    if ((col % 2) == 0) {
      ok_c = find_dark_centroid_local(
        gray,
        w,
        h,
        px_pred,
        py_pred,
        search_r,
        &px_obs,
        &py_obs
      );
    }
    else {
      ok_c = find_light_centroid_local(
        gray,
        w,
        h,
        px_pred,
        py_pred,
        search_r,
        &px_obs,
        &py_obs
      );
    }
    if (ok_c) {
      src[n][0] = mx;
      src[n][1] = my;
      dst[n][0] = px_obs;
      dst[n][1] = py_obs;
      n++;
    }
  }
  for (int row = 7; row <= qr_size - 8; row++) {
    if (n >= MAX_CORR) {
      break;
    }
    double mx = 6.5;
    double my = (double)row + 0.5;
    double pw = H_init[6] * mx + H_init[7] * my + H_init[8];
    if (fabs(pw) < 1e-10) {
      continue;
    }
    double px_pred = (H_init[0] * mx + H_init[1] * my + H_init[2]) / pw;
    double py_pred = (H_init[3] * mx + H_init[4] * my + H_init[5]) / pw;
    double px_obs, py_obs;
    int ok_c;
    if ((row % 2) == 0) {
      ok_c = find_dark_centroid_local(
        gray,
        w,
        h,
        px_pred,
        py_pred,
        search_r,
        &px_obs,
        &py_obs
      );
    }
    else {
      ok_c = find_light_centroid_local(
        gray,
        w,
        h,
        px_pred,
        py_pred,
        search_r,
        &px_obs,
        &py_obs
      );
    }
    if (ok_c) {
      src[n][0] = mx;
      src[n][1] = my;
      dst[n][0] = px_obs;
      dst[n][1] = py_obs;
      n++;
    }
  }
  /* Alignment pattern anchors: sample the 8 known-light inner-ring cells
     around the BR alignment center to pin the quadratic warp's BR quadrant
     — without this the warp is only constrained by the single alignment
     center in that region and extrapolates wildly into the data area. */
  if (have_alignment) {
    double ax = (double)qr_size - 6.5; /* alignment center in module coords */
    double ay = (double)qr_size - 6.5;
    static const double ring1[8][2] = {
      {-1.0, -1.0},
      {0.0, -1.0},
      {1.0, -1.0},
      {-1.0, 0.0},
      {1.0, 0.0},
      {-1.0, 1.0},
      {0.0, 1.0},
      {1.0, 1.0},
    };
    for (int i = 0; i < 8; i++) {
      if (n >= MAX_CORR) {
        break;
      }
      double mx = ax + ring1[i][0];
      double my = ay + ring1[i][1];
      double pw = H_init[6] * mx + H_init[7] * my + H_init[8];
      if (fabs(pw) < 1e-10) {
        continue;
      }
      double px_pred = (H_init[0] * mx + H_init[1] * my + H_init[2]) / pw;
      double py_pred = (H_init[3] * mx + H_init[4] * my + H_init[5]) / pw;
      double px_obs, py_obs;
      if (find_light_centroid_local(
            gray,
            w,
            h,
            px_pred,
            py_pred,
            search_r,
            &px_obs,
            &py_obs
          )) {
        src[n][0] = mx;
        src[n][1] = my;
        dst[n][0] = px_obs;
        dst[n][1] = py_obs;
        n++;
      }
    }
  }
  /* Need at least 8 well-spread observations for a stable quadratic fit
     (6 unknowns per axis + a bit of over-determination). */
  if (n < 8) {
    return NULL;
  }
  double coeffs[12];
  if (!fit_quadratic_warp(src, dst, n, coeffs)) {
    return NULL;
  }
  uint8_t *grid = sample_grid_quadratic(bin, w, h, coeffs, qr_size);
  if (!grid) {
    return NULL;
  }
  int fd = 16, rc = 100000;
  char *d = decode_grid(grid, NULL, qr_size, version, &fd, &rc);
  free(grid);
  if (d) {
    if (out_fmt_dist) {
      *out_fmt_dist = fd;
    }
    if (out_rs_cost) {
      *out_rs_cost = rc;
    }
  }
  return d;
}

/* Refine a homography by observing pixel-space centroids of every dark
   module in the two QR timing patterns and solving a least-squares fit
   over the combined set of control points and timing observations. The
   3 finder centers (and optional BR alignment) give the coarse shape;
   the timing observations, which thread the QR's interior, constrain
   the transform through the data area and correct the sub-module drift
   that a 4-point homography can't resolve on curled or lens-distorted
   captures (e.g. a receipt photographed at an angle). Returns 1 if the
   refined H is valid and used ≥ 6 points total. Leaves H_out untouched
   on failure. */
static int refine_h_with_timing(
  uint8_t const *gray,
  int w,
  int h,
  const double H_init[9],
  int qr_size,
  float ms,
  FinderPattern tl,
  FinderPattern tr,
  FinderPattern bl,
  int have_alignment,
  double alignment_x,
  double alignment_y,
  double H_out[9]
) {
  if (!gray || qr_size < 21) {
    return 0;
  }
  enum { MAX_CORR = 80 };
  double src[MAX_CORR][2];
  double dst[MAX_CORR][2];
  int n = 0;

  /* 3 finder centers as anchoring control points. */
  src[n][0] = 3.5;
  src[n][1] = 3.5;
  dst[n][0] = tl.x;
  dst[n][1] = tl.y;
  n++;
  src[n][0] = qr_size - 3.5;
  src[n][1] = 3.5;
  dst[n][0] = tr.x;
  dst[n][1] = tr.y;
  n++;
  src[n][0] = 3.5;
  src[n][1] = qr_size - 3.5;
  dst[n][0] = bl.x;
  dst[n][1] = bl.y;
  n++;

  /* Optional alignment-pattern center for the BR corner. */
  if (have_alignment) {
    src[n][0] = qr_size - 6.5;
    src[n][1] = qr_size - 6.5;
    dst[n][0] = alignment_x;
    dst[n][1] = alignment_y;
    n++;
  }

  /* Timing pattern: module row 6 columns 8..qr_size-9, dark at even cols;
     module col 6 rows 8..qr_size-9, dark at even rows. Sample the dark
     modules' pixel-space centroids using the initial H as a guide and a
     half-module search radius — the initial guess is within a module's
     width of truth on realistic captures. */
  float search_r = ms * 0.5f;
  for (int col = 8; col <= qr_size - 9; col += 2) {
    if (n >= MAX_CORR) {
      break;
    }
    double mx = (double)col + 0.5;
    double my = 6.5;
    double pw = H_init[6] * mx + H_init[7] * my + H_init[8];
    if (fabs(pw) < 1e-10) {
      continue;
    }
    double px_pred = (H_init[0] * mx + H_init[1] * my + H_init[2]) / pw;
    double py_pred = (H_init[3] * mx + H_init[4] * my + H_init[5]) / pw;
    double px_obs, py_obs;
    if (find_dark_centroid_local(
          gray,
          w,
          h,
          px_pred,
          py_pred,
          search_r,
          &px_obs,
          &py_obs
        )) {
      src[n][0] = mx;
      src[n][1] = my;
      dst[n][0] = px_obs;
      dst[n][1] = py_obs;
      n++;
    }
  }
  for (int row = 8; row <= qr_size - 9; row += 2) {
    if (n >= MAX_CORR) {
      break;
    }
    double mx = 6.5;
    double my = (double)row + 0.5;
    double pw = H_init[6] * mx + H_init[7] * my + H_init[8];
    if (fabs(pw) < 1e-10) {
      continue;
    }
    double px_pred = (H_init[0] * mx + H_init[1] * my + H_init[2]) / pw;
    double py_pred = (H_init[3] * mx + H_init[4] * my + H_init[5]) / pw;
    double px_obs, py_obs;
    if (find_dark_centroid_local(
          gray,
          w,
          h,
          px_pred,
          py_pred,
          search_r,
          &px_obs,
          &py_obs
        )) {
      src[n][0] = mx;
      src[n][1] = my;
      dst[n][0] = px_obs;
      dst[n][1] = py_obs;
      n++;
    }
  }

  /* Need meaningfully more than the 4-point minimum for the least-squares
     to actually do anything; if only the control points were observed
     (all timing centroid calls failed — likely a low-contrast or obscured
     QR) the refined H would equal the initial one. */
  if (n < 6) {
    return 0;
  }
  return compute_homography_ls(src, dst, n, H_out);
}

/* ---- Alignment pattern search ---- */

/* Check if (cx, cy) looks like the center of a QR alignment pattern:
   dark center with light ring 1 module out and dark ring 2 modules out.
   Samples along 8 radial directions at 1 and 2 module offsets, and
   requires at least 6 of 8 each to match. */
static int
is_alignment_center(uint8_t const *px, int w, int h, int cx, int cy, float ms) {
  if (cx < 0 || cx >= w || cy < 0 || cy >= h) {
    return 0;
  }
  if (!is_dark(px, w, h, cx, cy)) {
    return 0;
  }
  /* 8 radial directions */
  static const float dirs[8][2] = {
    {1.0f, 0.0f},
    {0.707f, 0.707f},
    {0.0f, 1.0f},
    {-0.707f, 0.707f},
    {-1.0f, 0.0f},
    {-0.707f, -0.707f},
    {0.0f, -1.0f},
    {0.707f, -0.707f},
  };
  int light_ok = 0;
  int dark_ring_ok = 0;
  int beyond_light_ok = 0;
  int beyond_checked = 0;
  for (int d = 0; d < 8; d++) {
    int lx = (int)(cx + ms * dirs[d][0] + 0.5f * (dirs[d][0] > 0 ? 1 : -1));
    int ly = (int)(cy + ms * dirs[d][1] + 0.5f * (dirs[d][1] > 0 ? 1 : -1));
    int rx =
      (int)(cx + 2.0f * ms * dirs[d][0] + 0.5f * (dirs[d][0] > 0 ? 1 : -1));
    int ry =
      (int)(cy + 2.0f * ms * dirs[d][1] + 0.5f * (dirs[d][1] > 0 ? 1 : -1));
    /* A real alignment pattern is exactly 5 modules wide: the outer dark
       ring sits at radius 2·ms, so just beyond (2.8·ms) we must be out of
       the pattern again. Data-module clusters that happen to expose a
       dark center + light 1·ms + dark 2·ms often keep going dark past
       radius 2, because they're embedded in a larger dense region —
       require "light past the dark ring" in the majority of directions
       to rule those out. */
    int bx =
      (int)(cx + 2.8f * ms * dirs[d][0] + 0.5f * (dirs[d][0] > 0 ? 1 : -1));
    int by =
      (int)(cy + 2.8f * ms * dirs[d][1] + 0.5f * (dirs[d][1] > 0 ? 1 : -1));
    if (lx < 0 || lx >= w || ly < 0 || ly >= h) {
      continue;
    }
    if (rx < 0 || rx >= w || ry < 0 || ry >= h) {
      continue;
    }
    if (!is_dark(px, w, h, lx, ly)) {
      light_ok++;
    }
    if (is_dark(px, w, h, rx, ry)) {
      dark_ring_ok++;
    }
    if (bx >= 0 && bx < w && by >= 0 && by < h) {
      beyond_checked++;
      if (!is_dark(px, w, h, bx, by)) {
        beyond_light_ok++;
      }
    }
  }
  if (light_ok < 6 || dark_ring_ok < 6) {
    return 0;
  }
  /* Require ≥ 5 of 8 outer directions to be light beyond the dark ring.
     5 (of 8) keeps a small margin for alignment patterns adjacent to a
     single dark data module while still killing patterns embedded in
     dark clusters. If we couldn't check enough directions (pattern near
     image edge), pass through — the existing 2-ring check has to carry. */
  if (beyond_checked >= 5 && beyond_light_ok < 5) {
    return 0;
  }
  return 1;
}

static int find_alignment_pattern(
  uint8_t const *px,
  int w,
  int h,
  float cx_pred,
  float cy_pred,
  float expected_ms,
  float search_radius,
  float *out_cx,
  float *out_cy
) {
  int x_min = (int)(cx_pred - search_radius);
  int x_max = (int)(cx_pred + search_radius);
  int y_min = (int)(cy_pred - search_radius);
  int y_max = (int)(cy_pred + search_radius);
  if (x_min < 0) {
    x_min = 0;
  }
  if (y_min < 0) {
    y_min = 0;
  }
  if (x_max >= w) {
    x_max = w - 1;
  }
  if (y_max >= h) {
    y_max = h - 1;
  }
  if (x_max <= x_min || y_max <= y_min) {
    return 0;
  }
  /* Scan every dark pixel; the one with the smallest distance to the
     predicted center that also passes the template check wins. */
  float best_cx = -1, best_cy = -1;
  float best_dist = 1e30f;
  for (int y = y_min; y <= y_max; y++) {
    for (int x = x_min; x <= x_max; x++) {
      if (!is_dark(px, w, h, x, y)) {
        continue;
      }
      if (!is_alignment_center(px, w, h, x, y, expected_ms)) {
        continue;
      }
      float dx = (float)x - cx_pred;
      float dy = (float)y - cy_pred;
      float dist = dx * dx + dy * dy;
      if (dist < best_dist) {
        best_dist = dist;
        best_cx = (float)x;
        best_cy = (float)y;
      }
    }
  }
  if (best_cx < 0) {
    return 0;
  }
  /* Refine by averaging all near-center dark pixels (the center module is
     ~1 module wide, so there will be a cluster of passing pixels). */
  int r_refine = (int)(expected_ms * 0.5f);
  if (r_refine < 1) {
    r_refine = 1;
  }
  double sum_x = 0, sum_y = 0;
  int count = 0;
  for (int dy = -r_refine; dy <= r_refine; dy++) {
    for (int dx = -r_refine; dx <= r_refine; dx++) {
      int x = (int)best_cx + dx;
      int y = (int)best_cy + dy;
      if (x < 0 || x >= w || y < 0 || y >= h) {
        continue;
      }
      if (is_dark(px, w, h, x, y) &&
          is_alignment_center(px, w, h, x, y, expected_ms)) {
        sum_x += x;
        sum_y += y;
        count++;
      }
    }
  }
  if (count > 0) {
    *out_cx = (float)(sum_x / count);
    *out_cy = (float)(sum_y / count);
  }
  else {
    *out_cx = best_cx;
    *out_cy = best_cy;
  }
  return 1;
}

/* ---- Grid sampling ----
   Homography-only: uses a 4-point perspective transform and samples a small
   cluster of pixels near each module center, taking the majority value to
   tolerate minor grid misalignment. The affine-equivalent case (no
   perspective) is expressible as a homography where the 4th point is the
   parallelogram completion, so a separate affine path isn't needed. */
/* Bilinear sample of the binary image at fractional pixel (fx, fy). Returns
   the 0..255 weighted blend of the four neighbouring pixels' dark-ness. */
static int bilinear_dark(uint8_t const *px, int w, int h, float fx, float fy) {
  int x0 = (int)floorf(fx);
  int y0 = (int)floorf(fy);
  int x1 = x0 + 1;
  int y1 = y0 + 1;
  float dx = fx - x0;
  float dy = fy - y0;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 < 0) {
    x1 = 0;
  }
  if (y1 < 0) {
    y1 = 0;
  }
  if (x0 >= w) {
    x0 = w - 1;
  }
  if (y0 >= h) {
    y0 = h - 1;
  }
  if (x1 >= w) {
    x1 = w - 1;
  }
  if (y1 >= h) {
    y1 = h - 1;
  }
  int v00 = px[y0 * w + x0] < 128 ? 1 : 0;
  int v01 = px[y0 * w + x1] < 128 ? 1 : 0;
  int v10 = px[y1 * w + x0] < 128 ? 1 : 0;
  int v11 = px[y1 * w + x1] < 128 ? 1 : 0;
  float v0 = v00 * (1.0f - dx) + v01 * dx;
  float v1 = v10 * (1.0f - dx) + v11 * dx;
  return (v0 * (1.0f - dy) + v1 * dy) >= 0.5f;
}

/* Sample a single grayscale value via bilinear interpolation, with edge
   clamping. Returns 0 if input gray is NULL (signals "no gray available"). */
static int
sample_gray_bilinear(uint8_t const *gray, int w, int h, float fx, float fy) {
  if (!gray) {
    return -1;
  }
  int x0 = (int)floorf(fx);
  int y0 = (int)floorf(fy);
  int x1 = x0 + 1;
  int y1 = y0 + 1;
  float dx = fx - x0;
  float dy = fy - y0;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 < 0) {
    x1 = 0;
  }
  if (y1 < 0) {
    y1 = 0;
  }
  if (x0 >= w) {
    x0 = w - 1;
  }
  if (y0 >= h) {
    y0 = h - 1;
  }
  if (x1 >= w) {
    x1 = w - 1;
  }
  if (y1 >= h) {
    y1 = h - 1;
  }
  float v00 = gray[y0 * w + x0];
  float v01 = gray[y0 * w + x1];
  float v10 = gray[y1 * w + x0];
  float v11 = gray[y1 * w + x1];
  float v0 = v00 * (1.0f - dx) + v01 * dx;
  float v1 = v10 * (1.0f - dx) + v11 * dx;
  float v = v0 * (1.0f - dy) + v1 * dy;
  if (v < 0.0f) {
    v = 0.0f;
  }
  if (v > 255.0f) {
    v = 255.0f;
  }
  return (int)(v + 0.5f);
}

/* Sample the QR module grid given a homography. If `gray_for_conf` is
   non-NULL and `out_conf` is non-NULL, also writes a per-module ambiguity
   buffer (1 = low-confidence module, 0 = high-confidence) of the same size
   as the grid. The caller is responsible for free()ing both buffers.

   Confidence is derived adaptively: after sampling all modules and their
   per-module mean grayscale, compute the median grayscale of the modules
   classified as light vs dark. A module is flagged ambiguous when its
   mean gray sits a long way from BOTH expected values (signals an
   occlusion patch whose color belongs to neither QR cluster) or when the
   5 sub-sample votes were a near-tie (signals the patch boundary crosses
   the module's sample area). */
static uint8_t *sample_grid_homography_offset_mode(
  uint8_t const *px,
  uint8_t const *gray_for_conf,
  int w,
  int h,
  const double H[9],
  int qr_size,
  double off_x,
  double off_y,
  int use_bilinear,
  uint8_t **out_conf
) {
  int total = qr_size * qr_size;
  uint8_t *grid = calloc(total, 1);
  if (!grid) {
    return NULL;
  }
  uint8_t *conf = NULL;
  uint8_t *gmean_buf = NULL;
  uint8_t *vote_tie = NULL;
  if (out_conf && gray_for_conf) {
    conf = calloc(total, 1);
    gmean_buf = malloc(total);
    vote_tie = calloc(total, 1);
    if (!conf || !gmean_buf || !vote_tie) {
      free(grid);
      free(conf);
      free(gmean_buf);
      free(vote_tie);
      return NULL;
    }
  }
  static const double offs[5][2] = {
    {0.5, 0.5},
    {0.35, 0.35},
    {0.65, 0.35},
    {0.35, 0.65},
    {0.65, 0.65},
  };
  for (int r = 0; r < qr_size; r++) {
    for (int c = 0; c < qr_size; c++) {
      int votes = 0;
      int gray_sum = 0;
      int gray_n = 0;
      for (int o = 0; o < 5; o++) {
        float fx, fy;
        apply_homography(
          H,
          c + offs[o][0] + off_x,
          r + offs[o][1] + off_y,
          &fx,
          &fy
        );
        int dark;
        if (use_bilinear) {
          dark = bilinear_dark(px, w, h, fx, fy);
        }
        else {
          int ix = (int)(fx + 0.5f);
          int iy = (int)(fy + 0.5f);
          dark = is_dark(px, w, h, ix, iy);
        }
        if (dark) {
          votes++;
        }
        if (gmean_buf) {
          int g = sample_gray_bilinear(gray_for_conf, w, h, fx, fy);
          if (g >= 0) {
            gray_sum += g;
            gray_n++;
          }
        }
      }
      grid[r * qr_size + c] = (votes >= 3) ? 1 : 0;
      if (gmean_buf) {
        int gmean = (gray_n > 0) ? (gray_sum / gray_n) : 128;
        if (gmean < 0) {
          gmean = 0;
        }
        if (gmean > 255) {
          gmean = 255;
        }
        gmean_buf[r * qr_size + c] = (uint8_t)gmean;
        if (vote_tie) {
          vote_tie[r * qr_size + c] =
            (uint8_t)((votes == 2 || votes == 3) ? 1 : 0);
        }
      }
    }
  }

  if (conf && gmean_buf) {
    /* Compute medians of dark / light cluster grays via histogram. */
    int hist_dark[256] = {0};
    int hist_light[256] = {0};
    int n_dark = 0, n_light = 0;
    for (int i = 0; i < total; i++) {
      if (grid[i]) {
        hist_dark[gmean_buf[i]]++;
        n_dark++;
      }
      else {
        hist_light[gmean_buf[i]]++;
        n_light++;
      }
    }
    int dark_med = 0, light_med = 255;
    if (n_dark > 0) {
      int target = n_dark / 2;
      int acc = 0;
      for (int v = 0; v < 256; v++) {
        acc += hist_dark[v];
        if (acc > target) {
          dark_med = v;
          break;
        }
      }
    }
    if (n_light > 0) {
      int target = n_light / 2;
      int acc = 0;
      for (int v = 0; v < 256; v++) {
        acc += hist_light[v];
        if (acc > target) {
          light_med = v;
          break;
        }
      }
    }
    /* Tolerance: a chunky fraction of the cluster gap. A module is flagged
       only when its mean gray is FAR from BOTH the dark and the light
       cluster medians — that is the strong signal that the pixel belongs
       to a foreign region (occlusion patch). We also keep a floor so the
       rule still bites when the clusters are weakly separated. */
    int gap = light_med - dark_med;
    if (gap < 0) {
      gap = -gap;
    }
    int tol = gap / 3;
    if (tol < 35) {
      tol = 35;
    }
    if (tol > 80) {
      tol = 80;
    }
    for (int i = 0; i < total; i++) {
      int g = gmean_buf[i];
      int dd = g - dark_med;
      if (dd < 0) {
        dd = -dd;
      }
      int dl = g - light_med;
      if (dl < 0) {
        dl = -dl;
      }
      int ambig = 0;
      if (dd > tol && dl > tol) {
        ambig = 1;
      }
      conf[i] = (uint8_t)ambig;
    }
  }

  free(gmean_buf);
  free(vote_tie);
  if (out_conf) {
    *out_conf = conf;
  }
  return grid;
}

/* ---- Timing pattern check ----
   The timing patterns on row 6 and column 6 alternate dark/light between the
   three finder patterns. Starting at module (6, 8) = dark, (6, 9) = light,
   (6, 10) = dark, … — dark whenever col or row is even. Real QRs have a
   near-perfect match; a grid sampled from three incidental 1:1:3:1:1 runs in
   noise produces essentially random bits here (~50% error). Returns the
   fraction of timing modules whose value disagrees with the spec, 0.0 for a
   perfect match and up to 1.0 for all wrong. */
static float timing_error_rate(uint8_t const *grid, int qr_size) {
  if (qr_size < 15) {
    return 0.0f;
  }
  int errors = 0;
  int total = 0;
  for (int c = 8; c <= qr_size - 9; c++) {
    int expected = ((c % 2) == 0) ? 1 : 0;
    int actual = grid[6 * qr_size + c] ? 1 : 0;
    if (actual != expected) {
      errors++;
    }
    total++;
  }
  for (int r = 8; r <= qr_size - 9; r++) {
    int expected = ((r % 2) == 0) ? 1 : 0;
    int actual = grid[r * qr_size + 6] ? 1 : 0;
    if (actual != expected) {
      errors++;
    }
    total++;
  }
  if (total == 0) {
    return 1.0f;
  }
  return (float)errors / (float)total;
}

/* ---- Finder pattern structural check ----
   After sampling, each of the three 7x7 finder regions should match the
   canonical dark/light pattern (outer ring dark, 1-module light inset, 3x3
   dark core). Plus the 1-module separator (row/col between finder and data)
   should be light. Incidental 1:1:3:1:1 runs in noise almost never project
   into grids whose TL/TR/BL 7x7 windows all match the finder pattern.
   Returns the fraction of the 48 checked cells (16 per finder) that
   disagree — callers reject above ~0.10. */
static const uint8_t FINDER_PATTERN[7][7] = {
  {1, 1, 1, 1, 1, 1, 1},
  {1, 0, 0, 0, 0, 0, 1},
  {1, 0, 1, 1, 1, 0, 1},
  {1, 0, 1, 1, 1, 0, 1},
  {1, 0, 1, 1, 1, 0, 1},
  {1, 0, 0, 0, 0, 0, 1},
  {1, 1, 1, 1, 1, 1, 1},
};

static float finder_error_rate(uint8_t const *grid, int qr_size) {
  if (qr_size < 21) {
    return 1.0f;
  }
  int errors = 0;
  int total = 0;
  /* TL at (0,0); TR at (0, qr_size-7); BL at (qr_size-7, 0). */
  const int origins[3][2] = {
    {0, 0},
    {0, qr_size - 7},
    {qr_size - 7, 0},
  };
  for (int f = 0; f < 3; f++) {
    int r0 = origins[f][0];
    int c0 = origins[f][1];
    for (int dr = 0; dr < 7; dr++) {
      for (int dc = 0; dc < 7; dc++) {
        int expected = FINDER_PATTERN[dr][dc];
        int actual = grid[(r0 + dr) * qr_size + (c0 + dc)] ? 1 : 0;
        if (actual != expected) {
          errors++;
        }
        total++;
      }
    }
  }
  if (total == 0) {
    return 1.0f;
  }
  return (float)errors / (float)total;
}

/* ---- Format information ---- */

static const int FMT_COORDS1[15][2] = {
  {8, 0},
  {8, 1},
  {8, 2},
  {8, 3},
  {8, 4},
  {8, 5},
  {8, 7},
  {8, 8},
  {7, 8},
  {5, 8},
  {4, 8},
  {3, 8},
  {2, 8},
  {1, 8},
  {0, 8},
};

static const uint32_t VALID_FMTS[32] = {
  0x5412, 0x5125, 0x5E7C, 0x5B4B, 0x45F9, 0x40CE, 0x4F97, 0x4AA0,
  0x77C4, 0x72F3, 0x7DAA, 0x789D, 0x662F, 0x6318, 0x6C41, 0x6976,
  0x1689, 0x13BE, 0x1CE7, 0x19D0, 0x0762, 0x0255, 0x0D0C, 0x083B,
  0x355F, 0x3068, 0x3F31, 0x3A06, 0x24B4, 0x2183, 0x2EDA, 0x2BED,
};

static int popcount_u(uint32_t v) {
  int c = 0;
  while (v) {
    c += v & 1;
    v >>= 1;
  }
  return c;
}

static int decode_format(
  uint8_t const *grid,
  int qr_size,
  int *ec_level,
  int *mask_pattern,
  int *out_dist
) {
  uint32_t bits1 = 0;
  for (int i = 0; i < 15; i++) {
    int col = FMT_COORDS1[i][0], row = FMT_COORDS1[i][1];
    if (row < qr_size && col < qr_size && grid[row * qr_size + col]) {
      bits1 |= (1u << i);
    }
  }
  uint32_t bits2 = 0;
  for (int i = 0; i < 7; i++) {
    int row = qr_size - 1 - i;
    if (grid[row * qr_size + 8]) {
      bits2 |= (1u << i);
    }
  }
  for (int i = 0; i < 8; i++) {
    int col = qr_size - 8 + i;
    if (grid[8 * qr_size + col]) {
      bits2 |= (1u << (7 + i));
    }
  }

  int best_d = 16, best_f = -1;
  for (int copy = 0; copy < 2; copy++) {
    uint32_t bits = (copy == 0) ? bits1 : bits2;
    for (int f = 0; f < 32; f++) {
      int d = popcount_u(bits ^ VALID_FMTS[f]);
      if (d < best_d) {
        best_d = d;
        best_f = f;
      }
    }
  }
  if (best_f < 0 || best_d > 3) {
    return -1;
  }

  int ec_ind = best_f >> 3;
  *mask_pattern = best_f & 7;
  static const int ec_map[4] = {1, 0, 3, 2};
  *ec_level = ec_map[ec_ind];
  if (out_dist) {
    *out_dist = best_d;
  }
  return 0;
}

/* ---- Masking ---- */

static int apply_mask(int mask, int row, int col) {
  switch (mask) {
  case 0:
    return (row + col) % 2 == 0;
  case 1:
    return row % 2 == 0;
  case 2:
    return col % 3 == 0;
  case 3:
    return (row + col) % 3 == 0;
  case 4:
    return (row / 2 + col / 3) % 2 == 0;
  case 5:
    return (row * col) % 2 + (row * col) % 3 == 0;
  case 6:
    return ((row * col) % 2 + (row * col) % 3) % 2 == 0;
  case 7:
    return ((row + col) % 2 + (row * col) % 3) % 2 == 0;
  default:
    return 0;
  }
}

/* ---- Function pattern check ---- */

static const int ALIGN_POS[41][8] = {
  {0},
  {0},
  {6, 18, 0},
  {6, 22, 0},
  {6, 26, 0},
  {6, 30, 0},
  {6, 34, 0},
  {6, 22, 38, 0},
  {6, 24, 42, 0},
  {6, 26, 46, 0},
  {6, 28, 50, 0},
  {6, 30, 54, 0},
  {6, 32, 58, 0},
  {6, 34, 62, 0},
  {6, 26, 46, 66, 0},
  {6, 26, 48, 70, 0},
  {6, 26, 50, 74, 0},
  {6, 30, 54, 78, 0},
  {6, 30, 56, 82, 0},
  {6, 30, 58, 86, 0},
  {6, 34, 62, 90, 0},
  {6, 28, 50, 72, 94, 0},
  {6, 26, 50, 74, 98, 0},
  {6, 30, 54, 78, 102, 0},
  {6, 28, 54, 80, 106, 0},
  {6, 32, 58, 84, 110, 0},
  {6, 30, 58, 86, 114, 0},
  {6, 34, 62, 90, 118, 0},
  {6, 26, 50, 74, 98, 122, 0},
  {6, 30, 54, 78, 102, 126, 0},
  {6, 26, 52, 78, 104, 130, 0},
  {6, 30, 56, 82, 108, 134, 0},
  {6, 34, 60, 86, 112, 138, 0},
  {6, 30, 58, 86, 114, 142, 0},
  {6, 34, 62, 90, 118, 146, 0},
  {6, 30, 54, 78, 102, 126, 150, 0},
  {6, 24, 50, 76, 102, 128, 154, 0},
  {6, 28, 54, 80, 106, 132, 158, 0},
  {6, 32, 58, 84, 110, 136, 162, 0},
  {6, 26, 54, 82, 110, 138, 166, 0},
  {6, 30, 58, 86, 114, 142, 170, 0},
};

static int is_function(int row, int col, int sz, int ver) {
  if (row <= 8 && col <= 8) {
    return 1;
  }
  if (row <= 8 && col >= sz - 8) {
    return 1;
  }
  if (row >= sz - 8 && col <= 8) {
    return 1;
  }
  if (row == 6 || col == 6) {
    return 1;
  }
  if (ver >= 7) {
    if (col >= sz - 11 && col <= sz - 9 && row <= 5) {
      return 1;
    }
    if (row >= sz - 11 && row <= sz - 9 && col <= 5) {
      return 1;
    }
  }
  if (ver >= 2 && ver <= 40) {
    const int *pos = ALIGN_POS[ver];
    int np = 0;
    while (pos[np] != 0 && np < 7) {
      np++;
    }
    for (int i = 0; i < np; i++) {
      for (int j = 0; j < np; j++) {
        int ay = pos[i], ax = pos[j];
        if (ay <= 8 && ax <= 8) {
          continue;
        }
        if (ay <= 8 && ax >= sz - 8) {
          continue;
        }
        if (ay >= sz - 8 && ax <= 8) {
          continue;
        }
        if (abs(row - ay) <= 2 && abs(col - ax) <= 2) {
          return 1;
        }
      }
    }
  }
  return 0;
}

/* ---- Data extraction in zigzag order ---- */

static int extract_bits(
  uint8_t const *grid,
  uint8_t const *conf,
  int sz,
  int ver,
  int mask,
  uint8_t *bits,
  uint8_t *bit_conf,
  int max_bits
) {
  int n = 0, up = 1;
  for (int right = sz - 1; right >= 1; right -= 2) {
    if (right == 6) {
      right = 5;
    }
    for (int i = 0; i < sz; i++) {
      int row = up ? (sz - 1 - i) : i;
      for (int c = 0; c < 2; c++) {
        int col = right - c;
        if (col < 0) {
          continue;
        }
        if (!is_function(row, col, sz, ver) && n < max_bits) {
          int val = grid[row * sz + col];
          if (apply_mask(mask, row, col)) {
            val ^= 1;
          }
          bits[n] = (uint8_t)val;
          if (bit_conf) {
            bit_conf[n] = conf ? conf[row * sz + col] : 0;
          }
          n++;
        }
      }
    }
    up = !up;
  }
  return n;
}

/* ---- Bits to bytes ---- */

static int bits_to_bytes(
  uint8_t const *bits,
  uint8_t const *bit_conf,
  int nbits,
  uint8_t *bytes,
  uint8_t *byte_ambig
) {
  int nb = nbits / 8;
  for (int i = 0; i < nb; i++) {
    uint8_t b = 0;
    int ambig = 0;
    for (int j = 0; j < 8; j++) {
      b = (b << 1) | bits[i * 8 + j];
      if (bit_conf && bit_conf[i * 8 + j]) {
        ambig = 1;
      }
    }
    bytes[i] = b;
    if (byte_ambig) {
      byte_ambig[i] = (uint8_t)ambig;
    }
  }
  return nb;
}

/* ---- Read N bits from byte array ---- */

static int read_bits(uint8_t const *data, int len, int *pos, int n) {
  int val = 0;
  for (int i = 0; i < n; i++) {
    int bi = *pos + i;
    int byte_idx = bi / 8, bit_idx = 7 - (bi % 8);
    if (byte_idx < len) {
      val = (val << 1) | ((data[byte_idx] >> bit_idx) & 1);
    }
    else {
      val <<= 1;
    }
  }
  *pos += n;
  return val;
}

/* ---- EC table ---- */

typedef struct {
  int total_cw, ec_cw_per_block;
  int g1_blocks, g1_data_cw;
  int g2_blocks, g2_data_cw;
} ECInfo;

static const ECInfo EC_TABLE[40][4] = {
  /* L                            M                         Q H */
  /* v1  */ {
    {26, 7, 1, 19, 0, 0},
    {26, 10, 1, 16, 0, 0},
    {26, 13, 1, 13, 0, 0},
    {26, 17, 1, 9, 0, 0}
  },
  /* v2  */
  {{44, 10, 1, 34, 0, 0},
   {44, 16, 1, 28, 0, 0},
   {44, 22, 1, 22, 0, 0},
   {44, 28, 1, 16, 0, 0}},
  /* v3  */
  {{70, 15, 1, 55, 0, 0},
   {70, 26, 1, 44, 0, 0},
   {70, 18, 2, 17, 0, 0},
   {70, 22, 2, 13, 0, 0}},
  /* v4  */
  {{100, 20, 1, 80, 0, 0},
   {100, 18, 2, 32, 0, 0},
   {100, 26, 2, 24, 0, 0},
   {100, 16, 4, 9, 0, 0}},
  /* v5  */
  {{134, 26, 1, 108, 0, 0},
   {134, 24, 2, 43, 0, 0},
   {134, 18, 2, 15, 2, 16},
   {134, 22, 2, 11, 2, 12}},
  /* v6  */
  {{172, 18, 2, 68, 0, 0},
   {172, 16, 4, 27, 0, 0},
   {172, 24, 4, 19, 0, 0},
   {172, 28, 4, 15, 0, 0}},
  /* v7  */
  {{196, 20, 2, 78, 0, 0},
   {196, 18, 4, 31, 0, 0},
   {196, 18, 2, 14, 4, 15},
   {196, 26, 4, 13, 1, 14}},
  /* v8  */
  {{242, 24, 2, 97, 0, 0},
   {242, 22, 2, 38, 2, 39},
   {242, 22, 4, 18, 2, 19},
   {242, 26, 4, 14, 2, 15}},
  /* v9  */
  {{292, 30, 2, 116, 0, 0},
   {292, 22, 3, 36, 2, 37},
   {292, 20, 4, 16, 4, 17},
   {292, 24, 4, 12, 4, 13}},
  /* v10 */
  {{346, 18, 2, 68, 2, 69},
   {346, 26, 4, 43, 1, 44},
   {346, 24, 6, 19, 2, 20},
   {346, 28, 6, 15, 2, 16}},
  /* v11 */
  {{404, 20, 4, 81, 0, 0},
   {404, 30, 1, 50, 4, 51},
   {404, 28, 4, 22, 4, 23},
   {404, 24, 3, 12, 8, 13}},
  /* v12 */
  {{466, 24, 2, 92, 2, 93},
   {466, 22, 6, 36, 2, 37},
   {466, 26, 4, 20, 6, 21},
   {466, 28, 7, 14, 4, 15}},
  /* v13 */
  {{532, 26, 4, 107, 0, 0},
   {532, 22, 8, 37, 1, 38},
   {532, 24, 8, 20, 4, 21},
   {532, 22, 12, 11, 4, 12}},
  /* v14 */
  {{581, 30, 3, 115, 1, 116},
   {581, 24, 4, 40, 5, 41},
   {581, 20, 11, 16, 5, 17},
   {581, 24, 11, 12, 5, 13}},
  /* v15 */
  {{655, 22, 5, 87, 1, 88},
   {655, 24, 5, 41, 5, 42},
   {655, 30, 5, 24, 7, 25},
   {655, 24, 11, 12, 7, 13}},
  /* v16 */
  {{733, 24, 5, 98, 1, 99},
   {733, 28, 7, 45, 3, 46},
   {733, 24, 15, 19, 2, 20},
   {733, 30, 3, 15, 13, 16}},
  /* v17 */
  {{815, 28, 1, 107, 5, 108},
   {815, 28, 10, 46, 1, 47},
   {815, 28, 1, 22, 15, 23},
   {815, 28, 2, 14, 17, 15}},
  /* v18 */
  {{901, 30, 5, 120, 1, 121},
   {901, 26, 9, 43, 4, 44},
   {901, 28, 17, 22, 1, 23},
   {901, 28, 2, 14, 19, 15}},
  /* v19 */
  {{991, 28, 3, 113, 4, 114},
   {991, 26, 3, 44, 11, 45},
   {991, 26, 17, 21, 4, 22},
   {991, 26, 9, 13, 16, 14}},
  /* v20 */
  {{1085, 28, 3, 107, 5, 108},
   {1085, 26, 3, 41, 13, 42},
   {1085, 30, 15, 24, 5, 25},
   {1085, 28, 15, 15, 10, 16}},
  /* v21 */
  {{1156, 28, 4, 116, 4, 117},
   {1156, 26, 17, 42, 0, 0},
   {1156, 28, 17, 22, 6, 23},
   {1156, 30, 19, 16, 6, 17}},
  /* v22 */
  {{1258, 28, 2, 111, 7, 112},
   {1258, 28, 17, 46, 0, 0},
   {1258, 30, 7, 24, 16, 25},
   {1258, 24, 34, 13, 0, 0}},
  /* v23 */
  {{1364, 30, 4, 121, 5, 122},
   {1364, 28, 4, 47, 14, 48},
   {1364, 30, 11, 24, 14, 25},
   {1364, 30, 16, 15, 14, 16}},
  /* v24 */
  {{1474, 30, 6, 117, 4, 118},
   {1474, 28, 6, 45, 14, 46},
   {1474, 30, 11, 24, 16, 25},
   {1474, 30, 30, 16, 2, 17}},
  /* v25 */
  {{1588, 26, 8, 106, 4, 107},
   {1588, 28, 8, 47, 13, 48},
   {1588, 30, 7, 24, 22, 25},
   {1588, 30, 22, 15, 13, 16}},
  /* v26 */
  {{1706, 28, 10, 114, 2, 115},
   {1706, 28, 19, 46, 4, 47},
   {1706, 28, 28, 22, 6, 23},
   {1706, 30, 33, 16, 4, 17}},
  /* v27 */
  {{1828, 30, 8, 122, 4, 123},
   {1828, 28, 22, 45, 3, 46},
   {1828, 30, 8, 23, 26, 24},
   {1828, 30, 12, 15, 28, 16}},
  /* v28 */
  {{1921, 30, 3, 117, 10, 118},
   {1921, 28, 3, 45, 23, 46},
   {1921, 30, 4, 24, 31, 25},
   {1921, 30, 11, 15, 31, 16}},
  /* v29 */
  {{2051, 30, 7, 116, 7, 117},
   {2051, 28, 21, 45, 7, 46},
   {2051, 30, 1, 23, 37, 24},
   {2051, 30, 19, 15, 26, 16}},
  /* v30 */
  {{2185, 30, 5, 115, 10, 116},
   {2185, 28, 19, 47, 10, 48},
   {2185, 30, 15, 24, 25, 25},
   {2185, 30, 23, 15, 25, 16}},
  /* v31 */
  {{2323, 30, 13, 115, 3, 116},
   {2323, 28, 2, 46, 29, 47},
   {2323, 30, 42, 24, 1, 25},
   {2323, 30, 23, 15, 28, 16}},
  /* v32 */
  {{2465, 30, 17, 115, 0, 0},
   {2465, 28, 10, 46, 23, 47},
   {2465, 30, 10, 24, 35, 25},
   {2465, 30, 19, 15, 35, 16}},
  /* v33 */
  {{2611, 30, 17, 115, 1, 116},
   {2611, 28, 14, 46, 21, 47},
   {2611, 30, 29, 24, 19, 25},
   {2611, 30, 11, 15, 46, 16}},
  /* v34 */
  {{2761, 30, 13, 115, 6, 116},
   {2761, 28, 14, 46, 23, 47},
   {2761, 30, 44, 24, 7, 25},
   {2761, 30, 59, 16, 1, 17}},
  /* v35 */
  {{2876, 30, 12, 121, 7, 122},
   {2876, 28, 12, 47, 26, 48},
   {2876, 30, 39, 24, 14, 25},
   {2876, 30, 22, 15, 41, 16}},
  /* v36 */
  {{3034, 30, 6, 121, 14, 122},
   {3034, 28, 6, 47, 34, 48},
   {3034, 30, 46, 24, 10, 25},
   {3034, 30, 2, 15, 64, 16}},
  /* v37 */
  {{3196, 30, 17, 122, 4, 123},
   {3196, 28, 29, 46, 14, 47},
   {3196, 30, 49, 24, 10, 25},
   {3196, 30, 24, 15, 46, 16}},
  /* v38 */
  {{3362, 30, 4, 122, 18, 123},
   {3362, 28, 13, 46, 32, 47},
   {3362, 30, 48, 24, 14, 25},
   {3362, 30, 42, 15, 32, 16}},
  /* v39 */
  {{3532, 30, 20, 117, 4, 118},
   {3532, 28, 40, 47, 7, 48},
   {3532, 30, 43, 24, 22, 25},
   {3532, 30, 10, 15, 67, 16}},
  /* v40 */
  {{3706, 30, 19, 118, 6, 119},
   {3706, 28, 18, 47, 31, 48},
   {3706, 30, 34, 24, 34, 25},
   {3706, 30, 20, 15, 61, 16}},
};

/* ---- Reed-Solomon error correction over GF(256), primitive poly 0x11d ---- */

static uint8_t gf_exp[512];
static uint8_t gf_log[256];
static int gf_initialized = 0;

static void gf_init(void) {
  if (gf_initialized) {
    return;
  }
  int x = 1;
  for (int i = 0; i < 255; i++) {
    gf_exp[i] = (uint8_t)x;
    gf_log[x] = (uint8_t)i;
    x <<= 1;
    if (x & 0x100) {
      x ^= 0x11d;
    }
  }
  for (int i = 255; i < 512; i++) {
    gf_exp[i] = gf_exp[i - 255];
  }
  gf_initialized = 1;
}

static uint8_t gf_mul(uint8_t a, uint8_t b) {
  if (a == 0 || b == 0) {
    return 0;
  }
  return gf_exp[gf_log[a] + gf_log[b]];
}

static uint8_t gf_inv(uint8_t a) { return gf_exp[255 - gf_log[a]]; }

/* Decode an RS block of nk data codewords followed by nroots EC codewords,
   in standard polynomial order (msg[0] is highest-degree coefficient).
   Optional erasure positions (eras_pos[k] is the msg index of an erasure)
   double the per-block correction budget: 2*errors + erasures <= nroots.
   Pass eras_pos=NULL/n_eras=0 for plain errors-only decoding (corrects up
   to nroots/2 errors). Returns number of corrections (errors+erasures) made
   in place, or -1 if the block is uncorrectable. */
static int rs_decode_block_eras(
  uint8_t *msg,
  int nk,
  int nroots,
  const int *eras_pos,
  int n_eras
) {
  gf_init();
  int n = nk + nroots;
  if (nroots <= 0 || nroots > 64 || nk <= 0 || n > 300) {
    return -1;
  }
  if (n_eras < 0 || n_eras > nroots) {
    return -1;
  }

  /* Syndromes: S[i] = msg(α^i) for i in [0, nroots). */
  uint8_t syn[64];
  int nonzero = 0;
  for (int i = 0; i < nroots; i++) {
    uint8_t x = gf_exp[i];
    uint8_t s = 0;
    for (int j = 0; j < n; j++) {
      s = gf_mul(s, x) ^ msg[j];
    }
    syn[i] = s;
    if (s) {
      nonzero = 1;
    }
  }
  if (!nonzero) {
    return 0;
  }

  /* Erasure locator Λ_E(x) = ∏ (1 - α^{n-1-eras_pos[k]} x), ascending degree.
     Roots at x = α^{-(n-1-eras_pos[k])}, matching the same x = α^{-i}
     convention used by the Chien search below. */
  uint8_t lambda_e[65] = {0};
  lambda_e[0] = 1;
  int le_deg = 0;
  uint8_t seen_eras[300] = {0};
  for (int k = 0; k < n_eras; k++) {
    int e = eras_pos[k];
    if (e < 0 || e >= n) {
      return -1;
    }
    if (seen_eras[e]) {
      continue; /* dedupe duplicate erasure positions */
    }
    seen_eras[e] = 1;
    int p = (n - 1 - e) % 255;
    if (p < 0) {
      p += 255;
    }
    uint8_t a = gf_exp[p];
    if (le_deg + 1 >= 65) {
      return -1;
    }
    le_deg++;
    for (int i = le_deg; i > 0; i--) {
      lambda_e[i] ^= gf_mul(a, lambda_e[i - 1]);
    }
  }

  /* Forney syndromes T(x) = (Λ_E(x) * S(x)) mod x^nroots. */
  uint8_t Tsyn[64] = {0};
  for (int i = 0; i < nroots; i++) {
    uint8_t s = 0;
    for (int j = 0; j <= le_deg && j <= i; j++) {
      s ^= gf_mul(lambda_e[j], syn[i - j]);
    }
    Tsyn[i] = s;
  }

  /* Berlekamp-Massey on the trailing (nroots - le_deg) Forney syndromes to
     find the error locator Σ(x) for unknown-position errors. The first
     le_deg Forney coefficients are absorbed by the erasure locator. */
  uint8_t C[65] = {0};
  uint8_t B[65] = {0};
  C[0] = 1;
  B[0] = 1;
  int L = 0;
  int m = 1;
  uint8_t b_last = 1;
  int n_iter = nroots - le_deg;

  for (int k = 0; k < n_iter; k++) {
    int kk = k + le_deg;
    uint8_t delta = Tsyn[kk];
    for (int i = 1; i <= L; i++) {
      if (kk - i >= 0) {
        delta ^= gf_mul(C[i], Tsyn[kk - i]);
      }
    }
    if (delta != 0) {
      uint8_t coef = gf_mul(delta, gf_inv(b_last));
      uint8_t Tc[65];
      memcpy(Tc, C, sizeof(Tc));
      for (int i = 0; i + m < 65; i++) {
        C[i + m] ^= gf_mul(coef, B[i]);
      }
      if (2 * L <= k) {
        L = k + 1 - L;
        memcpy(B, Tc, sizeof(B));
        b_last = delta;
        m = 1;
      }
      else {
        m++;
      }
    }
    else {
      m++;
    }
  }

  if (2 * L + le_deg > nroots) {
    return -1;
  }

  /* Combined locator Ψ(x) = Λ_E(x) * Σ(x). Coefficients in [0..130). */
  uint8_t Psi[130] = {0};
  for (int i = 0; i <= le_deg; i++) {
    if (!lambda_e[i]) {
      continue;
    }
    for (int j = 0; j <= L; j++) {
      if (!C[j]) {
        continue;
      }
      Psi[i + j] ^= gf_mul(lambda_e[i], C[j]);
    }
  }
  int psi_deg = le_deg + L;
  if (psi_deg <= 0 || psi_deg > nroots) {
    return -1;
  }

  /* Chien search: combined locator roots are at x = α^{-i}. */
  int err_pos[64];
  int nerr = 0;
  for (int i = 0; i < n; i++) {
    int xi = (255 - (i % 255)) % 255;
    uint8_t v = 0;
    for (int j = 0; j <= psi_deg; j++) {
      if (Psi[j]) {
        int e = (gf_log[Psi[j]] + xi * j) % 255;
        v ^= gf_exp[e];
      }
    }
    if (v == 0) {
      if (nerr >= 64) {
        return -1;
      }
      err_pos[nerr++] = i;
    }
  }
  if (nerr != psi_deg) {
    return -1;
  }

  /* Forney: Ω(x) = (S(x) * Ψ(x)) mod x^nroots, then magnitude at position i
     is Ω(α^{-i}) / Ψ'(α^{-i}). Ψ has degree up to nroots, so omega and Ψ'
     buffers need to span that range. */
  uint8_t omega[130] = {0};
  for (int i = 0; i < nroots; i++) {
    for (int j = 0; j <= psi_deg && i + j < 130; j++) {
      if (i + j < nroots) {
        omega[i + j] ^= gf_mul(syn[i], Psi[j]);
      }
    }
  }

  uint8_t Psip[130] = {0};
  for (int i = 1; i <= psi_deg; i += 2) {
    Psip[i - 1] = Psi[i];
  }

  for (int k = 0; k < nerr; k++) {
    int i = err_pos[k];
    int xi = (255 - (i % 255)) % 255;

    uint8_t o = 0;
    for (int j = 0; j < nroots; j++) {
      if (omega[j]) {
        int e = (gf_log[omega[j]] + xi * j) % 255;
        o ^= gf_exp[e];
      }
    }

    uint8_t cp = 0;
    for (int j = 0; j < 130; j++) {
      if (Psip[j]) {
        int e = (gf_log[Psip[j]] + xi * j) % 255;
        cp ^= gf_exp[e];
      }
    }

    if (cp == 0) {
      return -1;
    }
    /* Forney for syndromes starting at b=0: e_j = X_j * Ω(X_j^{-1}) /
       Ψ'(X_j^{-1}). The X_j factor is essential — without it the recovered
       magnitude is e_j / X_j, the verify check fails, and RS decoding silently
       bails out on every block with at least one real error. */
    int xpow = i % 255;
    uint8_t correction = gf_mul(o, gf_inv(cp));
    correction = gf_mul(correction, gf_exp[xpow]);
    msg[n - 1 - i] ^= correction;
  }

  /* Verify: all syndromes zero now. */
  for (int i = 0; i < nroots; i++) {
    uint8_t x = gf_exp[i];
    uint8_t s = 0;
    for (int j = 0; j < n; j++) {
      s = gf_mul(s, x) ^ msg[j];
    }
    if (s != 0) {
      return -1;
    }
  }

  return nerr;
}

static int rs_decode_block(uint8_t *msg, int nk, int nroots) {
  return rs_decode_block_eras(msg, nk, nroots, NULL, 0);
}

/* ---- De-interleave codewords + Reed-Solomon error correction ----
   Splits the raw interleaved codeword stream into per-block [data | EC]
   buffers, runs RS decode on each block, and writes the corrected data
   codewords to `out`. Returns the total number of data codewords, or 0 on
   structural failure. Individual block RS failures are tolerated: the
   (uncorrected) bytes are still written so the payload decoder can try.
   If out_rs_cost is non-NULL, writes the total RS error-correction cost:
   sum of bits corrected across all blocks, plus a heavy penalty per
   uncorrectable block. Lower values mean a more confident decode. */
static int deinterleave(
  uint8_t const *raw,
  uint8_t const *raw_ambig,
  int raw_len,
  const ECInfo *info,
  uint8_t *out,
  int *out_rs_cost
) {
  int total_blocks = info->g1_blocks + info->g2_blocks;
  if (total_blocks == 0 || total_blocks > 100) {
    return 0;
  }
  int ec_cw = info->ec_cw_per_block;
  int total_data =
    info->g1_blocks * info->g1_data_cw + info->g2_blocks * info->g2_data_cw;
  int total_needed = total_data + total_blocks * ec_cw;
  if (total_needed > raw_len) {
    /* Not enough bytes to run RS. Fall back to copying available data only. */
    int n = total_data < raw_len ? total_data : raw_len;
    memcpy(out, raw, n);
    return n;
  }

  uint8_t **blocks = calloc(total_blocks, sizeof(uint8_t *));
  uint8_t **block_ambig = NULL;
  int *dlen = calloc(total_blocks, sizeof(int));
  if (raw_ambig) {
    block_ambig = calloc(total_blocks, sizeof(uint8_t *));
  }
  if (!blocks || !dlen || (raw_ambig && !block_ambig)) {
    free(blocks);
    free(dlen);
    free(block_ambig);
    return 0;
  }
  int alloc_fail = 0;
  for (int b = 0; b < total_blocks; b++) {
    dlen[b] = (b < info->g1_blocks) ? info->g1_data_cw : info->g2_data_cw;
    blocks[b] = calloc(dlen[b] + ec_cw, 1);
    if (block_ambig) {
      block_ambig[b] = calloc(dlen[b] + ec_cw, 1);
      if (!block_ambig[b]) {
        alloc_fail = 1;
      }
    }
    if (!blocks[b]) {
      alloc_fail = 1;
    }
  }
  if (alloc_fail) {
    for (int b = 0; b < total_blocks; b++) {
      free(blocks[b]);
      if (block_ambig) {
        free(block_ambig[b]);
      }
    }
    free(blocks);
    free(block_ambig);
    free(dlen);
    return 0;
  }

  int max_data_cw = (info->g2_blocks > 0) ? info->g2_data_cw : info->g1_data_cw;
  int pos = 0;

  /* Data codewords are interleaved round-robin by position. Group-1 blocks
     run out first (they're shorter), so later rounds only fill group-2. */
  for (int i = 0; i < max_data_cw; i++) {
    for (int b = 0; b < total_blocks && pos < raw_len; b++) {
      if (i < dlen[b]) {
        blocks[b][i] = raw[pos];
        if (block_ambig) {
          block_ambig[b][i] = raw_ambig[pos];
        }
        pos++;
      }
    }
  }
  /* EC codewords: all blocks have the same EC length, so clean round-robin. */
  for (int i = 0; i < ec_cw; i++) {
    for (int b = 0; b < total_blocks && pos < raw_len; b++) {
      blocks[b][dlen[b] + i] = raw[pos];
      if (block_ambig) {
        block_ambig[b][dlen[b] + i] = raw_ambig[pos];
      }
      pos++;
    }
  }

  /* Snapshot the original block bytes BEFORE rs_decode_block mutates them
     in place. Used to retry with erasures (and to detect when erasure
     decoding flips bytes the errors-only path "corrected" away). */
  uint8_t **orig = NULL;
  if (block_ambig) {
    orig = calloc(total_blocks, sizeof(uint8_t *));
    if (orig) {
      for (int b = 0; b < total_blocks; b++) {
        int blk_n = dlen[b] + ec_cw;
        orig[b] = malloc(blk_n);
        if (orig[b]) {
          memcpy(orig[b], blocks[b], blk_n);
        }
      }
    }
  }

  int rs_cost = 0;
  for (int b = 0; b < total_blocks; b++) {
    int blk_n = dlen[b] + ec_cw;
    int corr = rs_decode_block(blocks[b], dlen[b], ec_cw);

    if (block_ambig && orig && orig[b]) {
      /* Collect erasure candidate positions for this block. */
      int eras_pos[64];
      int n_eras = 0;
      for (int k = 0; k < blk_n && n_eras < ec_cw && n_eras < 64; k++) {
        if (block_ambig[b][k]) {
          eras_pos[n_eras++] = k;
        }
      }

      if (n_eras > 0) {
        uint8_t *retry = malloc(blk_n);
        if (retry) {
          memcpy(retry, orig[b], blk_n);
          int corr2 =
            rs_decode_block_eras(retry, dlen[b], ec_cw, eras_pos, n_eras);
          if (corr2 >= 0) {
            int prefer_erasure = 0;
            if (corr < 0) {
              /* Errors-only failed; erasure version is our only option. */
              prefer_erasure = 1;
            }
            else if (memcmp(retry, blocks[b], blk_n) != 0) {
              /* Errors-only "succeeded" but with a different answer than
                 the erasure-aware decode. The erasure decode had MORE
                 constraints (specific byte positions known to be unreliable)
                 so it's more likely to be the true codeword when our
                 erasure candidates overlap the real errors. The risk: if
                 most candidates were wrong, this flips a correct decode to
                 a wrong one — the upper-layer payload validator and
                 fmt_dist scoring is what saves us when that happens. */
              prefer_erasure = 1;
            }
            if (prefer_erasure) {
              memcpy(blocks[b], retry, blk_n);
              corr = corr2;
              rs_cost += corr2 + n_eras * 2 + 200;
              free(retry);
              continue;
            }
          }
          free(retry);
        }
      }
    }

    if (corr < 0) {
      /* Uncorrectable block: every EC symbol effectively spent without
         success. Penalise heavily so this decode loses ties to one whose
         blocks all decoded cleanly. */
      rs_cost += ec_cw * 8 + 1000;
    }
    else {
      rs_cost += corr;
    }
  }
  if (orig) {
    for (int b = 0; b < total_blocks; b++) {
      free(orig[b]);
    }
    free(orig);
  }
  if (out_rs_cost) {
    *out_rs_cost = rs_cost;
  }

  int out_pos = 0;
  for (int b = 0; b < total_blocks; b++) {
    memcpy(out + out_pos, blocks[b], dlen[b]);
    out_pos += dlen[b];
    free(blocks[b]);
    if (block_ambig) {
      free(block_ambig[b]);
    }
  }
  free(blocks);
  free(block_ambig);
  free(dlen);
  return out_pos;
}

/* ---- Payload decoding ---- */

static char *decode_payload(uint8_t const *data, int len, int ver) {
  if (len < 1) {
    return NULL;
  }
  int pos = 0;
  int mode = read_bits(data, len, &pos, 4);
  if (mode == 0) {
    return NULL;
  }

  /* ECI (Extended Channel Interpretation): consume the assignment number
     bytes and continue with the next mode indicator. URL QRs commonly
     begin with ECI 26 (UTF-8) or ECI 3 (ISO-8859-1) followed by byte
     mode. We don't do any character-set re-encoding — all the ECIs we
     care about produce payloads that are already valid UTF-8 or ASCII. */
  if (mode == 7) {
    int first = read_bits(data, len, &pos, 1);
    if (first < 0) {
      return NULL;
    }
    if (first == 0) {
      /* 1-byte ECI (7 more bits, total 8) */
      if (read_bits(data, len, &pos, 7) < 0) {
        return NULL;
      }
    }
    else {
      int second = read_bits(data, len, &pos, 1);
      if (second < 0) {
        return NULL;
      }
      if (second == 0) {
        /* 2-byte ECI (14 more bits, total 16) */
        if (read_bits(data, len, &pos, 14) < 0) {
          return NULL;
        }
      }
      else {
        /* 3-byte ECI (21 more bits, total 24) */
        if (read_bits(data, len, &pos, 21) < 0) {
          return NULL;
        }
      }
    }
    mode = read_bits(data, len, &pos, 4);
    if (mode == 0) {
      return NULL;
    }
  }

  int cc_bits;
  if (mode == 1) {
    cc_bits = (ver < 10) ? 10 : (ver < 27) ? 12 : 14;
  }
  else if (mode == 2) {
    cc_bits = (ver < 10) ? 9 : (ver < 27) ? 11 : 13;
  }
  else if (mode == 4) {
    cc_bits = (ver < 10) ? 8 : 16;
  }
  else if (mode == 8) {
    cc_bits = (ver < 10) ? 8 : (ver < 27) ? 10 : 12;
  }
  else {
    return NULL;
  }

  int count = read_bits(data, len, &pos, cc_bits);
  /* Reject impossible counts: must be positive and must fit in the data bits
     remaining after all header bits consumed so far (mode + optional ECI
     header + count). A bit-flip in the high bit of the count field can
     otherwise produce massively-inflated counts that consume padding bytes
     as fake content (numeric mode is especially prone — corrupted padding
     bytes decode as long runs of '0' digits). */
  int avail_bits = len * 8 - pos;
  if (avail_bits < 0) {
    free(NULL);
    return NULL;
  }
  int max_count;
  if (mode == 1) { /* Numeric: 10 bits per 3 digits */
    max_count = (avail_bits * 3) / 10;
  }
  else if (mode == 2) { /* Alphanumeric: 11 bits per 2 chars */
    max_count = (avail_bits * 2) / 11;
  }
  else if (mode == 4) { /* Byte: 8 bits per char */
    max_count = avail_bits / 8;
  }
  else { /* Kanji: 13 bits per char */
    max_count = avail_bits / 13;
  }
  if (count <= 0 || count > max_count) {
    return NULL;
  }

  char *result = malloc(count + 1);
  if (!result) {
    return NULL;
  }

  if (mode == 4) { /* Byte */
    for (int i = 0; i < count; i++) {
      result[i] = (char)read_bits(data, len, &pos, 8);
    }
    result[count] = '\0';
  }
  else if (mode == 2) { /* Alphanumeric */
    static const char tbl[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
    int i = 0;
    while (i + 1 < count) {
      int v = read_bits(data, len, &pos, 11);
      result[i] = tbl[v / 45];
      result[i + 1] = tbl[v % 45];
      i += 2;
    }
    if (i < count) {
      result[i] = tbl[read_bits(data, len, &pos, 6)];
    }
    result[count] = '\0';
  }
  else if (mode == 1) { /* Numeric */
    int i = 0;
    while (i + 2 < count) {
      int v = read_bits(data, len, &pos, 10);
      result[i] = '0' + v / 100;
      result[i + 1] = '0' + (v / 10) % 10;
      result[i + 2] = '0' + v % 10;
      i += 3;
    }
    if (count - i == 2) {
      int v = read_bits(data, len, &pos, 7);
      result[i] = '0' + v / 10;
      result[i + 1] = '0' + v % 10;
    }
    else if (count - i == 1) {
      result[i] = '0' + read_bits(data, len, &pos, 4);
    }
    result[count] = '\0';
  }
  else {
    free(result);
    return NULL;
  }
  return result;
}

/* Decode a sampled grid into a payload string, assuming canonical
   orientation (finders at TL / TR / BL). Returns NULL on failure. Writes
   the format-code Hamming distance to *out_fmt_dist if non-NULL. Writes
   the Reed-Solomon correction cost (lower = more confident) to
   *out_rs_cost if non-NULL. */
static char *decode_grid_oriented(
  uint8_t *grid,
  uint8_t const *conf,
  int qr_size,
  int version,
  int *out_fmt_dist,
  int *out_rs_cost
) {
  /* Structural check on the three finder windows. Real QRs match the
     7x7 finder pattern at (0,0) / (0, qr_size-7) / (qr_size-7, 0) almost
     perfectly; hallucinated grids only match about half the 147 cells.
     The 0.15 cap leaves room for moderate homography drift and sampling
     jitter under blur/perspective while killing the small-module numeric
     FPs that slipped past the timing alternation. */
  float fer = finder_error_rate(grid, qr_size);
  if (fer > 0.15f) {
    return NULL;
  }
  /* Gate on the timing pattern before the expensive RS path runs. Triples
     lifted from noise almost always flunk this (alternation is random).
     Base threshold 0.22 gives real QRs under moderate blur/perspective
     plenty of headroom while nearly every hallucinated grid sits around
     0.5. When the three finder windows match their 7x7 template almost
     perfectly (fer < 0.03) we're confident the triple is real — raise
     the timing cap to 0.35 so photographs with paper curl or moderate
     perspective drift don't get rejected on what is ultimately a grid-
     sampling approximation error, not a false positive. */
  float timing_cap = (fer < 0.03f) ? 0.35f : 0.22f;
  if (timing_error_rate(grid, qr_size) > timing_cap) {
    return NULL;
  }
  int ec_level, mask_pattern;
  int fmt_dist = 16;
  if (decode_format(grid, qr_size, &ec_level, &mask_pattern, &fmt_dist) < 0) {
    return NULL;
  }
  if (out_fmt_dist) {
    *out_fmt_dist = fmt_dist;
  }
  if (out_rs_cost) {
    *out_rs_cost = 100000;
  }
  int max_bits = qr_size * qr_size;
  uint8_t *data_bits = malloc(max_bits);
  uint8_t *bit_conf = NULL;
  if (conf) {
    bit_conf = malloc(max_bits);
    if (!bit_conf) {
      free(data_bits);
      return NULL;
    }
  }
  if (!data_bits) {
    free(bit_conf);
    return NULL;
  }
  int nbits = extract_bits(
    grid,
    conf,
    qr_size,
    version,
    mask_pattern,
    data_bits,
    bit_conf,
    max_bits
  );
  int max_bytes = nbits / 8;
  uint8_t *raw_bytes = malloc(max_bytes + 1);
  uint8_t *byte_ambig = NULL;
  if (bit_conf) {
    byte_ambig = malloc(max_bytes + 1);
    if (!byte_ambig) {
      free(data_bits);
      free(bit_conf);
      free(raw_bytes);
      return NULL;
    }
  }
  if (!raw_bytes) {
    free(data_bits);
    free(bit_conf);
    free(byte_ambig);
    return NULL;
  }
  int nraw = bits_to_bytes(data_bits, bit_conf, nbits, raw_bytes, byte_ambig);
  free(data_bits);
  free(bit_conf);

  int data_cw_count = nraw;
  uint8_t *data_bytes = raw_bytes;
  uint8_t *deint_buf = NULL;
  int rs_cost = 100000;
  if (version >= 1 && version <= 40 && ec_level >= 0 && ec_level <= 3) {
    const ECInfo *info = &EC_TABLE[version - 1][ec_level];
    int total_data =
      info->g1_blocks * info->g1_data_cw + info->g2_blocks * info->g2_data_cw;
    if (total_data > 0 && total_data <= nraw) {
      deint_buf = malloc(total_data + 1);
      if (deint_buf) {
        data_cw_count =
          deinterleave(raw_bytes, byte_ambig, nraw, info, deint_buf, &rs_cost);
        data_bytes = deint_buf;
      }
      else {
        data_cw_count = total_data;
      }
    }
  }
  char *decoded = decode_payload(data_bytes, data_cw_count, version);
  free(raw_bytes);
  free(byte_ambig);
  free(deint_buf);
  if (decoded) {
    int dlen = (int)strlen(decoded);
    for (int i = 0; i < dlen; i++) {
      uint8_t ch = (uint8_t)decoded[i];
      if (ch < 0x20 || ch > 0x7e) {
        free(decoded);
        return NULL;
      }
    }
  }
  if (decoded && out_rs_cost) {
    *out_rs_cost = rs_cost;
  }
  return decoded;
}

/* Transpose an n*n byte buffer (grid[i*n+j] -> out[j*n+i]). */
static void transpose_grid_u8(uint8_t const *src, uint8_t *dst, int n) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      dst[j * n + i] = src[i * n + j];
    }
  }
}

/* Decode a sampled grid, retrying with the matrix transposed when the
   canonical orientation fails. A "mirrored" QR (ZXing terminology: the
   symbol read as its own reflection, e.g. through a mirror or printed
   wrong-way-round) appears with rows and columns swapped relative to the
   spec. Finder and timing patterns are symmetric under transposition, so
   those structural checks pass on both orientations, but decode_format
   samples the wrong cells and the decode aborts. Transposing the grid
   (and confidence buffer) puts the format, mask, alignment patterns, and
   zigzag data reading back into canonical positions. Runs only after the
   canonical decode returns NULL, so valid non-mirrored QRs never pay for
   the retry and cannot be displaced by a spurious mirror decode. */
static char *decode_grid(
  uint8_t *grid,
  uint8_t const *conf,
  int qr_size,
  int version,
  int *out_fmt_dist,
  int *out_rs_cost
) {
  char *d = decode_grid_oriented(
    grid,
    conf,
    qr_size,
    version,
    out_fmt_dist,
    out_rs_cost
  );
  if (d) {
    return d;
  }

  int n = qr_size * qr_size;
  uint8_t *tgrid = malloc((size_t)n);
  if (!tgrid) {
    return NULL;
  }
  uint8_t *tconf = NULL;
  if (conf) {
    tconf = malloc((size_t)n);
    if (!tconf) {
      free(tgrid);
      return NULL;
    }
    transpose_grid_u8(conf, tconf, qr_size);
  }
  transpose_grid_u8(grid, tgrid, qr_size);

  int fd = 16, rc = 100000;
  char *dm = decode_grid_oriented(tgrid, tconf, qr_size, version, &fd, &rc);
  free(tgrid);
  free(tconf);
  if (dm) {
    if (out_fmt_dist) {
      *out_fmt_dist = fd;
    }
    if (out_rs_cost) {
      *out_rs_cost = rc;
    }
  }
  return dm;
}

/* ---- Decode the grid under a homography, then perturb the sample offset
   to hunt for a stronger decode ----
   The 5-point voting in sample_grid_homography_offset_mode is robust to small
   grid drift inside a single module, but if the WHOLE sampling grid is shifted
   by a fraction of a module (typical when finder centers carry sub-module error
   under blur or perspective), entire rows or columns get misread the same
   way. A 1-bit error in the byte-mode count field is the most damaging
   failure mode — it can truncate an 11-character decode to 3 characters
   while still passing the format BCH cleanly. Re-sampling at small (dx, dy)
   shifts in module space and keeping the best decode catches these
   off-by-fraction grid misalignments. */
static char *decode_with_perturbation(
  uint8_t const *pixels,
  uint8_t const *gray,
  int width,
  int height,
  const double H[9],
  int qr_size,
  int version,
  int *out_fmt_dist,
  int *out_rs_cost
) {
  /* Sample all 9 offsets (canonical + 8 perturbations) and collect the
     decodes with their format-distance scores. We then apply two acceptance
     paths: (a) if canonical succeeds, allow a strictly-longer perturbation
     that has the canonical result as a prefix to replace it (the byte-mode
     count-truncation recovery — "c3Q" -> "c3QrgLROiE5"). (b) if canonical
     fails, only accept a perturbation result that two or more perturbations
     agree on (a consensus decode), because lone perturbation decodes from
     triples whose canonical homography fails are almost always wrong same-
     length samples that other triples would decode correctly. */
#define PERT_N 10
  /* Sample plan: 8 NN perturbations + 2 canonical samples (NN + bilinear).
     Bilinear is only used at the canonical (0,0) offset — its purpose is
     specifically to recover from the byte-mode count-truncation pattern
     where NN nearest-pixel rounding clips the high bit of the count field.
     Off-canonical bilinear samples introduce too many same-length wrong
     decodes that distort the consensus vote in path (b). */
  static const double offs[PERT_N][2] = {
    {0.00, 0.00},
    {0.00, 0.00}, /* p=0 NN canonical, p=1 bilinear canonical */
    {0.30, 0.00},
    {-0.30, 0.00},
    {0.00, 0.30},
    {0.00, -0.30},
    {0.30, 0.30},
    {-0.30, 0.30},
    {0.30, -0.30},
    {-0.30, -0.30},
  };
  char *decodes[PERT_N] = {0};
  int fds[PERT_N];
  int rs_costs[PERT_N];
  size_t lens[PERT_N] = {0};
  for (int i = 0; i < PERT_N; i++) {
    fds[i] = 16;
    rs_costs[i] = 100000;
  }
  for (int p = 0; p < PERT_N; p++) {
    int use_bilinear = (p == 1);
    uint8_t *conf = NULL;
    uint8_t *g = sample_grid_homography_offset_mode(
      pixels,
      gray,
      width,
      height,
      H,
      qr_size,
      offs[p][0],
      offs[p][1],
      use_bilinear,
      gray ? &conf : NULL
    );
    if (!g) {
      free(conf);
      continue;
    }
    char *d = decode_grid(g, conf, qr_size, version, &fds[p], &rs_costs[p]);
    free(g);
    free(conf);
    if (d) {
      decodes[p] = d;
      lens[p] = strlen(d);
    }
    /* Early exit if the NN canonical (p=0) is already a long, perfect
       decode with no RS corrections — nothing can credibly improve on it. */
    if (p == 0 && d && fds[0] == 0 && lens[0] >= 8 && rs_costs[0] == 0) {
      break;
    }
  }

  char *best = NULL;
  int best_fd = 16;
  int best_rs_out = 100000;

  if (decodes[0]) {
    /* Path (a): canonical succeeded. Replace either with (i) a strictly
       longer decode that has the canonical as a prefix (the byte-mode
       count-truncation recovery), or (ii) a same-length decode whose
       Reed-Solomon correction cost is strictly lower (RS needed fewer
       fixes, which is a strong signal that the perturbation hit cleaner
       data bits even when format BCH ties at fmt_dist=0). */
    best = decodes[0];
    best_fd = fds[0];
    int best_rs = rs_costs[0];
    decodes[0] = NULL;
    for (int p = 1; p < PERT_N; p++) {
      if (!decodes[p]) {
        continue;
      }
      int replace = 0;
      if (lens[p] > strlen(best) &&
          strncmp(decodes[p], best, strlen(best)) == 0) {
        replace = 1;
      }
      else if (lens[p] == strlen(best) && rs_costs[p] < best_rs) {
        replace = 1;
      }
      if (replace) {
        free(best);
        best = decodes[p];
        best_fd = fds[p];
        best_rs = rs_costs[p];
        decodes[p] = NULL;
      }
    }
    best_rs_out = best_rs;
  }
  else {
    /* Path (b): canonical failed. Pick the perturbation decode that the
       most other perturbations agree on, requiring at least two
       agreements total. Tie-break: longer string, then lower fmt_dist.
       Without consensus, return NULL so other triples can be tried. */
    int best_idx = -1;
    int best_count = 0;
    for (int i = 1; i < PERT_N; i++) {
      if (!decodes[i]) {
        continue;
      }
      int count = 0;
      for (int j = 1; j < PERT_N; j++) {
        if (decodes[j] && strcmp(decodes[i], decodes[j]) == 0) {
          count++;
        }
      }
      int better = 0;
      if (count > best_count) {
        better = 1;
      }
      else if (count == best_count && best_idx >= 0) {
        if (lens[i] > lens[best_idx]) {
          better = 1;
        }
        else if (lens[i] == lens[best_idx] && fds[i] < fds[best_idx]) {
          better = 1;
        }
      }
      if (better) {
        best_count = count;
        best_idx = i;
      }
    }
    if (best_idx >= 0 && best_count >= 2) {
      best = decodes[best_idx];
      best_fd = fds[best_idx];
      best_rs_out = rs_costs[best_idx];
      decodes[best_idx] = NULL;
    }
  }

  for (int p = 0; p < PERT_N; p++) {
    free(decodes[p]);
  }
#undef PERT_N
  if (best) {
    if (out_fmt_dist) {
      *out_fmt_dist = best_fd;
    }
    if (out_rs_cost) {
      *out_rs_cost = best_rs_out;
    }
  }
  return best;
}

/* ---- Try to decode a single QR code from a triple ---- */

static char *try_decode_triple(
  uint8_t const *pixels,
  uint8_t const *gray,
  int width,
  int height,
  FinderPattern tl,
  FinderPattern tr,
  FinderPattern bl,
  int *out_fmt_dist,
  int *out_rs_cost,
  Corners *out_corners,
  Point2D out_finders[3],
  double *out_module_size
) {
  float dist_t = fp_dist(tl, tr), dist_l = fp_dist(tl, bl);
  float avg_d = (dist_t + dist_l) / 2.0f;
  float avg_m = (tl.module_size + tr.module_size + bl.module_size) / 3.0f;
  if (avg_m < 0.5f) {
    return NULL;
  }
  float qr_sz_f = avg_d / avg_m + 7.0f;
  int version = (int)((qr_sz_f - 21.0f + 2.0f) / 4.0f) + 1;
  if (version < 1) {
    version = 1;
  }
  if (version > 40) {
    version = 40;
  }
  int qr_size = 21 + (version - 1) * 4;

  /* Build a perspective transform for grid sampling. For v≥2 QRs we search
     for the BR alignment pattern and use its pixel location as the 4th
     homography point; under perspective distortion this beats the
     parallelogram-BR assumption that affine sampling relies on. For v1 (no
     alignment) we fall back to the parallelogram BR. */
  int fmt_dist = 16;
  int rs_cost = 100000;
  char *decoded = NULL;

  /* Track the best initial homography we have so timing-pattern refinement
     has a starting point to predict module-centroid positions from. */
  double H_primary[9];
  int have_primary_H = 0;
  int have_alignment = 0;
  double alignment_x = 0, alignment_y = 0;

  if (version >= 2) {
    float sx0 = 3.5f, sy0 = 3.5f;
    float sx1 = qr_size - 3.5f, sy1 = 3.5f;
    float sx2 = 3.5f, sy2 = qr_size - 3.5f;
    float denom = (sx0 - sx2) * (sy1 - sy2) - (sx1 - sx2) * (sy0 - sy2);
    if (fabsf(denom) >= 1e-6f) {
      float a11 =
        ((tl.x - bl.x) * (sy1 - sy2) - (tr.x - bl.x) * (sy0 - sy2)) / denom;
      float a12 =
        ((tr.x - bl.x) * (sx0 - sx2) - (tl.x - bl.x) * (sx1 - sx2)) / denom;
      float a21 =
        ((tl.y - bl.y) * (sy1 - sy2) - (tr.y - bl.y) * (sy0 - sy2)) / denom;
      float a22 =
        ((tr.y - bl.y) * (sx0 - sx2) - (tl.y - bl.y) * (sx1 - sx2)) / denom;
      float b1 = tl.x - a11 * sx0 - a12 * sy0;
      float b2 = tl.y - a21 * sx0 - a22 * sy0;
      float amx = qr_size - 6.5f, amy = qr_size - 6.5f;
      float predicted_ax = a11 * amx + a12 * amy + b1;
      float predicted_ay = a21 * amx + a22 * amy + b2;
      float search_r = avg_m * 10.0f;
      float found_ax, found_ay;
      if (find_alignment_pattern(
            pixels,
            width,
            height,
            predicted_ax,
            predicted_ay,
            avg_m,
            search_r,
            &found_ax,
            &found_ay
          )) {
        have_alignment = 1;
        alignment_x = found_ax;
        alignment_y = found_ay;
        double src[4][2] = {{sx0, sy0}, {sx1, sy1}, {amx, amy}, {sx2, sy2}};
        double dst[4][2] =
          {{tl.x, tl.y}, {tr.x, tr.y}, {found_ax, found_ay}, {bl.x, bl.y}};
        double H[9];
        if (compute_homography(src, dst, H)) {
          memcpy(H_primary, H, sizeof(H));
          have_primary_H = 1;
          int fd = 16;
          int rc = 100000;
          char *d = decode_with_perturbation(
            pixels,
            gray,
            width,
            height,
            H,
            qr_size,
            version,
            &fd,
            &rc
          );
          if (d) {
            decoded = d;
            fmt_dist = fd;
            rs_cost = rc;
          }
        }
        /* If the canonical alignment-based homography failed, sweep a
           small grid of alignment-point perturbations and retry. Paper curl
           and perspective foreshortening mean the real alignment pattern
           can sit several pixels from where find_alignment_pattern snaps
           to, while the homography is extremely sensitive to this 4th
           point — a few pixels at the BR alignment translate to sub-module
           drift across the data area, tripping the timing and finder
           gates even when the three finder positions are pin-accurate.
           Sweep radius scales with module size (the residual alignment
           uncertainty is always a fraction of a module); 2x2 module
           radius at half-module steps is cheap (25 samples) and covers
           the drift seen on moderately curled photos. */
        if (!decoded) {
          float step = avg_m * 0.5f;
          float radius = avg_m * 1.5f;
          int n_steps = (int)(radius / step);
          char *best_sweep = NULL;
          int best_sweep_fd = 16;
          int best_sweep_rc = 100000;
          for (int dy = -n_steps; dy <= n_steps; dy++) {
            for (int dx = -n_steps; dx <= n_steps; dx++) {
              if (dx == 0 && dy == 0) {
                continue; /* already tried */
              }
              double pert_dst[4][2] = {
                {tl.x, tl.y},
                {tr.x, tr.y},
                {found_ax + dx * step, found_ay + dy * step},
                {bl.x, bl.y}
              };
              double Hp[9];
              if (!compute_homography(src, pert_dst, Hp)) {
                continue;
              }
              int fd = 16;
              int rc = 100000;
              char *d = decode_with_perturbation(
                pixels,
                gray,
                width,
                height,
                Hp,
                qr_size,
                version,
                &fd,
                &rc
              );
              if (!d) {
                continue;
              }
              /* Take the lowest-cost decode across the whole sweep rather
                 than stopping on the first hit — some perturbed alignments
                 yield a consensus decode that is still garbage (random
                 alphanumeric strings with fd=0 and rc=0 do occur when
                 sampling happens to line up with a false-but-self-
                 consistent grid). Prefer longer strings and lower RS
                 cost so the best geometric fit wins. */
              int replace = 0;
              if (!best_sweep) {
                replace = 1;
              }
              else if (fd < best_sweep_fd) {
                replace = 1;
              }
              else if (fd == best_sweep_fd &&
                       (int)strlen(d) > (int)strlen(best_sweep)) {
                replace = 1;
              }
              else if (fd == best_sweep_fd &&
                       (int)strlen(d) == (int)strlen(best_sweep) &&
                       rc < best_sweep_rc) {
                replace = 1;
              }
              if (replace) {
                free(best_sweep);
                best_sweep = d;
                best_sweep_fd = fd;
                best_sweep_rc = rc;
              }
              else {
                free(d);
              }
            }
          }
          /* Only accept sweep results with a strong confidence signal:
             perfect format BCH (fd = 0) and near-zero RS correction cost.
             Random perturbations in a QR-free region can consensus-
             decode as alphanumeric garbage with fd=0 but substantial RS
             cost (≥ 100), so locking the rc threshold low rejects those.
             Real decodes with the right alignment typically need zero
             corrections; a small tolerance (≤ 5) lets through slightly
             noisy captures without admitting hallucinated decodes. */
          if (best_sweep && best_sweep_fd == 0 && best_sweep_rc <= 5) {
            decoded = best_sweep;
            fmt_dist = best_sweep_fd;
            rs_cost = best_sweep_rc;
          }
          else {
            free(best_sweep);
          }
        }
      }
    }
  }

  /* If no homography-based decode worked yet, try the plain parallelogram
     BR (affine-equivalent). */
  if (!decoded) {
    float br_x = tr.x + bl.x - tl.x;
    float br_y = tr.y + bl.y - tl.y;
    double src[4][2] = {
      {3.5, 3.5},
      {qr_size - 3.5, 3.5},
      {qr_size - 3.5, qr_size - 3.5},
      {3.5, qr_size - 3.5}
    };
    double dst[4][2] = {{tl.x, tl.y}, {tr.x, tr.y}, {br_x, br_y}, {bl.x, bl.y}};
    double H[9];
    if (compute_homography(src, dst, H)) {
      if (!have_primary_H) {
        memcpy(H_primary, H, sizeof(H));
        have_primary_H = 1;
      }
      int fd = 16;
      int rc = 100000;
      char *d = decode_with_perturbation(
        pixels,
        gray,
        width,
        height,
        H,
        qr_size,
        version,
        &fd,
        &rc
      );
      if (d) {
        decoded = d;
        fmt_dist = fd;
        rs_cost = rc;
      }
    }
  }

  /* Timing-pattern homography refinement. When a 4-point homography (from
     alignment pattern or parallelogram BR) can't produce a clean decode,
     the usual cause on photos with any perspective or paper curl is
     sub-module drift in the QR's interior — the 4 control points anchor
     the corners but nothing constrains the transform *between* them.
     Observing pixel-space centroids of every known-dark timing module
     gives us ≈ 2·(qr_size − 16) extra correspondences threading the
     code's interior, and solving an over-determined DLT over the union
     of control points + timing observations averages out the distortion
     the 4-point fit can't model. Run it only after the canonical paths
     have failed (it adds real cost: ~2·qr_size centroid searches + an
     8×8 normal-equation solve + another 10-perturbation decode pass). */
  if (!decoded && gray && have_primary_H && version >= 2) {
    double H_cur[9];
    memcpy(H_cur, H_primary, sizeof(H_cur));
    /* Iterate refinement a few times — each pass re-predicts timing-
       module positions through the currently best H, re-observes
       centroids in the pixel buffer, and re-fits. Three rounds is
       usually enough to converge; diminishing returns beyond that. */
    for (int iter = 0; iter < 3 && !decoded; iter++) {
      double H_ref[9];
      if (!refine_h_with_timing(
            gray,
            width,
            height,
            H_cur,
            qr_size,
            avg_m,
            tl,
            tr,
            bl,
            have_alignment,
            alignment_x,
            alignment_y,
            H_ref
          )) {
        break;
      }
      memcpy(H_cur, H_ref, sizeof(H_cur));
      int fd = 16;
      int rc = 100000;
      char *d = decode_with_perturbation(
        pixels,
        gray,
        width,
        height,
        H_ref,
        qr_size,
        version,
        &fd,
        &rc
      );
      if (d) {
        decoded = d;
        fmt_dist = fd;
        rs_cost = rc;
      }
    }

    /* Last-resort attempt: quadratic (polynomial) warp sampling. When
       every homography we can build still fails to decode, the surface
       itself isn't planar — typical case is a QR printed on a curled
       receipt. Fit a bivariate degree-2 polynomial to 3 finders +
       alignment + timing observations and sample the grid with it. The
       quadratic warp can follow a cylindrical bend that no single
       homography can model. Only used when the refined H's decode
       failed, so the added cost is bounded to the hardest images. */
    if (!decoded) {
      int fd = 16;
      int rc = 100000;
      char *d = try_decode_quadratic(
        pixels,
        gray,
        width,
        height,
        qr_size,
        version,
        tl,
        tr,
        bl,
        have_alignment,
        alignment_x,
        alignment_y,
        H_cur,
        avg_m,
        &fd,
        &rc
      );
      if (d) {
        decoded = d;
        fmt_dist = fd;
        rs_cost = rc;
      }
    }
  }

  if (!decoded) {
    return NULL;
  }
  if (out_fmt_dist) {
    *out_fmt_dist = fmt_dist;
  }
  if (out_rs_cost) {
    *out_rs_cost = rs_cost;
  }

  {
    /* Compute QR code outer corners from finder pattern centers.
       Finder centers sit at module (3.5, 3.5) from their corners. */
    if (out_corners) {
      float ux = (tr.x - tl.x) / (qr_size - 7.0f);
      float uy = (tr.y - tl.y) / (qr_size - 7.0f);
      float vx = (bl.x - tl.x) / (qr_size - 7.0f);
      float vy = (bl.y - tl.y) / (qr_size - 7.0f);
      out_corners->tl_x = tl.x - 3.5f * (ux + vx);
      out_corners->tl_y = tl.y - 3.5f * (uy + vy);
      out_corners->tr_x = tr.x + 3.5f * (ux - vx);
      out_corners->tr_y = tr.y + 3.5f * (uy - vy);
      float br_center_x = tr.x + bl.x - tl.x;
      float br_center_y = tr.y + bl.y - tl.y;
      out_corners->br_x = br_center_x + 3.5f * (ux + vx);
      out_corners->br_y = br_center_y + 3.5f * (uy + vy);
      out_corners->bl_x = bl.x - 3.5f * (ux - vx);
      out_corners->bl_y = bl.y - 3.5f * (uy - vy);
    }
    if (out_finders) {
      out_finders[0].x = tl.x;
      out_finders[0].y = tl.y;
      out_finders[1].x = tr.x;
      out_finders[1].y = tr.y;
      out_finders[2].x = bl.x;
      out_finders[2].y = bl.y;
    }
    if (out_module_size) {
      float ux = (tr.x - tl.x) / (qr_size - 7.0f);
      float uy = (tr.y - tl.y) / (qr_size - 7.0f);
      float vx = (bl.x - tl.x) / (qr_size - 7.0f);
      float vy = (bl.y - tl.y) / (qr_size - 7.0f);
      float mu = sqrtf(ux * ux + uy * uy);
      float mv = sqrtf(vx * vx + vy * vy);
      *out_module_size = (mu + mv) / 2.0f;
    }
  }
  return decoded;
}

/* ---- Main API ---- */

/* Run the finder + decode pipeline on a single binary buffer.
   best_* are initialized by the caller and refined across multiple attempts
   (e.g. one attempt per binarization strategy). gray is passed for
   subpixel finder-center refinement. */
static void try_pipeline(
  uint8_t const *bin,
  uint8_t const *gray,
  int w,
  int h,
  char **best_decoded,
  int *best_fmt_dist,
  int *best_rs_cost,
  Corners *best_corners,
  Point2D best_finders[3],
  double *best_module_size
) {
  FinderPattern fps[MAX_CANDIDATES];
  int nfp = find_finders(bin, w, h, fps, MAX_CANDIDATES);
  if (nfp < 3) {
    return;
  }
  for (int i = 0; i < nfp; i++) {
    refine_finder_center(gray, w, h, &fps[i]);
  }

  FinderPattern triples[MAX_TRIPLES][3];
  int n_triples = select_and_rank(bin, w, h, fps, nfp, triples, MAX_TRIPLES);

  for (int t = 0; t < n_triples; t++) {
    int fmt_dist = 16;
    int rs_cost = 100000;
    Corners corners = {0};
    Point2D finders[3];
    double module_size = 0;
    char *decoded = try_decode_triple(
      bin,
      gray,
      w,
      h,
      triples[t][0],
      triples[t][1],
      triples[t][2],
      &fmt_dist,
      &rs_cost,
      &corners,
      finders,
      &module_size
    );
    if (decoded) {
      /* Composite score: lower fmt_dist wins, tie-break first on decoded
         length (longer decodes mean a larger byte-mode count field
         survived), then on Reed-Solomon cost (lower = the data block
         decoded with fewer error corrections, which discriminates between
         two "Mz4Y9X" candidates where one has clean RS and the other is
         RS-uncorrectable but happens to share the same fmt_dist). */
      int dlen = (int)strlen(decoded);
      int replace = 0;
      if (fmt_dist < *best_fmt_dist) {
        replace = 1;
      }
      else if (fmt_dist == *best_fmt_dist && *best_decoded != NULL) {
        int prev_len = (int)strlen(*best_decoded);
        if (dlen > prev_len) {
          replace = 1;
        }
        else if (dlen == prev_len && rs_cost < *best_rs_cost) {
          replace = 1;
        }
      }
      if (replace) {
        free(*best_decoded);
        *best_decoded = decoded;
        *best_fmt_dist = fmt_dist;
        *best_rs_cost = rs_cost;
        *best_corners = corners;
        for (int i = 0; i < 3; i++) {
          best_finders[i] = finders[i];
        }
        *best_module_size = module_size;
      }
      else {
        free(decoded);
      }
    }
  }
}

/* Runs the full binarizer/preprocessor cascade against `gray_pixels`. The
   sequence is cheapest-first; each attempt short-circuits once a
   high-confidence decode (fmt_dist <= 1, length >= 5) is in hand, so the
   work only expands as far as the image demands. Called by the public
   decoder at each pyramid level and once again on the inverted buffer —
   which lets light-on-dark QRs (level_13) ride the same Sauvola/upsample/
   median passes the normal-polarity pass has. `is_finest_level` gates the
   native-resolution-only passes (2x/3x upsample), which exist to rescue
   tiny QRs at native scale; upsampling from a downsampled pyramid level
   is strictly worse than running those passes at level 0. */
static void run_all_attempts(
  uint8_t const *gray_pixels,
  int w,
  int h,
  int is_finest_level,
  char **best_decoded,
  int *best_fmt_dist,
  int *best_rs_cost,
  Corners *best_corners,
  Point2D best_finders[3],
  double *best_module_size
) {
  /* Attempt 1: Otsu on original. */
  uint8_t *bin = binarize_global_otsu(gray_pixels, w, h);
  if (bin) {
    try_pipeline(
      bin,
      gray_pixels,
      w,
      h,
      best_decoded,
      best_fmt_dist,
      best_rs_cost,
      best_corners,
      best_finders,
      best_module_size
    );
    free(bin);
  }

  /* Attempt 2: zxing-cpp hybrid on original. Sparse-threshold per 8x8 block
     plus 5x5 neighbour smoothing — low-contrast interior blocks inherit a
     threshold from contrasty edge blocks instead of falling back to a
     whole-image Otsu. Primary recovery path for faded / pastel palettes. */
  if (!(*best_decoded && *best_fmt_dist <= 1 && strlen(*best_decoded) >= 5)) {
    bin = binarize_hybrid(gray_pixels, w, h);
    if (bin) {
      try_pipeline(
        bin,
        gray_pixels,
        w,
        h,
        best_decoded,
        best_fmt_dist,
        best_rs_cost,
        best_corners,
        best_finders,
        best_module_size
      );
      free(bin);
    }
  }

  /* Attempt 3: adaptive on original. */
  if (!(*best_decoded && *best_fmt_dist <= 1 && strlen(*best_decoded) >= 5)) {
    bin = binarize_adaptive(gray_pixels, w, h);
    if (bin) {
      try_pipeline(
        bin,
        gray_pixels,
        w,
        h,
        best_decoded,
        best_fmt_dist,
        best_rs_cost,
        best_corners,
        best_finders,
        best_module_size
      );
      free(bin);
    }
  }

  /* The remaining attempts (Sauvola, unsharp, upsample) target large,
     visually-degraded photographs. On tiny synthetic buffers they are more
     likely to hallucinate a decode from noise than to recover a real QR
     — anything smaller than ~100 px on the short side cannot plausibly
     hold a decodable v1 QR (≥ ~5 px/module × 21 modules + quiet zone). */
  int small_image = (w < 100 || h < 100);

  /* Attempt 3: Sauvola on original. Targets faded / low-contrast modules
     where adaptive's fixed min_std fallback misclassifies. */
  if (!small_image &&
      !(*best_decoded && *best_fmt_dist <= 1 && strlen(*best_decoded) >= 5)) {
    bin = binarize_sauvola(gray_pixels, w, h);
    if (bin) {
      try_pipeline(
        bin,
        gray_pixels,
        w,
        h,
        best_decoded,
        best_fmt_dist,
        best_rs_cost,
        best_corners,
        best_finders,
        best_module_size
      );
      free(bin);
    }
  }

  /* Attempt 4: unsharp mask + Otsu, then unsharp mask + adaptive. Targets
     blurred QRs (level_4/5 generators add Gaussian blur radius up to 1.8). */
  if (!small_image &&
      !(*best_decoded && *best_fmt_dist <= 1 && strlen(*best_decoded) >= 5)) {
    uint8_t *sharp = unsharp_mask_gray(gray_pixels, w, h);
    if (sharp) {
      bin = binarize_global_otsu(sharp, w, h);
      if (bin) {
        try_pipeline(
          bin,
          sharp,
          w,
          h,
          best_decoded,
          best_fmt_dist,
          best_rs_cost,
          best_corners,
          best_finders,
          best_module_size
        );
        free(bin);
      }
      if (!(*best_decoded && *best_fmt_dist <= 1 && strlen(*best_decoded) >= 5
          )) {
        bin = binarize_adaptive(sharp, w, h);
        if (bin) {
          try_pipeline(
            bin,
            sharp,
            w,
            h,
            best_decoded,
            best_fmt_dist,
            best_rs_cost,
            best_corners,
            best_finders,
            best_module_size
          );
          free(bin);
        }
      }
      free(sharp);
    }
  }

  /* Attempt 5: 2x bilinear upsample + Otsu, then upsample + adaptive.
     Targets tiny QRs whose modules are too small for finder ratio checks
     at native resolution. Coordinates from any winning upsampled decode are
     scaled back by 1/n before returning. Attempt 5b (3x) follows when 2x
     still doesn't yield a confident decode; helpful for QRs with mod < 3 px
     where even doubled modules remain on the edge of the ratio tolerance.
     Skipped at coarser pyramid levels: upsampling a downsampled buffer is
     strictly worse than running this at level 0 on the original data. */
  for (int n_try = 2; n_try <= 3; n_try++) {
    if (small_image || !is_finest_level) {
      break;
    }
    /* Skip the upscaled passes when we already have a long, perfect-format
       decode at the native scale; otherwise always try, because a tiny QR
       (modules < 4 px) cannot reliably decode at native resolution and a
       length-5 wrong decode (e.g. "Mz4Y9c" with fd=0 + uncorrectable RS)
       must not block 3x exploration that may surface "Mz4Y9a". */
    if (*best_decoded && *best_fmt_dist == 0 && strlen(*best_decoded) >= 10) {
      break;
    }
    int wn = w * n_try;
    int hn = h * n_try;
    uint8_t *grayn = upsample_nx_bilinear(gray_pixels, w, h, n_try);
    if (!grayn) {
      continue;
    }
    char *best_decoded_up = NULL;
    Corners best_corners_up = {0};
    Point2D best_finders_up[3] = {{0, 0}, {0, 0}, {0, 0}};
    double best_module_size_up = 0;
    int best_fmt_dist_up = 16;
    int best_rs_cost_up = 100000;

    bin = binarize_global_otsu(grayn, wn, hn);
    if (bin) {
      try_pipeline(
        bin,
        grayn,
        wn,
        hn,
        &best_decoded_up,
        &best_fmt_dist_up,
        &best_rs_cost_up,
        &best_corners_up,
        best_finders_up,
        &best_module_size_up
      );
      free(bin);
    }
    if (!(best_decoded_up && best_fmt_dist_up <= 1)) {
      bin = binarize_adaptive(grayn, wn, hn);
      if (bin) {
        try_pipeline(
          bin,
          grayn,
          wn,
          hn,
          &best_decoded_up,
          &best_fmt_dist_up,
          &best_rs_cost_up,
          &best_corners_up,
          best_finders_up,
          &best_module_size_up
        );
        free(bin);
      }
    }
    free(grayn);

    /* Promote the upsampled result if it improves on what we already have:
       lower fmt_dist wins, tie-break on longer decode length, then on
       lower RS cost. Mirrors the scoring inside try_pipeline. Scale
       spatial outputs back to native coordinates by 1/n. */
    if (best_decoded_up) {
      int replace = 0;
      int prev_len = (*best_decoded) ? (int)strlen(*best_decoded) : -1;
      int new_len = (int)strlen(best_decoded_up);
      if (!*best_decoded) {
        replace = 1;
      }
      else if (best_fmt_dist_up < *best_fmt_dist) {
        replace = 1;
      }
      else if (best_fmt_dist_up == *best_fmt_dist && new_len > prev_len) {
        replace = 1;
      }
      else if (best_fmt_dist_up == *best_fmt_dist && new_len == prev_len &&
               best_rs_cost_up < *best_rs_cost) {
        replace = 1;
      }
      if (replace) {
        double inv = 1.0 / (double)n_try;
        free(*best_decoded);
        *best_decoded = best_decoded_up;
        *best_fmt_dist = best_fmt_dist_up;
        *best_rs_cost = best_rs_cost_up;
        best_corners->tl_x = best_corners_up.tl_x * inv;
        best_corners->tl_y = best_corners_up.tl_y * inv;
        best_corners->tr_x = best_corners_up.tr_x * inv;
        best_corners->tr_y = best_corners_up.tr_y * inv;
        best_corners->br_x = best_corners_up.br_x * inv;
        best_corners->br_y = best_corners_up.br_y * inv;
        best_corners->bl_x = best_corners_up.bl_x * inv;
        best_corners->bl_y = best_corners_up.bl_y * inv;
        for (int i = 0; i < 3; i++) {
          best_finders[i].x = best_finders_up[i].x * inv;
          best_finders[i].y = best_finders_up[i].y * inv;
        }
        *best_module_size = best_module_size_up * inv;
      }
      else {
        free(best_decoded_up);
      }
    }
  }

  /* Attempt 6: 3x3 median filter + Otsu, then median + adaptive. Median
     denoising preserves edges while removing salt-and-pepper noise that
     defeats both finder-ratio checks and module-center voting. Especially
     helpful for level_5 images (sigma 4-9 noise + low contrast). Runs
     when nothing decoded OR the existing decode is suspiciously short
     (a count-field truncation of byte mode); a long decode from earlier
     is trusted and we don't risk displacing it. */
  int existing_short = *best_decoded && strlen(*best_decoded) < 8;
  if (!small_image && (!*best_decoded || existing_short)) {
    uint8_t *med = median_filter_3x3(gray_pixels, w, h);
    if (med) {
      bin = binarize_global_otsu(med, w, h);
      if (bin) {
        try_pipeline(
          bin,
          med,
          w,
          h,
          best_decoded,
          best_fmt_dist,
          best_rs_cost,
          best_corners,
          best_finders,
          best_module_size
        );
        free(bin);
      }
      if (!(*best_decoded && *best_fmt_dist <= 1 && strlen(*best_decoded) >= 5
          )) {
        bin = binarize_adaptive(med, w, h);
        if (bin) {
          try_pipeline(
            bin,
            med,
            w,
            h,
            best_decoded,
            best_fmt_dist,
            best_rs_cost,
            best_corners,
            best_finders,
            best_module_size
          );
          free(bin);
        }
      }
      free(med);
    }
  }
}

/* ---- Pyramid-level decode helpers ---- */

/* Scales all spatial fields of a decode state by `scale`. Used to convert
   coordinates from a downsampled pyramid level back into original-image
   pixel space. */
static void scale_decode_state(
  Corners *corners,
  Point2D finders[3],
  double *module_size,
  double scale
) {
  corners->tl_x *= scale;
  corners->tl_y *= scale;
  corners->tr_x *= scale;
  corners->tr_y *= scale;
  corners->br_x *= scale;
  corners->br_y *= scale;
  corners->bl_x *= scale;
  corners->bl_y *= scale;
  for (int i = 0; i < 3; i++) {
    finders[i].x *= scale;
    finders[i].y *= scale;
  }
  *module_size *= scale;
}

/* Returns 1 if (cand_*) should replace (cur_*). Ranking is: lower fmt_dist
   first, then longer decoded string, then lower RS cost. Matches the
   promotion logic previously inlined in the upsample / downsample blocks. */
static int decode_is_better(
  char const *cur,
  int cur_fmt,
  int cur_rs,
  char const *cand,
  int cand_fmt,
  int cand_rs
) {
  if (!cand) {
    return 0;
  }
  if (!cur) {
    return 1;
  }
  if (cand_fmt != cur_fmt) {
    return cand_fmt < cur_fmt;
  }
  int cur_len = (int)strlen(cur);
  int cand_len = (int)strlen(cand);
  if (cand_len != cur_len) {
    return cand_len > cur_len;
  }
  return cand_rs < cur_rs;
}

FCVQRCodeResult fcv_decode_qr_codes(
  uint32_t width,
  uint32_t height,
  uint8_t const *const gray_pixels
) {
  FCVQRCodeResult result;
  result.codes = NULL;
  result.count = 0;

  if (!gray_pixels || width == 0 || height == 0) {
    return result;
  }
  if (width > INT32_MAX || height > INT32_MAX) {
    return result;
  }

  result.codes = malloc(MAX_QR_CODES * sizeof(FCVQRCode));
  if (!result.codes) {
    return result;
  }

  int w = (int)width;
  int h = (int)height;

  /* ---- Build a box-averaged pyramid ----
     Level 0 is the original; level k+1 is a 2x2 box-average of level k.
     We stop halving once the short side would drop below MIN_SHORT_SIDE,
     below which even a version-1 QR (21 modules) can't plausibly survive
     binarization (<6 px/module after quiet zone). */
#define QR_PYRAMID_MAX 6
#define QR_PYRAMID_MIN_SHORT 120
#define QR_PYRAMID_TARGET_SHORT 640

  uint8_t const *lv_px[QR_PYRAMID_MAX];
  int lv_w[QR_PYRAMID_MAX], lv_h[QR_PYRAMID_MAX];
  int lv_owned[QR_PYRAMID_MAX];

  lv_px[0] = gray_pixels;
  lv_w[0] = w;
  lv_h[0] = h;
  lv_owned[0] = 0;
  int n_levels = 1;
  while (n_levels < QR_PYRAMID_MAX) {
    int nw = lv_w[n_levels - 1] / 2;
    int nh = lv_h[n_levels - 1] / 2;
    int short_side = nw < nh ? nw : nh;
    if (short_side < QR_PYRAMID_MIN_SHORT) {
      break;
    }
    uint8_t *down = downsample_2x_box(
      lv_px[n_levels - 1],
      lv_w[n_levels - 1],
      lv_h[n_levels - 1]
    );
    if (!down) {
      break;
    }
    lv_px[n_levels] = down;
    lv_w[n_levels] = nw;
    lv_h[n_levels] = nh;
    lv_owned[n_levels] = 1;
    n_levels++;
  }

  /* Target level: the coarsest (highest-index) level whose short side is
     still >= TARGET. Decouples first-pass work from sensor resolution:
     the common case of "one QR filling a large fraction of a phone photo"
     gets scanned at ~640 px regardless of whether the source is 1 MP or
     48 MP. If no level meets the target (small input), target stays 0. */
  int target = 0;
  for (int i = 1; i < n_levels; i++) {
    int short_side = lv_w[i] < lv_h[i] ? lv_w[i] : lv_h[i];
    if (short_side >= QR_PYRAMID_TARGET_SHORT) {
      target = i;
    }
    else {
      break;
    }
  }

  char *best_decoded = NULL;
  Corners best_corners = {0};
  Point2D best_finders[3] = {{0, 0}, {0, 0}, {0, 0}};
  double best_module_size = 0;
  int best_fmt_dist = 16;
  int best_rs_cost = 100000;

  /* Visit order: target first (best balance of detail vs. noise for the
     typical "one QR filling most of a phone photo" case), then coarser
     levels (cheap, different binarizer response — catches level_13's
     low-contrast pastel-on-checkered case the old inline 2x-downsample
     attempt used to handle), then finer levels (expensive, last resort
     for tiny QRs). Stop as soon as a confident decode (fmt_dist <= 1,
     length >= 5) is in hand. Non-confident decodes are kept as best-so-
     far and may be displaced by a cleaner result from another level. */
  int visit_order[QR_PYRAMID_MAX];
  int n_visits = 0;
  visit_order[n_visits++] = target;
  for (int lvl = target + 1; lvl < n_levels; lvl++) {
    visit_order[n_visits++] = lvl;
  }
  for (int lvl = target - 1; lvl >= 0; lvl--) {
    visit_order[n_visits++] = lvl;
  }

  for (int vi = 0; vi < n_visits; vi++) {
    int lvl = visit_order[vi];
    double scale = (double)(1 << lvl);
    int is_finest = (lvl == 0);
    int lw = lv_w[lvl];
    int lh = lv_h[lvl];
    uint8_t const *lpx = lv_px[lvl];

    char *decoded = NULL;
    Corners corners = {0};
    Point2D finders[3] = {{0, 0}, {0, 0}, {0, 0}};
    double module_size = 0;
    int fmt_dist = 16;
    int rs_cost = 100000;

    run_all_attempts(
      lpx,
      lw,
      lh,
      is_finest,
      &decoded,
      &fmt_dist,
      &rs_cost,
      &corners,
      finders,
      &module_size
    );

    /* Inverted-polarity retry at this level — handles light-on-dark QRs.
       Only fires if nothing at this level has decoded confidently.
       Confidence = fmt_dist <= 1 (format info BCH can correct 1-bit errors
       in its 15-bit code, so a 0-or-1-bit fmt_dist reliably indicates a
       real QR). Length is not a trust signal: legitimate v1 QRs routinely
       encode 2-4 characters. */
    if (!(decoded && fmt_dist <= 1)) {
      uint8_t *inv = invert_gray(lpx, lw, lh);
      if (inv) {
        run_all_attempts(
          inv,
          lw,
          lh,
          is_finest,
          &decoded,
          &fmt_dist,
          &rs_cost,
          &corners,
          finders,
          &module_size
        );
        free(inv);
      }
    }

    if (decoded) {
      scale_decode_state(&corners, finders, &module_size, scale);
      if (decode_is_better(
            best_decoded,
            best_fmt_dist,
            best_rs_cost,
            decoded,
            fmt_dist,
            rs_cost
          )) {
        free(best_decoded);
        best_decoded = decoded;
        best_fmt_dist = fmt_dist;
        best_rs_cost = rs_cost;
        best_corners = corners;
        for (int i = 0; i < 3; i++) {
          best_finders[i] = finders[i];
        }
        best_module_size = module_size;
      }
      else {
        free(decoded);
      }
    }

    if (best_decoded && best_fmt_dist <= 1) {
      break;
    }
  }

  for (int i = 1; i < n_levels; i++) {
    if (lv_owned[i]) {
      free((uint8_t *)lv_px[i]);
    }
  }

#undef QR_PYRAMID_MAX
#undef QR_PYRAMID_MIN_SHORT
#undef QR_PYRAMID_TARGET_SHORT

  if (best_decoded) {
    FCVQRCode *qr = &result.codes[result.count++];
    qr->text = best_decoded;
    qr->corners = best_corners;
    for (int i = 0; i < 3; i++) {
      qr->finders[i] = best_finders[i];
    }
    qr->module_size = best_module_size;
  }

  return result;
}

void fcv_free_qr_result(FCVQRCodeResult result) {
  if (result.codes) {
    for (size_t i = 0; i < result.count; i++) {
      free(result.codes[i].text);
    }
    free(result.codes);
  }
}
