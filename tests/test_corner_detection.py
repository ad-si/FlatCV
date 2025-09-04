#!/usr/bin/env -S uv run --quiet --script
# /// script
# dependencies = [
#   "shapely",
# ]
# ///
"""
Test script for corner detection accuracy.
Compares detected corners with ground truth and calculates overlap percentage.
"""

import json
import subprocess
import sys
import os
from pathlib import Path
import math
from shapely.geometry import Polygon
from shapely.ops import unary_union


def corners_to_polygon(corners):
  """Convert corners dictionary to a Polygon object."""
  corner_keys = ["top_left", "top_right", "bottom_right", "bottom_left"]

  # Check if all required corners exist
  for key in corner_keys:
    if key not in corners:
      return None

  # Create polygon from corners in the correct order
  points = [
    corners["top_left"],
    corners["top_right"],
    corners["bottom_right"],
    corners["bottom_left"]
  ]

  try:
    return Polygon(points)
  except Exception:
    return None


def calculate_overlap_percentage(detected_corners, ground_truth_corners):
  """
  Calculate the overlap percentage
  between detected and ground truth quadrilaterals.
  Returns the intersection area divided by the union area as a percentage.
  """
  detected_poly = corners_to_polygon(detected_corners)
  ground_truth_poly = corners_to_polygon(ground_truth_corners)

  if detected_poly is None or ground_truth_poly is None:
    return 0.0

  if not detected_poly.is_valid or not ground_truth_poly.is_valid:
    return 0.0

  try:
    intersection = detected_poly.intersection(ground_truth_poly)
    union = detected_poly.union(ground_truth_poly)

    if union.area == 0:
      return 0.0

    overlap_percentage = (intersection.area / union.area) * 100
    return overlap_percentage

  except Exception:
    return 0.0


def run_corner_detection(image_path):
  """Run flatcv corner detection and parse JSON output."""
  try:
    result = subprocess.run(
      ["./flatcv", image_path, "detect_corners"],
      capture_output=True,
      text=True,
      check=True
    )

    # Parse the JSON output (first part before the log messages)
    output_lines = result.stdout.strip().split('\n')
    json_lines = []

    # Find JSON content (starts with { and ends with })
    in_json = False
    brace_count = 0

    for line in output_lines:
      if line.strip().startswith('{'):
        in_json = True
        brace_count = line.count('{') - line.count('}')
        json_lines.append(line)
      elif in_json:
        json_lines.append(line)
        brace_count += line.count('{') - line.count('}')
        if brace_count == 0:
          break

    json_str = '\n'.join(json_lines)
    return json.loads(json_str)

  except (subprocess.CalledProcessError, json.JSONDecodeError, FileNotFoundError) as e:
    print(f"Error running corner detection on {image_path}: {e}")
    return None


def main():
  """Main test function."""
  # Get the directory containing test documents
  test_dir = Path("tests/documents")

  if not test_dir.exists():
    print(f"Test directory {test_dir} not found")
    return 1

  # Find all image files with corresponding JSON files recursively
  image_files = list(test_dir.glob("**/*.jpeg"))

  if not image_files:
    print("No test images found")
    return 1

  total_overlap = 0.0
  total_tests = 0
  
  # Group images by category
  categories = {}
  for image_file in image_files:
    category = image_file.parent.name
    if category not in categories:
      categories[category] = []
    categories[category].append(image_file)

  print(f"Running corner detection tests on {len(image_files)} images...")
  print()

  # Process each category separately
  for category in sorted(categories.keys()):
    print(f"🏷️  {category.upper().replace('_', ' ')}")
    category_overlap = 0.0
    category_tests = 0
    
    for image_file in sorted(categories[category]):
      json_file = image_file.with_suffix('.json')

      if not json_file.exists():
        print(f"⚠️  Skipping {image_file.name} - no ground truth file")
        continue

      # Load ground truth
      try:
        with open(json_file, 'r') as f:
          ground_truth = json.load(f)
      except json.JSONDecodeError as e:
        print(f"❌ Error reading {json_file.name}: {e}")
        continue

      # Run corner detection
      detected = run_corner_detection(str(image_file))
      if detected is None:
        print(f"❌ {image_file.name}: Corner detection failed (0.0% overlap)")
        category_tests += 1
        total_tests += 1
        continue

      # Calculate overlap percentage
      overlap = calculate_overlap_percentage(
        detected.get('corners', {}),
        ground_truth.get('corners', {})
      )

      category_overlap += overlap
      category_tests += 1
      total_tests += 1
      total_overlap += overlap

      print(f"📊 {image_file.name}: {overlap:.1f}% overlap")
    
    # Print category summary
    if category_tests > 0:
      category_avg = category_overlap / category_tests
      print(f"   📈 {category.replace('_', ' ').title()}: {category_avg:.1f}% average (n={category_tests})")
    print()

  # Calculate and display overall results
  if total_tests > 0:
    average_overlap = total_overlap / total_tests
    print(f"🎯 Overall average quadrilateral overlap across {total_tests} images: {average_overlap:.1f}%")
    return 0
  else:
    print("No tests were run")
    return 1


if __name__ == "__main__":
  sys.exit(main())
