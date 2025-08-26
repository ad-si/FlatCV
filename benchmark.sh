#!/bin/dash

print_header() {
  printf "\n===== ▶️ %s =====\n\n" "$1"
}

run_benchmark() {
  # DEBUG=1

  print_header "$1"
  shift

  if test -z "$DEBUG"
  then
    hyperfine --warmup=2 "$@" | sed -n '/Summary/,$p'
  else
    hyperfine --warmup=2 --runs=1 "$@"
  fi
}


mkdir -p tmp

# TODO:   './flatcv imgs/parrot_hq.jpeg rotate 90 tmp/rotate_flatcv.jpeg' \
run_benchmark "Rotate" \
  'gm convert imgs/parrot_hq.jpeg -rotate 90 tmp/rotate_gm.jpeg' \
  'magick convert imgs/parrot_hq.jpeg -rotate 90 tmp/rotate_magick.jpeg' \
  'vips rot imgs/parrot_hq.jpeg tmp/rotate_vips.jpeg d90'

run_benchmark "Resize" \
  './flatcv imgs/parrot_hq.jpeg resize 256x256 tmp/resize_flatcv.jpeg' \
  'gm convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_gm.jpeg' \
  'magick convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_magick.jpeg' \
  'vips thumbnail imgs/parrot_hq.jpeg tmp/resize_vips.jpeg 256 --height 256 --size force'

run_benchmark "Resize 50%" \
  './flatcv imgs/parrot_hq.jpeg resize 50% tmp/resize_50_flatcv.jpeg' \
  'gm convert imgs/parrot_hq.jpeg -resize 50% tmp/resize_50_gm.jpeg' \
  'magick convert imgs/parrot_hq.jpeg -resize 50% tmp/resize_50_magick.jpeg' \
  'vips resize imgs/parrot_hq.jpeg tmp/resize_50_vips.jpeg 0.5'

run_benchmark "Crop" \
  './flatcv imgs/parrot_hq.jpeg crop 500x500+1200+1200 tmp/crop_flatcv.jpeg' \
  'gm convert imgs/parrot_hq.jpeg -crop 500x500+1200+1200 tmp/crop_gm.jpeg' \
  'magick convert imgs/parrot_hq.jpeg -crop 500x500+1200+1200 tmp/crop_magick.jpeg' \
  'vips crop imgs/parrot_hq.jpeg tmp/crop_vips.jpeg 1200 1200 500 500'

run_benchmark "Grayscale" \
  './flatcv imgs/village.jpeg grayscale tmp/gray_flatcv.jpeg' \
  'gm convert imgs/village.jpeg -colorspace Gray tmp/gray_gm.jpeg' \
  'magick convert imgs/village.jpeg -colorspace Gray tmp/gray_magick.jpeg' \
  'vips colourspace imgs/village.jpeg tmp/gray_vips.jpeg b-w'

run_benchmark "Blur" \
  './flatcv imgs/page_hq.png blur 21 tmp/blur_flatcv.jpeg' \
  'gm convert imgs/page_hq.png -blur 21x7 tmp/blur_gm.jpeg' \
  'magick convert imgs/page_hq.png -blur 21x7 tmp/blur_magick.jpeg' \
  'vips gaussblur imgs/page_hq.png tmp/blur_vips.jpeg 7'

run_benchmark "Sobel" \
  './flatcv imgs/page_hq.png sobel tmp/sobel_flatcv.jpeg' \
  'magick convert imgs/page_hq.png -define convolve:scale="!" \
          -define morphology:compose=Lighten \
          -morphology Convolve "Sobel:>" tmp/sobel_magick.jpeg' \
  'vips sobel imgs/page_hq.png tmp/sobel_vips.jpeg'

run_benchmark "Histogram RGB" \
  './flatcv imgs/parrot_hq.jpeg histogram tmp/histogram_rgb_flatcv.jpeg' \
  'magick convert imgs/parrot_hq.jpeg histogram:tmp/histogram_rgb_magick.jpeg'

run_benchmark "Histogram Gray" \
  './flatcv imgs/parrot_grayscale.jpeg histogram tmp/histogram_gray_flatcv.jpeg' \
  'magick convert imgs/parrot_grayscale.jpeg histogram:tmp/histogram_gray_magick.jpeg'
