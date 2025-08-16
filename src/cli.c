#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "conversion.h"
#include "crop.h"
#include "draw.h"
#include "foerstner_corner.h"
#include "perspectivetransform.h"
#include "rgba_to_grayscale.h"
#include "single_to_multichannel.h"
#include "sobel_edge_detection.h"
#include "watershed_segmentation.h"

typedef struct {
  char operation[32];
  double param;
  double param2;
  double param3;
  double param4;
  char param_str[64]; // For string parameters like "50%" or "200x300"
  int has_param;
  int has_param2;
  int has_param3;
  int has_param4;
  int has_string_param;
} PipelineOp;

typedef struct {
  PipelineOp *ops;
  int count;
  int capacity;
} Pipeline;

void print_usage(const char *program_name) {
  printf("Usage: %s <input> <pipeline> <output>\n", program_name);
  printf("Pipeline operations:\n");
  printf("  grayscale       - Convert image to grayscale\n");
  printf("  blur <radius>   - Apply gaussian blur with radius\n");
  printf("  resize <50%%>    - Resize image uniformly by percentage\n");
  printf("  resize <50%%x80%%> - Resize with different x and y percentages\n");
  printf("  resize <200x300> - Resize to absolute dimensions\n");
  printf("  threshold       - Apply Otsu threshold\n");
  printf("  bw_smart        - Smart black and white conversion\n");
  printf(
    "  bw_smooth       - Smooth (anti-aliased) black and white conversion\n"
  );
  printf("  detect_corners  - Detect corners in the image\n");
  printf("  sobel           - Apply Sobel edge detection\n");
  printf(
    "  circle <hex_color> <radius> <x>x<y> - Draw a colored circle at position "
    "(x,y)\n"
  );
  printf("  disk <hex_color> <radius> <x>x<y> - Draw a filled colored disk at "
         "position "
         "(x,y)\n");
  printf("  watershed '<x1>x<y1> <x2>x<y2> ...' - Watershed segmentation with "
         "markers at "
         "specified coordinates\n");
  printf("  crop <x> <y> <width> <height> - Crop the image\n");
  printf("\nPipeline syntax:\n");
  printf("  Operations are applied in sequence\n");
  printf("  Use parentheses for operations with parameters: (blur 3.0)\n");
  printf("\nExamples:\n");
  printf("  %s input.jpg grayscale output.jpg\n", program_name);
  printf("  %s input.jpg resize 50%% output.jpg\n", program_name);
  printf("  %s input.jpg resize '50%%x200%%' output.jpg\n", program_name);
  printf("  %s input.jpg resize 800x600 output.jpg\n", program_name);
  printf(
    "  %s input.jpg \"grayscale, resize 50%%, blur 2\" output.jpg\n",
    program_name
  );
  printf(
    "  %s input.jpg \"circle FF0000 50 200x150\" output.jpg\n",
    program_name
  );
  printf(
    "  %s input.jpg \"disk 00FF00 30 100x200\" output.jpg\n",
    program_name
  );
  printf(
    "  %s input.jpg \"watershed '100x50 200x150 300x100'\" output.jpg\n",
    program_name
  );
}

Pipeline *create_pipeline() {
  Pipeline *p = malloc(sizeof(Pipeline));
  p->capacity = 8;
  p->count = 0;
  p->ops = malloc(sizeof(PipelineOp) * p->capacity);
  return p;
}

void free_pipeline(Pipeline *p) {
  if (p) {
    free(p->ops);
    free(p);
  }
}

void add_operation(
  Pipeline *p,
  const char *op,
  double param,
  int has_param,
  double param2,
  int has_param2,
  double param3,
  int has_param3,
  double param4,
  int has_param4,
  const char *param_str,
  int has_string_param
) {
  if (p->count >= p->capacity) {
    p->capacity *= 2;
    p->ops = realloc(p->ops, sizeof(PipelineOp) * p->capacity);
  }

  strncpy(
    p->ops[p->count].operation,
    op,
    sizeof(p->ops[p->count].operation) - 1
  );
  p->ops[p->count].operation[sizeof(p->ops[p->count].operation) - 1] = '\0';
  p->ops[p->count].param = param;
  p->ops[p->count].has_param = has_param;
  p->ops[p->count].param2 = param2;
  p->ops[p->count].has_param2 = has_param2;
  p->ops[p->count].param3 = param3;
  p->ops[p->count].has_param3 = has_param3;
  p->ops[p->count].param4 = param4;
  p->ops[p->count].has_param4 = has_param4;

  if (has_string_param && param_str) {
    strncpy(
      p->ops[p->count].param_str,
      param_str,
      sizeof(p->ops[p->count].param_str) - 1
    );
    p->ops[p->count].param_str[sizeof(p->ops[p->count].param_str) - 1] = '\0';
  }
  else {
    p->ops[p->count].param_str[0] = '\0';
  }
  p->ops[p->count].has_string_param = has_string_param;

  p->count++;
}

/* Remove leading and trailing white-space, returns pointer to first
   non-blank char (string is modified in place). */
static char *trim_whitespace(char *s) {
  while (*s && isspace((unsigned char)*s)) {
    s++; // left-trim
  }
  if (*s == '\0') {
    return s;
  }
  char *end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end)) {
    end--;
  }
  *(end + 1) = '\0'; // right-trim
  return s;
}

char *skip_whitespace(char *str) {
  while (*str && isspace(*str)) {
    str++;
  }
  return str;
}

int parse_pipeline(
  int argc,
  char *argv[],
  int start_idx,
  int end_idx,
  Pipeline *pipeline
) {
  (void)argc; // Unused parameter

  /* 1. build one big string from all arguments that form the pipeline */
  size_t total_len = 0;
  for (int i = start_idx; i < end_idx; ++i) {
    total_len += strlen(argv[i]) + 2; // space OR ", "
  }

  char *combined = malloc(total_len + 1);
  if (!combined) {
    fprintf(stderr, "Error: out of memory\n");
    return 0;
  }

  combined[0] = '\0';
  for (int i = start_idx; i < end_idx; ++i) {
    const char *tok = argv[i];

    /* add space before every token except the first one */
    if (i > start_idx) {
      strcat(combined, " ");
    }

    strcat(combined, tok);
  }

  /* 2. split the combined string by commas – each chunk is one operation */
  char *saveptr;
  for (char *tok = strtok_r(combined, ",", &saveptr); tok;
       tok = strtok_r(NULL, ",", &saveptr)) {
    char *piece = trim_whitespace(tok);
    if (*piece == '\0') {
      continue; // empty fragment, skip
    }

    /* optional surrounding parentheses */
    size_t len = strlen(piece);
    if (piece[0] == '(' && piece[len - 1] == ')') {
      piece[len - 1] = '\0';
      piece = trim_whitespace(piece + 1);
    }
    if (*piece == '\0') {
      continue;
    }

    /* split into operation name and (optional) parameters */
    char *space = strchr(piece, ' ');
    if (space) {
      *space = '\0';
      char *params_str = trim_whitespace(space + 1);

      /* Check if this is a resize operation with special formats */
      if (strcmp(piece, "resize") == 0) {
        /* Check for percentage format (50% or 50%x80%) or absolute format
         * (200x300) */
        if (strchr(params_str, '%') || strchr(params_str, 'x')) {
          add_operation(
            pipeline,
            piece,
            0.0,
            0,
            0.0,
            0,
            0.0,
            0,
            0.0,
            0,
            params_str,
            1
          );
        }
        else {
          /* Fall back to numeric parsing for backward compatibility */
          char *space2 = strchr(params_str, ' ');
          if (space2) {
            *space2 = '\0';
            double param1 = atof(trim_whitespace(params_str));
            double param2 = atof(trim_whitespace(space2 + 1));
            add_operation(
              pipeline,
              piece,
              param1,
              1,
              param2,
              1,
              0.0,
              0,
              0.0,
              0,
              NULL,
              0
            );
          }
          else {
            double param = atof(params_str);
            add_operation(
              pipeline,
              piece,
              param,
              1,
              0.0,
              0,
              0.0,
              0,
              0.0,
              0,
              NULL,
              0
            );
          }
        }
      }
      else if (strcmp(piece, "circle") == 0) {
        /* Circle operation requires: hex_color radius xXy */
        char *space2 = strchr(params_str, ' ');
        if (space2) {
          *space2 = '\0';
          char *color = trim_whitespace(params_str);
          char *remaining = trim_whitespace(space2 + 1);
          char *space3 = strchr(remaining, ' ');
          if (space3) {
            *space3 = '\0';
            double radius = atof(trim_whitespace(remaining));
            char *position_str = trim_whitespace(space3 + 1);

            // Parse position in format "x×y" or "xxy"
            char *pos_copy = strdup(position_str);
            char *x_pos = strchr(pos_copy, 'x');
            if (x_pos) {
              *x_pos = '\0';
              double center_x = atof(trim_whitespace(pos_copy));
              double center_y = atof(trim_whitespace(x_pos + 1));

              // Store color in param_str, radius in param, x,y in param2,param3
              char combined_params[128];
              snprintf(
                combined_params,
                sizeof(combined_params),
                "%s %.2f %.2f",
                color,
                center_x,
                center_y
              );
              add_operation(
                pipeline,
                piece,
                radius,
                1,
                center_x,
                1,
                center_y,
                1,
                0.0,
                0,
                combined_params,
                1
              );
              free(pos_copy);
            }
            else {
              fprintf(
                stderr,
                "Error: circle position must be in format 'xXy' (e.g., "
                "'200x150')\n"
              );
              free(pos_copy);
              return 0;
            }
          }
          else {
            fprintf(
              stderr,
              "Error: circle operation requires: hex_color radius xXy\n"
            );
            return 0;
          }
        }
        else {
          fprintf(
            stderr,
            "Error: circle operation requires: hex_color radius xXy\n"
          );
          return 0;
        }
      }
      else if (strcmp(piece, "disk") == 0) {
        /* Disk operation requires: hex_color radius xXy */
        char *space2 = strchr(params_str, ' ');
        if (space2) {
          *space2 = '\0';
          char *color = trim_whitespace(params_str);
          char *remaining = trim_whitespace(space2 + 1);
          char *space3 = strchr(remaining, ' ');
          if (space3) {
            *space3 = '\0';
            double radius = atof(trim_whitespace(remaining));
            char *position_str = trim_whitespace(space3 + 1);

            // Parse position in format "x×y" or "xxy"
            char *pos_copy = strdup(position_str);
            char *x_pos = strchr(pos_copy, 'x');
            if (x_pos) {
              *x_pos = '\0';
              double center_x = atof(trim_whitespace(pos_copy));
              double center_y = atof(trim_whitespace(x_pos + 1));

              // Store color in param_str, radius in param, x,y in param2,param3
              char combined_params[128];
              snprintf(
                combined_params,
                sizeof(combined_params),
                "%s %.2f %.2f",
                color,
                center_x,
                center_y
              );
              add_operation(
                pipeline,
                piece,
                radius,
                1,
                center_x,
                1,
                center_y,
                1,
                0.0,
                0,
                combined_params,
                1
              );
              free(pos_copy);
            }
            else {
              fprintf(
                stderr,
                "Error: disk position must be in format 'xXy' (e.g., "
                "'200x150')\n"
              );
              free(pos_copy);
              return 0;
            }
          }
          else {
            fprintf(
              stderr,
              "Error: disk operation requires: hex_color radius xXy\n"
            );
            return 0;
          }
        }
        else {
          fprintf(
            stderr,
            "Error: disk operation requires: hex_color radius xXy\n"
          );
          return 0;
        }
      }
      else if (strcmp(piece, "watershed") == 0) {
        /* Watershed operation requires: x1×y1,x2×y2,... */
        add_operation(
          pipeline,
          piece,
          0.0,
          0,
          0.0,
          0,
          0.0,
          0,
          0.0,
          0,
          params_str,
          1
        );
      }
      else if (strcmp(piece, "crop") == 0) {
        double x, y, w, h;
        if (sscanf(params_str, "%lf %lf %lf %lf", &x, &y, &w, &h) == 4) {
          add_operation(pipeline, piece, x, 1, y, 1, w, 1, h, 1, NULL, 0);
        }
        else {
          fprintf(
            stderr,
            "Error: crop operation requires x, y, width, height\n"
          );
          return 0;
        }
      }
      else {
        /* Regular numeric parameter parsing for other operations */
        char *space2 = strchr(params_str, ' ');
        if (space2) {
          *space2 = '\0';
          double param1 = atof(trim_whitespace(params_str));
          double param2 = atof(trim_whitespace(space2 + 1));
          add_operation(
            pipeline,
            piece,
            param1,
            1,
            param2,
            1,
            0.0,
            0,
            0.0,
            0,
            NULL,
            0
          );
        }
        else {
          double param = atof(params_str);
          add_operation(
            pipeline,
            piece,
            param,
            1,
            0.0,
            0,
            0.0,
            0,
            0.0,
            0,
            NULL,
            0
          );
        }
      }
    }
    else {
      add_operation(pipeline, piece, 0.0, 0, 0.0, 0, 0.0, 0, 0.0, 0, NULL, 0);
    }
  }

  free(combined);
  return 1;
}

unsigned char *apply_operation(
  int *width,
  int *height,
  const char *operation,
  double param,
  int has_param,
  double param2,
  int has_param2,
  double param3,
  int has_param3,
  double param4,
  int has_param4,
  const char *param_str,
  int has_string_param,
  unsigned char *input_data
) {
  if (strcmp(operation, "grayscale") == 0) {
    return (unsigned char *)grayscale(*width, *height, input_data);
  }
  else if (strcmp(operation, "blur") == 0) {
    if (!has_param) {
      fprintf(stderr, "Error: blur operation requires radius parameter\n");
      return NULL;
    }
    return (unsigned char *)
      apply_gaussian_blur(*width, *height, param, input_data);
  }
  else if (strcmp(operation, "resize") == 0) {
    double resize_x, resize_y;

    if (has_string_param) {
      // Parse new resize formats: 50%, 50%x80%, 200x300
      char *param_copy = strdup(param_str);
      char *x_pos = strchr(param_copy, 'x');

      if (x_pos) {
        // Format: 50%x80% or 200x300
        *x_pos = '\0';
        char *first_part = trim_whitespace(param_copy);
        char *second_part = trim_whitespace(x_pos + 1);

        if (strchr(first_part, '%')) {
          // Percentage format: 50%x80%
          resize_x = atof(first_part) / 100.0;
          resize_y = strchr(second_part, '%') ? atof(second_part) / 100.0
                                              : atof(second_part) / 100.0;
        }
        else {
          // Absolute size format: 200x300
          resize_x = atof(first_part) / *width;
          resize_y = atof(second_part) / *height;
        }
      }
      else if (strchr(param_copy, '%')) {
        // Uniform percentage format: 50%
        resize_x = resize_y = atof(param_copy) / 100.0;
      }
      else {
        fprintf(stderr, "Error: Invalid resize format '%s'\n", param_str);
        free(param_copy);
        return NULL;
      }

      free(param_copy);
    }
    else if (has_param) {
      // Backward compatibility: numeric parameters
      resize_x = param;
      resize_y = has_param2 ? param2 : param;
    }
    else {
      fprintf(stderr, "Error: resize operation requires resize parameter\n");
      return NULL;
    }

    unsigned int out_width, out_height;
    unsigned char *result = (unsigned char *)resize(
      *width,
      *height,
      resize_x,
      resize_y,
      &out_width,
      &out_height,
      input_data
    );

    if (result) {
      *width = out_width;
      *height = out_height;
    }

    return result;
  }
  else if (strcmp(operation, "threshold") == 0) {
    return (unsigned char *)
      otsu_threshold_rgba(*width, *height, false, input_data);
  }
  else if (strcmp(operation, "bw_smart") == 0) {
    return (unsigned char *)bw_smart(*width, *height, false, input_data);
  }
  else if (strcmp(operation, "bw_smooth") == 0) {
    return (unsigned char *)bw_smart(*width, *height, true, input_data);
  }
  else if (strcmp(operation, "detect_corners") == 0) {
    Corners corners = detect_corners(input_data, *width, *height);
    printf("  Detected corners:\n");
    printf("    Top-left:     (%.2f, %.2f)\n", corners.tl_x, corners.tl_y);
    printf("    Top-right:    (%.2f, %.2f)\n", corners.tr_x, corners.tr_y);
    printf("    Bottom-right: (%.2f, %.2f)\n", corners.br_x, corners.br_y);
    printf("    Bottom-left:  (%.2f, %.2f)\n", corners.bl_x, corners.bl_y);

    // Return a copy of the input data since we don't modify the image
    unsigned char *result = malloc(*width * *height * 4);
    if (result) {
      memcpy(result, input_data, *width * *height * 4);
    }
    return result;
  }
  else if (strcmp(operation, "sobel") == 0) {
    unsigned char *grayscale_sobel =
      sobel_edge_detection(*width, *height, 4, input_data);
    if (!grayscale_sobel) {
      return NULL;
    }

    // Convert single-channel grayscale to RGBA format
    unsigned char *rgba_result =
      single_to_multichannel(*width, *height, grayscale_sobel);
    free(grayscale_sobel);
    return (unsigned char *)rgba_result;
  }
  else if (strcmp(operation, "circle") == 0) {
    if (!has_string_param || !has_param || !has_param2 || !has_param3) {
      fprintf(
        stderr,
        "Error: circle operation requires: hex_color radius xXy\n"
      );
      return NULL;
    }
    // Parse color and coordinates from param_str
    char *param_copy = strdup(param_str);
    char *color = strtok(param_copy, " ");
    double center_x = param2;
    double center_y = param3;
    double radius = param;

    unsigned int img_length_byte = (*width) * (*height) * 4;
    unsigned char *result = malloc(img_length_byte);
    if (!result) {
      free(param_copy);
      return NULL;
    }
    memcpy(result, input_data, img_length_byte);

    draw_circle(*width, *height, 4, color, radius, center_x, center_y, result);

    free(param_copy);
    return result;
  }
  else if (strcmp(operation, "disk") == 0) {
    if (!has_string_param || !has_param || !has_param2 || !has_param3) {
      fprintf(stderr, "Error: disk operation requires: hex_color radius xXy\n");
      return NULL;
    }
    // Parse color and coordinates from param_str
    char *param_copy = strdup(param_str);
    char *color = strtok(param_copy, " ");
    double center_x = param2;
    double center_y = param3;
    double radius = param;

    unsigned int img_length_byte = (*width) * (*height) * 4;
    unsigned char *result = malloc(img_length_byte);
    if (!result) {
      free(param_copy);
      return NULL;
    }
    memcpy(result, input_data, img_length_byte);

    draw_disk(*width, *height, 4, color, radius, center_x, center_y, result);

    free(param_copy);
    return result;
  }
  else if (strcmp(operation, "watershed") == 0) {
    if (!has_string_param) {
      fprintf(
        stderr,
        "Error: watershed operation requires marker coordinates\n"
      );
      return NULL;
    }

    // Parse marker coordinates from param_str: "x1×y1 x2×y2 ..."
    char *param_copy = strdup(param_str);

    // Count number of markers by counting spaces + 1
    int num_markers = 1;
    for (char *p = param_copy; *p; p++) {
      if (*p == ' ') {
        num_markers++;
      }
    }

    Point2D *markers = malloc(num_markers * sizeof(Point2D));
    if (!markers) {
      free(param_copy);
      return NULL;
    }

    // Parse each marker
    char *token = strtok(param_copy, " ");
    int marker_idx = 0;

    while (token && marker_idx < num_markers) {
      char *x_pos = strchr(token, 'x');
      if (x_pos) {
        *x_pos = '\0';
        markers[marker_idx].x = atof(trim_whitespace(token));
        markers[marker_idx].y = atof(trim_whitespace(x_pos + 1));
        marker_idx++;
      }
      token = strtok(NULL, " ");
    }

    if (marker_idx == 0) {
      fprintf(
        stderr,
        "Error: No valid markers found in format 'x1×y1 x2×y2 ...'\n"
      );
      free(markers);
      free(param_copy);
      return NULL;
    }

    // Convert RGBA to single-channel grayscale for watershed segmentation
    unsigned char *grayscale_data =
      rgba_to_grayscale(*width, *height, input_data);
    if (!grayscale_data) {
      free(markers);
      free(param_copy);
      return NULL;
    }

    unsigned char *result = (unsigned char *)watershed_segmentation(
      *width,
      *height,
      grayscale_data,
      markers,
      marker_idx,
      false
    );
    free(grayscale_data);

    free(markers);
    free(param_copy);
    return result;
  }
  else if (strcmp(operation, "crop") == 0) {
    if (!has_param || !has_param2 || !has_param3 || !has_param4) {
      fprintf(stderr, "Error: crop operation requires x, y, width, height\n");
      return NULL;
    }
    unsigned int x = (unsigned int)param;
    unsigned int y = (unsigned int)param2;
    unsigned int new_width = (unsigned int)param3;
    unsigned int new_height = (unsigned int)param4;
    unsigned char *result =
      crop(*width, *height, 4, input_data, x, y, new_width, new_height);
    if (result) {
      *width = new_width;
      *height = new_height;
    }
    return result;
  }
  else {
    fprintf(stderr, "Error: Unknown operation '%s'\n", operation);
    return NULL;
  }
}

unsigned char *execute_pipeline(
  int *width,
  int *height,
  Pipeline *pipeline,
  unsigned char *input_data
) {
  unsigned char *current_data = input_data;
  unsigned char *temp_data = NULL;

  for (int i = 0; i < pipeline->count; i++) {
    PipelineOp *op = &pipeline->ops[i];
    printf("Applying operation: %s", op->operation);
    if (op->has_string_param) {
      printf(" with parameter: %s", op->param_str);
    }
    else if (op->has_param) {
      printf(" with parameter: %.2f", op->param);
      if (op->has_param2) {
        printf(" %.2f", op->param2);
        if (op->has_param3) {
          printf(" %.2f", op->param3);
        }
        if (op->has_param4) {
          printf(" %.2f", op->param4);
        }
      }
    }
    printf("\n");

    clock_t start_time = clock();
    unsigned char *result = apply_operation(
      width,
      height,
      op->operation,
      op->param,
      op->has_param,
      op->param2,
      op->has_param2,
      op->param3,
      op->has_param3,
      op->param4,
      op->has_param4,
      op->param_str,
      op->has_string_param,
      current_data
    );
    clock_t end_time = clock();

    double elapsed_time_ms =
      ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    printf(
      "  → Completed in %.1f ms (output: %dx%d)\n",
      elapsed_time_ms,
      *width,
      *height
    );

    if (!result) {
      if (temp_data && temp_data != input_data) {
        free(temp_data);
      }
      return NULL;
    }

    // Free the previous intermediate result (but not the original input)
    if (temp_data && temp_data != input_data) {
      free(temp_data);
    }

    temp_data = result;
    current_data = result;
  }

  return current_data;
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    print_usage(argv[0]);
    return 1;
  }

  const char *input_path = argv[1];
  const char *output_path = argv[argc - 1];

  // Parse pipeline from arguments between input and output
  Pipeline *pipeline = create_pipeline();
  if (!parse_pipeline(argc, argv, 2, argc - 1, pipeline)) {
    free_pipeline(pipeline);
    return 1;
  }

  if (pipeline->count == 0) {
    fprintf(stderr, "Error: No operations specified\n");
    free_pipeline(pipeline);
    return 1;
  }

  int width, height, channels;
  unsigned char *image_data =
    stbi_load(input_path, &width, &height, &channels, 4);

  if (!image_data) {
    fprintf(stderr, "Error: Could not load image '%s'\n", input_path);
    free_pipeline(pipeline);
    return 1;
  }

  printf("Loaded image: %dx%d with %d channels\n", width, height, channels);
  printf("Executing pipeline with %d operations:\n", pipeline->count);

  unsigned char *result_data =
    execute_pipeline(&width, &height, pipeline, image_data);

  if (!result_data) {
    fprintf(stderr, "Error: Failed to execute pipeline\n");
    stbi_image_free(image_data);
    free_pipeline(pipeline);
    return 1;
  }

  printf("Final output dimensions: %dx%d\n", width, height);

  int write_result;
  const char *ext = strrchr(output_path, '.');
  if (ext && strcmp(ext, ".png") == 0) {
    write_result =
      stbi_write_png(output_path, width, height, 4, result_data, width * 4);
  }
  else if (ext && (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)) {
    write_result =
      stbi_write_jpg(output_path, width, height, 4, result_data, 90);
  }
  else {
    write_result =
      stbi_write_png(output_path, width, height, 4, result_data, width * 4);
  }

  if (!write_result) {
    fprintf(stderr, "Error: Could not save image to '%s'\n", output_path);
    stbi_image_free(image_data);
    if (result_data != image_data) {
      free(result_data);
    }
    free_pipeline(pipeline);
    return 1;
  }

  printf("Successfully saved processed image to '%s'\n", output_path);

  stbi_image_free(image_data);
  if (result_data != image_data) {
    free(result_data);
  }
  free_pipeline(pipeline);
  return 0;
}
