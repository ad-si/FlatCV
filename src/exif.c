#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef FLATCV_AMALGAMATION
#include "exif.h"
#endif

static uint16_t read_u16(FILE *file, int is_little_endian) {
  uint8_t buf[2];
  if (fread(buf, 1, 2, file) != 2) {
    return 0;
  }
  if (is_little_endian) {
    return (uint16_t)(buf[0] | (buf[1] << 8));
  }
  return (uint16_t)((buf[0] << 8) | buf[1]);
}

static uint32_t read_u32(FILE *file, int is_little_endian) {
  uint8_t buf[4];
  if (fread(buf, 1, 4, file) != 4) {
    return 0;
  }
  if (is_little_endian) {
    return (uint32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
  }
  return (uint32_t)((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
}

/**
 * Parse the orientation tag from a TIFF/EXIF block. `tiff_start` must point
 * to the first byte of the TIFF header (the "II" / "MM" byte-order marker).
 * Returns the orientation value (1-8) or 1 if not found.
 */
static int32_t read_orientation_from_tiff(FILE *file, long tiff_start) {
  fseek(file, tiff_start, SEEK_SET);

  uint8_t tiff_header[2];
  if (fread(tiff_header, 1, 2, file) != 2) {
    return 1;
  }

  int is_little_endian = (tiff_header[0] == 'I' && tiff_header[1] == 'I');
  int is_big_endian = (tiff_header[0] == 'M' && tiff_header[1] == 'M');

  if (!is_little_endian && !is_big_endian) {
    return 1;
  }

  uint16_t magic = read_u16(file, is_little_endian);
  if (magic != 42) {
    return 1;
  }

  uint32_t ifd_offset = read_u32(file, is_little_endian);

  fseek(file, tiff_start + ifd_offset, SEEK_SET);

  uint16_t num_entries = read_u16(file, is_little_endian);

  for (uint16_t i = 0; i < num_entries; i++) {
    uint16_t tag = read_u16(file, is_little_endian);
    (void)read_u16(file, is_little_endian); // Skip type
    (void)read_u32(file, is_little_endian); // Skip count

    if (tag == 0x0112) { // Orientation
      return (int32_t)read_u16(file, is_little_endian);
    }
    else {
      fseek(file, 4, SEEK_CUR); // Skip value/offset
    }
  }

  return 1;
}

/**
 * Read the EXIF orientation from a JPEG file. `file` must be positioned just
 * after the two-byte SOI marker (0xFFD8).
 */
static int32_t get_jpeg_orientation(FILE *file) {
  uint8_t marker[2];
  while (fread(marker, 1, 2, file) == 2) {
    if (marker[0] != 0xFF) {
      break;
    }

    if (marker[1] == 0xD9 || marker[1] == 0xDA) { // EOI or SOS
      break;
    }

    uint16_t size = read_u16(file, 0); // Markers are always big-endian

    if (marker[1] == 0xE1) { // APP1 - EXIF
      uint8_t exif_header[6];
      if (fread(exif_header, 1, 6, file) != 6) {
        break;
      }

      if (memcmp(exif_header, "Exif\0\0", 6) != 0) {
        fseek(file, size - 2 - 6, SEEK_CUR);
        continue;
      }

      return read_orientation_from_tiff(file, ftell(file));
    }
    else {
      fseek(file, size - 2, SEEK_CUR);
    }
  }

  return 1;
}

/**
 * Read the EXIF orientation from a PNG file. `file` must be positioned just
 * after the eight-byte PNG signature.
 *
 * EXIF data is stored in an `eXIf` chunk whose payload is a raw TIFF block
 * (no leading "Exif\0\0" prefix, unlike JPEG's APP1 segment).
 */
static int32_t get_png_orientation(FILE *file) {
  uint8_t type[4];
  while (1) {
    uint32_t length = read_u32(file, 0); // Chunk fields are big-endian
    if (fread(type, 1, 4, file) != 4) {
      break;
    }

    if (memcmp(type, "eXIf", 4) == 0) {
      return read_orientation_from_tiff(file, ftell(file));
    }

    if (memcmp(type, "IEND", 4) == 0) {
      break;
    }

    fseek(file, (long)length + 4, SEEK_CUR); // Skip chunk data + CRC
  }

  return 1;
}

int32_t fcv_get_exif_orientation(char const *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    return 1;
  }

  uint8_t signature[8];
  size_t read_count = fread(signature, 1, 8, file);

  int32_t orientation = 1;

  static uint8_t const png_signature[8] =
    {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

  if (read_count >= 2 && signature[0] == 0xFF && signature[1] == 0xD8) {
    fseek(file, 2, SEEK_SET);
    orientation = get_jpeg_orientation(file);
  }
  else if (read_count == 8 && memcmp(signature, png_signature, 8) == 0) {
    orientation = get_png_orientation(file);
  }

  fclose(file);
  return orientation;
}
