#include <math.h>
#include <stddef.h>
#include <stdint.h>
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

static int select_and_rank(
  FinderPattern *fps,
  int nfp,
  FinderPattern triples[][3],
  int max_triples
) {
  if (nfp < 3) {
    return 0;
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

  int limit = (nfp > 15) ? 15 : nfp;
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

        float score = side_ratio - ms_var * 2.0f - hyp_err * 2.0f;

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

/* ---- Grid sampling via affine transform ---- */

static uint8_t *sample_grid(
  uint8_t const *px,
  int w,
  int h,
  FinderPattern tl,
  FinderPattern tr,
  FinderPattern bl,
  int qr_size
) {
  uint8_t *grid = calloc(qr_size * qr_size, 1);
  if (!grid) {
    return NULL;
  }
  float sx0 = 3.5f, sy0 = 3.5f;
  float sx1 = qr_size - 3.5f, sy1 = 3.5f;
  float sx2 = 3.5f, sy2 = qr_size - 3.5f;
  float denom = (sx0 - sx2) * (sy1 - sy2) - (sx1 - sx2) * (sy0 - sy2);
  if (fabsf(denom) < 1e-6f) {
    free(grid);
    return NULL;
  }
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
  for (int r = 0; r < qr_size; r++) {
    for (int c = 0; c < qr_size; c++) {
      float mx = c + 0.5f, my = r + 0.5f;
      int px_ = (int)(a11 * mx + a12 * my + b1);
      int py_ = (int)(a21 * mx + a22 * my + b2);
      grid[r * qr_size + c] = is_dark(px, w, h, px_, py_) ? 1 : 0;
    }
  }
  return grid;
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
  int sz,
  int ver,
  int mask,
  uint8_t *bits,
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
          bits[n++] = (uint8_t)val;
        }
      }
    }
    up = !up;
  }
  return n;
}

/* ---- Bits to bytes ---- */

static int bits_to_bytes(uint8_t const *bits, int nbits, uint8_t *bytes) {
  int nb = nbits / 8;
  for (int i = 0; i < nb; i++) {
    uint8_t b = 0;
    for (int j = 0; j < 8; j++) {
      b = (b << 1) | bits[i * 8 + j];
    }
    bytes[i] = b;
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

/* ---- De-interleave data codewords ---- */

static int deinterleave(
  uint8_t const *raw,
  int raw_len,
  const ECInfo *info,
  uint8_t *out
) {
  int total_blocks = info->g1_blocks + info->g2_blocks;
  if (total_blocks == 0 || total_blocks > 100) {
    return 0;
  }
  int total_data =
    info->g1_blocks * info->g1_data_cw + info->g2_blocks * info->g2_data_cw;
  if (total_data > raw_len) {
    total_data = raw_len;
  }

  if (total_blocks == 1) {
    int n = total_data < raw_len ? total_data : raw_len;
    memcpy(out, raw, n);
    return n;
  }

  int max_data_cw = (info->g2_blocks > 0) ? info->g2_data_cw : info->g1_data_cw;
  int pos = 0, out_pos = 0;

  uint8_t **blocks = calloc(total_blocks, sizeof(uint8_t *));
  int *block_len = calloc(total_blocks, sizeof(int));
  if (!blocks || !block_len) {
    free(blocks);
    free(block_len);
    return 0;
  }

  for (int b = 0; b < total_blocks; b++) {
    int cw = (b < info->g1_blocks) ? info->g1_data_cw : info->g2_data_cw;
    blocks[b] = calloc(cw, 1);
    block_len[b] = cw;
  }

  pos = 0;
  for (int i = 0; i < max_data_cw && pos < raw_len; i++) {
    for (int b = 0; b < total_blocks && pos < raw_len; b++) {
      if (i < block_len[b]) {
        blocks[b][i] = raw[pos++];
      }
    }
  }

  out_pos = 0;
  for (int b = 0; b < total_blocks; b++) {
    for (int i = 0; i < block_len[b]; i++) {
      out[out_pos++] = blocks[b][i];
    }
    free(blocks[b]);
  }
  free(blocks);
  free(block_len);
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
  if (count <= 0 || count > 4296) {
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

/* ---- Try to decode a single QR code from a triple ---- */

static char *try_decode_triple(
  uint8_t const *pixels,
  int width,
  int height,
  FinderPattern tl,
  FinderPattern tr,
  FinderPattern bl,
  int *out_fmt_dist,
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

  uint8_t *grid = sample_grid(pixels, width, height, tl, tr, bl, qr_size);
  if (!grid) {
    return NULL;
  }

  int ec_level, mask_pattern;
  int fmt_dist = 16;
  int fmt_ok =
    decode_format(grid, qr_size, &ec_level, &mask_pattern, &fmt_dist);
  if (fmt_ok < 0) {
    free(grid);
    return NULL;
  }
  if (out_fmt_dist) {
    *out_fmt_dist = fmt_dist;
  }

  int max_bits = qr_size * qr_size;
  uint8_t *data_bits = malloc(max_bits);
  if (!data_bits) {
    free(grid);
    return NULL;
  }

  int nbits =
    extract_bits(grid, qr_size, version, mask_pattern, data_bits, max_bits);
  free(grid);

  int max_bytes = nbits / 8;
  uint8_t *raw_bytes = malloc(max_bytes + 1);
  if (!raw_bytes) {
    free(data_bits);
    return NULL;
  }
  int nraw = bits_to_bytes(data_bits, nbits, raw_bytes);
  free(data_bits);

  int data_cw_count = nraw;
  uint8_t *data_bytes = raw_bytes;
  uint8_t *deint_buf = NULL;

  if (version >= 1 && version <= 40 && ec_level >= 0 && ec_level <= 3) {
    const ECInfo *info = &EC_TABLE[version - 1][ec_level];
    int total_data =
      info->g1_blocks * info->g1_data_cw + info->g2_blocks * info->g2_data_cw;
    if (total_data > 0 && total_data <= nraw) {
      int total_blocks = info->g1_blocks + info->g2_blocks;
      if (total_blocks > 1) {
        deint_buf = malloc(total_data + 1);
        if (deint_buf) {
          data_cw_count = deinterleave(raw_bytes, nraw, info, deint_buf);
          data_bytes = deint_buf;
        }
      }
      else {
        data_cw_count = total_data;
      }
    }
  }

  char *decoded = decode_payload(data_bytes, data_cw_count, version);
  free(raw_bytes);
  free(deint_buf);

  /* Sanity check: reject decodes with non-printable characters */
  if (decoded) {
    int dlen = (int)strlen(decoded);
    for (int i = 0; i < dlen; i++) {
      uint8_t ch = (uint8_t)decoded[i];
      if (ch < 0x20 || ch > 0x7e) {
        free(decoded);
        return NULL;
      }
    }
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

  FinderPattern fps[MAX_CANDIDATES];
  int nfp = find_finders(gray_pixels, w, h, fps, MAX_CANDIDATES);

  if (nfp < 3) {
    return result;
  }

  FinderPattern triples[MAX_TRIPLES][3];
  int n_triples = select_and_rank(fps, nfp, triples, MAX_TRIPLES);

  char *best_decoded = NULL;
  Corners best_corners = {0};
  Point2D best_finders[3] = {{0, 0}, {0, 0}, {0, 0}};
  double best_module_size = 0;
  int best_fmt_dist = 16;

  for (int t = 0; t < n_triples; t++) {
    int fmt_dist = 16;
    Corners corners = {0};
    Point2D finders[3];
    double module_size = 0;
    char *decoded = try_decode_triple(
      gray_pixels,
      w,
      h,
      triples[t][0],
      triples[t][1],
      triples[t][2],
      &fmt_dist,
      &corners,
      finders,
      &module_size
    );
    if (decoded) {
      if (fmt_dist < best_fmt_dist) {
        free(best_decoded);
        best_decoded = decoded;
        best_fmt_dist = fmt_dist;
        best_corners = corners;
        for (int i = 0; i < 3; i++) {
          best_finders[i] = finders[i];
        }
        best_module_size = module_size;
        if (fmt_dist == 0) {
          break; /* perfect match */
        }
      }
      else {
        free(decoded);
      }
    }
  }

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
