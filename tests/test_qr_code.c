#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

#include "qr_code.h"

static int test_failures = 0;

static int test_null_pixels(void) {
  printf("Test: Null pixels...");
  FCVQRCodeResult result = fcv_decode_qr_codes(100, 100, NULL);
  if (result.count != 0 || result.codes != NULL) {
    printf(" FAILED\n");
    fcv_free_qr_result(result);
    return 1;
  }
  fcv_free_qr_result(result);
  printf(" OK\n");
  return 0;
}

static int test_zero_dimensions(void) {
  printf("Test: Zero dimensions...");
  uint8_t pixels[100] = {0};
  FCVQRCodeResult result = fcv_decode_qr_codes(0, 0, pixels);
  if (result.count != 0 || result.codes != NULL) {
    printf(" FAILED\n");
    fcv_free_qr_result(result);
    return 1;
  }
  fcv_free_qr_result(result);
  printf(" OK\n");
  return 0;
}

static int test_solid_images(void) {
  printf("Test: Solid white/black/stripe images (no QR)...");
  const uint32_t w = 64;
  const uint32_t h = 64;
  uint8_t *buf = malloc(w * h);
  if (!buf) {
    printf(" FAILED (alloc)\n");
    return 1;
  }

  memset(buf, 255, w * h);
  FCVQRCodeResult r1 = fcv_decode_qr_codes(w, h, buf);
  int ok = (r1.count == 0);
  fcv_free_qr_result(r1);

  memset(buf, 0, w * h);
  FCVQRCodeResult r2 = fcv_decode_qr_codes(w, h, buf);
  ok = ok && (r2.count == 0);
  fcv_free_qr_result(r2);

  for (uint32_t i = 0; i < w * h; i++) {
    buf[i] = (i % 2 == 0) ? 0 : 255;
  }
  FCVQRCodeResult r3 = fcv_decode_qr_codes(w, h, buf);
  ok = ok && (r3.count == 0);
  fcv_free_qr_result(r3);

  free(buf);
  if (!ok) {
    printf(" FAILED\n");
    return 1;
  }
  printf(" OK\n");
  return 0;
}

static int test_free_null_result(void) {
  printf("Test: Free null result...");
  FCVQRCodeResult result = {NULL, 0};
  fcv_free_qr_result(result);
  printf(" OK\n");
  return 0;
}

static int test_generated_images(void) {
  const char *dir_path = "tests/qr_codes_generated";
  printf("Test: Generated QR images (%s)...\n", dir_path);

  DIR *dir = opendir(dir_path);
  if (!dir) {
    printf("  SKIP: directory not found\n");
    return 0;
  }

  int passed = 0;
  int failed = 0;
  int total = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    const char *name = entry->d_name;
    size_t len = strlen(name);
    if (len < 5 || strcmp(name + len - 4, ".png") != 0) {
      continue;
    }

    /* Filename: <text>_<x1>-<y1>_<x2>-<y2>_<x3>-<y3>_<x4>-<y4>.png */
    char stem[512];
    size_t stem_len = len - 4;
    if (stem_len >= sizeof(stem)) {
      continue;
    }
    memcpy(stem, name, stem_len);
    stem[stem_len] = '\0';

    const char *text_end = stem + stem_len;
    int ok_parse = 1;
    for (int g = 0; g < 4; g++) {
      const char *p = text_end;
      while (p > stem && *(p - 1) != '_') {
        p--;
      }
      if (p <= stem) {
        ok_parse = 0;
        break;
      }
      text_end = p - 1;
    }
    if (!ok_parse) {
      continue;
    }

    char expected[256];
    size_t text_len = (size_t)(text_end - stem);
    if (text_len >= sizeof(expected)) {
      continue;
    }
    memcpy(expected, stem, text_len);
    expected[text_len] = '\0';

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir_path, name);

    int width, height, channels;
    uint8_t *img = stbi_load(path, &width, &height, &channels, 1);
    if (!img) {
      failed++;
      total++;
      printf("  FAIL %s: could not load\n", name);
      continue;
    }

    FCVQRCodeResult result = fcv_decode_qr_codes(width, height, img);
    total++;

    int match = 0;
    for (size_t j = 0; j < result.count; j++) {
      if (result.codes[j].text && strcmp(result.codes[j].text, expected) == 0) {
        match = 1;
        break;
      }
    }

    if (match) {
      passed++;
    }
    else {
      failed++;
      printf(
        "  FAIL %s: decoded %zu codes, expected \"%s\"",
        name,
        result.count,
        expected
      );
      if (result.count > 0 && result.codes[0].text) {
        printf(", got \"%s\"", result.codes[0].text);
      }
      printf("\n");
    }

    fcv_free_qr_result(result);
    stbi_image_free(img);
  }

  closedir(dir);
  printf(
    "  Generated results: %d/%d passed, %d failed\n",
    passed,
    total,
    failed
  );
  test_failures += failed;
  return failed;
}

typedef struct {
  char **entries;
  size_t count;
  size_t capacity;
  uint8_t *seen; /* parallel to entries: 1 once matched in this run */
} KnownFailureList;

static int known_failure_load(KnownFailureList *kfl, const char *path) {
  kfl->entries = NULL;
  kfl->count = 0;
  kfl->capacity = 0;
  kfl->seen = NULL;

  FILE *f = fopen(path, "r");
  if (!f) {
    return 0; /* missing file = no known failures */
  }

  char line[512];
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') {
      p++;
    }
    if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') {
      continue;
    }
    size_t len = strlen(p);
    while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r' ||
                       p[len - 1] == ' ' || p[len - 1] == '\t')) {
      p[--len] = '\0';
    }
    if (len == 0) {
      continue;
    }

    if (kfl->count == kfl->capacity) {
      size_t new_cap = kfl->capacity == 0 ? 16 : kfl->capacity * 2;
      char **ne = realloc(kfl->entries, new_cap * sizeof(*ne));
      uint8_t *ns = realloc(kfl->seen, new_cap * sizeof(*ns));
      if (!ne || !ns) {
        fclose(f);
        return -1;
      }
      kfl->entries = ne;
      kfl->seen = ns;
      kfl->capacity = new_cap;
    }
    kfl->entries[kfl->count] = strdup(p);
    kfl->seen[kfl->count] = 0;
    kfl->count++;
  }
  fclose(f);
  return 0;
}

static int known_failure_check(KnownFailureList *kfl, const char *relpath) {
  for (size_t i = 0; i < kfl->count; i++) {
    if (strcmp(kfl->entries[i], relpath) == 0) {
      kfl->seen[i] = 1;
      return 1;
    }
  }
  return 0;
}

static void known_failure_free(KnownFailureList *kfl) {
  for (size_t i = 0; i < kfl->count; i++) {
    free(kfl->entries[i]);
  }
  free(kfl->entries);
  free(kfl->seen);
}

static int expected_text_from_filename(
  const char *name,
  size_t name_len,
  char *out,
  size_t out_size
) {
  /* Filename: <index>_<text>.png — split on the first '_'. */
  if (name_len < 5 || strcmp(name + name_len - 4, ".png") != 0) {
    return -1;
  }
  size_t stem_len = name_len - 4;
  const char *sep = NULL;
  for (size_t i = 0; i < stem_len; i++) {
    if (name[i] == '_') {
      sep = name + i;
      break;
    }
  }
  if (!sep || (size_t)(sep - name) >= stem_len - 1) {
    return -1;
  }
  size_t text_len = stem_len - (size_t)(sep - name) - 1;
  if (text_len == 0 || text_len >= out_size) {
    return -1;
  }
  memcpy(out, sep + 1, text_len);
  out[text_len] = '\0';
  return 0;
}

typedef struct {
  int total;
  int passed;
  int failed_expected; /* failed AND listed in known_failures */
  int failed_new;      /* failed AND NOT listed: regressions */
  int fixed;           /* listed but passed: candidates to remove */
} LevelStats;

static int run_difficulty_level(
  const char *root_dir,
  const char *level_name,
  KnownFailureList *kfl,
  LevelStats *stats
) {
  char dir_path[512];
  snprintf(dir_path, sizeof(dir_path), "%s/%s", root_dir, level_name);

  DIR *dir = opendir(dir_path);
  if (!dir) {
    printf("  %s: SKIP (directory missing)\n", level_name);
    return 0;
  }

  memset(stats, 0, sizeof(*stats));
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    const char *name = entry->d_name;
    size_t name_len = strlen(name);
    char expected[256];
    if (expected_text_from_filename(
          name,
          name_len,
          expected,
          sizeof(expected)
        ) != 0) {
      continue;
    }

    char relpath[512];
    snprintf(relpath, sizeof(relpath), "%s/%s", level_name, name);

    char full_path[768];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, name);

    int width, height, channels;
    uint8_t *img = stbi_load(full_path, &width, &height, &channels, 1);
    if (!img) {
      stats->total++;
      stats->failed_new++;
      printf("  %s: LOAD FAIL\n", relpath);
      continue;
    }

    FCVQRCodeResult result = fcv_decode_qr_codes(width, height, img);
    stats->total++;

    int match = 0;
    for (size_t j = 0; j < result.count; j++) {
      if (result.codes[j].text && strcmp(result.codes[j].text, expected) == 0) {
        match = 1;
        break;
      }
    }

    int is_known = known_failure_check(kfl, relpath);
    if (match) {
      stats->passed++;
      if (is_known) {
        stats->fixed++;
        printf("  %s: FIXED (remove from known_failures.txt)\n", relpath);
      }
    }
    else {
      if (is_known) {
        stats->failed_expected++;
      }
      else {
        stats->failed_new++;
        printf("  %s: REGRESSION — expected \"%s\"", relpath, expected);
        if (result.count > 0 && result.codes[0].text) {
          printf(", got \"%s\"", result.codes[0].text);
        }
        printf("\n");
      }
    }

    fcv_free_qr_result(result);
    stbi_image_free(img);
  }

  closedir(dir);
  return 0;
}

static int test_difficulty_images(void) {
  const char *root_dir = "tests/qr_codes_difficulty";
  const char *kfl_path = "tests/qr_codes_difficulty_known_failures.txt";
  const char *levels[] = {
    "level_1",  "level_2",  "level_3",  "level_4",  "level_5",
    "level_6",  "level_7",  "level_8",  "level_9",  "level_10",
    "level_11", "level_12", "level_13", "level_14", "level_15",
    "level_16", "level_17", "level_18", "level_19", "level_20",
  };
  const size_t num_levels = sizeof(levels) / sizeof(levels[0]);

  printf("Test: Difficulty-tiered QR images (%s)...\n", root_dir);

  DIR *dir = opendir(root_dir);
  if (!dir) {
    printf("  SKIP: directory not found\n");
    return 0;
  }
  closedir(dir);

  KnownFailureList kfl;
  if (known_failure_load(&kfl, kfl_path) < 0) {
    printf("  FAILED to load %s\n", kfl_path);
    return 1;
  }

  LevelStats totals = {0};
  int regression_count = 0;
  int fixed_count = 0;

  for (size_t i = 0; i < num_levels; i++) {
    LevelStats ls;
    run_difficulty_level(root_dir, levels[i], &kfl, &ls);
    totals.total += ls.total;
    totals.passed += ls.passed;
    totals.failed_expected += ls.failed_expected;
    totals.failed_new += ls.failed_new;
    totals.fixed += ls.fixed;
    regression_count += ls.failed_new;
    fixed_count += ls.fixed;

    if (ls.total > 0) {
      double rate = 100.0 * (double)ls.passed / (double)ls.total;
      printf(
        "  %s: %d/%d passed (%.1f%%), %d known-failing\n",
        levels[i],
        ls.passed,
        ls.total,
        rate,
        ls.failed_expected
      );
    }
  }

  /* Any known-failure entry that was never matched (because its image
   * no longer exists) is stale and should be removed. */
  int stale_count = 0;
  for (size_t i = 0; i < kfl.count; i++) {
    if (!kfl.seen[i]) {
      printf(
        "  STALE: %s listed in known_failures.txt but not found\n",
        kfl.entries[i]
      );
      stale_count++;
    }
  }

  printf(
    "  Difficulty total: %d/%d passed, %d known-failing, %d new regressions, "
    "%d fixed, %d stale\n",
    totals.passed,
    totals.total,
    totals.failed_expected,
    regression_count,
    fixed_count,
    stale_count
  );

  known_failure_free(&kfl);

  int bad = regression_count + fixed_count + stale_count;
  if (bad > 0) {
    test_failures += bad;
    return 1;
  }
  return 0;
}

static int test_photo_images(void) {
  const char *dir_path = "tests/qr_codes_photos";
  printf("Test: Photo QR images (%s)...\n", dir_path);

  DIR *dir = opendir(dir_path);
  if (!dir) {
    printf("  SKIP: directory not found\n");
    return 0;
  }

  int passed = 0;
  int failed = 0;
  int total = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    const char *name = entry->d_name;
    size_t len = strlen(name);
    if (len < 5 || strcmp(name + len - 4, ".txt") != 0) {
      continue;
    }

    char txt_path[512];
    snprintf(txt_path, sizeof(txt_path), "%s/%s", dir_path, name);

    FILE *f = fopen(txt_path, "r");
    if (!f) {
      continue;
    }
    char expected[1024];
    size_t nread = fread(expected, 1, sizeof(expected) - 1, f);
    fclose(f);
    while (nread > 0 &&
           (expected[nread - 1] == '\n' || expected[nread - 1] == '\r' ||
            expected[nread - 1] == ' ')) {
      nread--;
    }
    expected[nread] = '\0';

    char base[512];
    size_t base_len = len - 4;
    memcpy(base, name, base_len);
    base[base_len] = '\0';

    const char *extensions[] = {".jpeg", ".jpg", ".png"};
    char img_path[512];
    int found_image = 0;
    for (int e = 0; e < 3; e++) {
      snprintf(
        img_path,
        sizeof(img_path),
        "%s/%s%s",
        dir_path,
        base,
        extensions[e]
      );
      FILE *test = fopen(img_path, "r");
      if (test) {
        fclose(test);
        found_image = 1;
        break;
      }
    }
    if (!found_image) {
      printf("  SKIP %s: no matching image\n", name);
      continue;
    }

    int width, height, channels;
    uint8_t *img = stbi_load(img_path, &width, &height, &channels, 1);
    if (!img) {
      failed++;
      total++;
      printf("  FAIL %s: could not load\n", base);
      continue;
    }

    FCVQRCodeResult result = fcv_decode_qr_codes(width, height, img);
    total++;

    int match = 0;
    for (size_t j = 0; j < result.count; j++) {
      if (result.codes[j].text && strcmp(result.codes[j].text, expected) == 0) {
        match = 1;
        break;
      }
    }

    if (match) {
      passed++;
    }
    else {
      failed++;
      printf(
        "  FAIL %s: decoded %zu codes, expected \"%s\"",
        base,
        result.count,
        expected
      );
      if (result.count > 0 && result.codes[0].text) {
        printf(", got \"%s\"", result.codes[0].text);
      }
      printf("\n");
    }

    fcv_free_qr_result(result);
    stbi_image_free(img);
  }

  closedir(dir);
  printf(
    "  Photo results: %d/%d passed, %d failed (informational)\n",
    passed,
    total,
    failed
  );
  return 0;
}

int main(void) {
  printf("Running QR code test suite\n");
  printf("==========================\n");

  int any_failed = 0;
  any_failed |= test_null_pixels();
  any_failed |= test_zero_dimensions();
  any_failed |= test_solid_images();
  any_failed |= test_free_null_result();
  any_failed |= test_generated_images();
  any_failed |= test_difficulty_images();
  any_failed |= test_photo_images();

  printf("\n==========================\n");
  if (test_failures > 0 || any_failed) {
    printf("❌ QR tests failed: %d image(s) mismatched\n", test_failures);
    return 1;
  }
  printf("✅ All QR tests passed\n");
  return 0;
}
