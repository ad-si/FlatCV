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

int32_t fcv_get_exif_orientation(char const *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    return 1;
  }

  uint8_t header[2];
  if (fread(header, 1, 2, file) != 2 || header[0] != 0xFF ||
      header[1] != 0xD8) {
    fclose(file);
    return 1;
  }

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

      long tiff_start = ftell(file);
      uint8_t tiff_header[2];
      if (fread(tiff_header, 1, 2, file) != 2) {
        break;
      }

      int is_little_endian = (tiff_header[0] == 'I' && tiff_header[1] == 'I');
      int is_big_endian = (tiff_header[0] == 'M' && tiff_header[1] == 'M');

      if (!is_little_endian && !is_big_endian) {
        break;
      }

      uint16_t magic = read_u16(file, is_little_endian);
      if (magic != 42) {
        break;
      }

      uint32_t ifd_offset = read_u32(file, is_little_endian);

      fseek(file, tiff_start + ifd_offset, SEEK_SET);

      uint16_t num_entries = read_u16(file, is_little_endian);

      for (uint16_t i = 0; i < num_entries; i++) {
        uint16_t tag = read_u16(file, is_little_endian);
        (void)read_u16(file, is_little_endian); // Skip type
        (void)read_u32(file, is_little_endian); // Skip count

        if (tag == 0x0112) { // Orientation
          uint16_t orientation = read_u16(file, is_little_endian);
          fclose(file);
          return (int32_t)orientation;
        }
        else {
          fseek(file, 4, SEEK_CUR); // Skip value/offset
        }
      }
      break; // IFD0 done
    }
    else {
      fseek(file, size - 2, SEEK_CUR);
    }
  }

  fclose(file);
  return 1;
}
