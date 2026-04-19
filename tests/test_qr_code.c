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
  any_failed |= test_photo_images();

  printf("\n==========================\n");
  if (test_failures > 0 || any_failed) {
    printf("❌ QR tests failed: %d image(s) mismatched\n", test_failures);
    return 1;
  }
  printf("✅ All QR tests passed\n");
  return 0;
}
