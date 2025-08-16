#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef FLATCV_AMALGAMATION
#include "perspectivetransform.h"
#else
#include "flatcv.h"
#endif

uint8_t *fcv_extract_document(
  uint32_t width,
  uint32_t height,
  uint8_t const *const data,
  uint32_t output_width,
  uint32_t output_height
) {
  if (!data) {
    fprintf(stderr, "Error: NULL image data\n");
    return NULL;
  }

  // Step 1: Detect corners of the document
  Corners detected_corners = fcv_detect_corners(data, width, height);

  // Step 2: Define destination corners (rectangular output)
  Corners dst_corners = {
    .tl_x = 0.0,
    .tl_y = 0.0,
    .tr_x = (double)(output_width - 1),
    .tr_y = 0.0,
    .br_x = (double)(output_width - 1),
    .br_y = (double)(output_height - 1),
    .bl_x = 0.0,
    .bl_y = (double)(output_height - 1)
  };

  // Step 3: Calculate perspective transformation matrix
  // Note: We need to map FROM destination TO source for the inverse transform
  Matrix3x3 *transform_matrix =
    fcv_calculate_perspective_transform(&dst_corners, &detected_corners);

  if (!transform_matrix) {
    fprintf(stderr, "Error: Failed to calculate perspective transform\n");
    return NULL;
  }

  // Step 4: Apply the transformation
  uint8_t *result = fcv_apply_matrix_3x3(
    width,
    height,
    (uint8_t *)data,
    output_width,
    output_height,
    transform_matrix
  );

  // Free the transformation matrix if it's not the identity matrix
  if (transform_matrix &&
      !(transform_matrix->m00 == 1.0 && transform_matrix->m01 == 0.0 &&
        transform_matrix->m02 == 0.0 && transform_matrix->m10 == 0.0 &&
        transform_matrix->m11 == 1.0 && transform_matrix->m12 == 0.0 &&
        transform_matrix->m20 == 0.0 && transform_matrix->m21 == 0.0 &&
        transform_matrix->m22 == 1.0)) {
    free(transform_matrix);
  }

  return result;
}

uint8_t *fcv_extract_document_auto(
  uint32_t width,
  uint32_t height,
  uint8_t const *const data,
  uint32_t *output_width,
  uint32_t *output_height
) {
  if (!data || !output_width || !output_height) {
    fprintf(stderr, "Error: NULL parameters\n");
    return NULL;
  }

  // Step 1: Detect corners of the document
  Corners detected_corners = fcv_detect_corners(data, width, height);

  // Step 2: Calculate optimal output dimensions based on detected corners
  // Calculate distances between corners to determine aspect ratio
  double top_width = sqrt(
    pow(detected_corners.tr_x - detected_corners.tl_x, 2) +
    pow(detected_corners.tr_y - detected_corners.tl_y, 2)
  );
  double bottom_width = sqrt(
    pow(detected_corners.br_x - detected_corners.bl_x, 2) +
    pow(detected_corners.br_y - detected_corners.bl_y, 2)
  );
  double left_height = sqrt(
    pow(detected_corners.bl_x - detected_corners.tl_x, 2) +
    pow(detected_corners.bl_y - detected_corners.tl_y, 2)
  );
  double right_height = sqrt(
    pow(detected_corners.br_x - detected_corners.tr_x, 2) +
    pow(detected_corners.br_y - detected_corners.tr_y, 2)
  );

  // Use maximum dimensions to preserve detail
  double max_width = fmax(top_width, bottom_width);
  double max_height = fmax(left_height, right_height);

  // Round to reasonable dimensions
  *output_width = (uint32_t)(max_width + 0.5);
  *output_height = (uint32_t)(max_height + 0.5);

  // Ensure minimum reasonable size
  if (*output_width < 100) {
    *output_width = 100;
  }
  if (*output_height < 100) {
    *output_height = 100;
  }

  // Step 3: Define destination corners (rectangular output)
  Corners dst_corners = {
    .tl_x = 0.0,
    .tl_y = 0.0,
    .tr_x = (double)(*output_width - 1),
    .tr_y = 0.0,
    .br_x = (double)(*output_width - 1),
    .br_y = (double)(*output_height - 1),
    .bl_x = 0.0,
    .bl_y = (double)(*output_height - 1)
  };

  // Step 4: Calculate perspective transformation matrix
  // Note: We need to map FROM destination TO source for the inverse transform
  Matrix3x3 *transform_matrix =
    fcv_calculate_perspective_transform(&dst_corners, &detected_corners);

  if (!transform_matrix) {
    fprintf(stderr, "Error: Failed to calculate perspective transform\n");
    return NULL;
  }

  // Step 5: Apply the transformation
  uint8_t *result = fcv_apply_matrix_3x3(
    width,
    height,
    (uint8_t *)data,
    *output_width,
    *output_height,
    transform_matrix
  );

  // Free the transformation matrix if it's not the identity matrix
  if (transform_matrix &&
      !(transform_matrix->m00 == 1.0 && transform_matrix->m01 == 0.0 &&
        transform_matrix->m02 == 0.0 && transform_matrix->m10 == 0.0 &&
        transform_matrix->m11 == 1.0 && transform_matrix->m12 == 0.0 &&
        transform_matrix->m20 == 0.0 && transform_matrix->m21 == 0.0 &&
        transform_matrix->m22 == 1.0)) {
    free(transform_matrix);
  }

  return result;
}