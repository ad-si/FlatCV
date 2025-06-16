#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "flatcv.h"

typedef struct {
  char operation[32];
  double param;
  int has_param;
} PipelineOp;

typedef struct {
  PipelineOp* ops;
  int count;
  int capacity;
} Pipeline;

void print_usage(const char* program_name) {
  printf("Usage: %s <input> <pipeline> <output>\n", program_name);
  printf("Pipeline operations:\n");
  printf("  grayscale     - Convert image to grayscale\n");
  printf("  blur <radius>   - Apply gaussian blur with radius\n");
  printf("  threshold     - Apply Otsu threshold\n");
  printf("  bw_smart      - Smart black and white conversion\n");
  printf("\nPipeline syntax:\n");
  printf("  Operations are applied in sequence\n");
  printf("  Use parentheses for operations with parameters: (blur 3.0)\n");
  printf("\nExamples:\n");
  printf("  %s input.jpg grayscale output.jpg\n", program_name);
  printf("  %s input.jpg grayscale (blur 3.0) output.jpg\n", program_name);
  printf("  %s input.jpg (blur 2.0) threshold output.jpg\n", program_name);
}

Pipeline* create_pipeline() {
  Pipeline* p = malloc(sizeof(Pipeline));
  p->capacity = 8;
  p->count = 0;
  p->ops = malloc(sizeof(PipelineOp) * p->capacity);
  return p;
}

void free_pipeline(Pipeline* p) {
  if (p) {
    free(p->ops);
    free(p);
  }
}

void add_operation(Pipeline* p, const char* op, double param, int has_param) {
  if (p->count >= p->capacity) {
    p->capacity *= 2;
    p->ops = realloc(p->ops, sizeof(PipelineOp) * p->capacity);
  }

  strncpy(p->ops[p->count].operation, op, sizeof(p->ops[p->count].operation) - 1);
  p->ops[p->count].operation[sizeof(p->ops[p->count].operation) - 1] = '\0';
  p->ops[p->count].param = param;
  p->ops[p->count].has_param = has_param;
  p->count++;
}

char* skip_whitespace(char* str) {
  while (*str && isspace(*str)) str++;
  return str;
}

int parse_pipeline(int argc, char* argv[], int start_idx, int end_idx, Pipeline* pipeline) {
  for (int i = start_idx; i < end_idx; i++) {
    char* arg = argv[i];

    if (arg[0] == '(') {
      // Operation with parameter in parentheses
      char* content = arg + 1;
      char* end_paren = strchr(content, ')');
      if (!end_paren) {
        fprintf(stderr, "Error: Missing closing parenthesis in '%s'\n", arg);
        return 0;
      }

      *end_paren = '\0';
      content = skip_whitespace(content);

      char* param_str = strchr(content, ' ');
      if (param_str) {
        *param_str = '\0';
        param_str = skip_whitespace(param_str + 1);
        double param = atof(param_str);
        add_operation(pipeline, content, param, 1);
      } else {
        add_operation(pipeline, content, 0.0, 0);
      }

      *end_paren = ')';
    } else {
      // Simple operation without parameters
      add_operation(pipeline, arg, 0.0, 0);
    }
  }
  return 1;
}

unsigned char* apply_operation(int width, int height, const char* operation, double param, int has_param, unsigned char* input_data) {
  if (strcmp(operation, "grayscale") == 0) {
    return (unsigned char*)grayscale(width, height, input_data);
  } else if (strcmp(operation, "blur") == 0) {
    if (!has_param) {
      fprintf(stderr, "Error: blur operation requires radius parameter\n");
      return NULL;
    }
    return (unsigned char*)apply_gaussian_blur(width, height, param, input_data);
  } else if (strcmp(operation, "threshold") == 0) {
    return (unsigned char*)otsu_threshold_rgba(width, height, false, input_data);
  } else if (strcmp(operation, "bw_smart") == 0) {
    return (unsigned char*)bw_smart(width, height, false, input_data);
  } else {
    fprintf(stderr, "Error: Unknown operation '%s'\n", operation);
    return NULL;
  }
}

unsigned char* execute_pipeline(int width, int height, Pipeline* pipeline, unsigned char* input_data) {
  unsigned char* current_data = input_data;
  unsigned char* temp_data = NULL;

  for (int i = 0; i < pipeline->count; i++) {
    PipelineOp* op = &pipeline->ops[i];
    printf("Applying operation: %s", op->operation);
    if (op->has_param) {
      printf(" with parameter: %.2f", op->param);
    }
    printf("\n");

    unsigned char* result = apply_operation(width, height, op->operation, op->param, op->has_param, current_data);

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

int main(int argc, char* argv[]) {
  if (argc < 4) {
    print_usage(argv[0]);
    return 1;
  }

  const char* input_path = argv[1];
  const char* output_path = argv[argc - 1];

  // Parse pipeline from arguments between input and output
  Pipeline* pipeline = create_pipeline();
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
  unsigned char* image_data = stbi_load(input_path, &width, &height, &channels, 4);

  if (!image_data) {
    fprintf(stderr, "Error: Could not load image '%s'\n", input_path);
    free_pipeline(pipeline);
    return 1;
  }

  printf("Loaded image: %dx%d with %d channels\n", width, height, channels);
  printf("Executing pipeline with %d operations:\n", pipeline->count);

  unsigned char* result_data = execute_pipeline(width, height, pipeline, image_data);

  if (!result_data) {
    fprintf(stderr, "Error: Failed to execute pipeline\n");
    stbi_image_free(image_data);
    free_pipeline(pipeline);
    return 1;
  }

  int write_result;
  const char* ext = strrchr(output_path, '.');
  if (ext && strcmp(ext, ".png") == 0) {
    write_result = stbi_write_png(output_path, width, height, 4, result_data, width * 4);
  } else if (ext && (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)) {
    write_result = stbi_write_jpg(output_path, width, height, 4, result_data, 90);
  } else {
    write_result = stbi_write_png(output_path, width, height, 4, result_data, width * 4);
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
