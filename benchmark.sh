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

run_benchmark "Resize" \
  './flatcv imgs/parrot_hq.jpeg resize 256x256 tmp/resize_flatcv.png' \
  'gm convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_gm.png' \
  'magick convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_magick.png'

run_benchmark "Crop" \
  './flatcv imgs/parrot_hq.jpeg crop 100 100 100 100 tmp/crop_flatcv.png' \
  'gm convert imgs/parrot_hq.jpeg -crop 100x100+200+200 tmp/crop_gm.png' \
  'magick convert imgs/parrot_hq.jpeg -crop 100x100+200+200 tmp/crop_magick.png'

run_benchmark "Grayscale" \
  './flatcv imgs/village.jpeg grayscale tmp/gray_flatcv.png' \
  'gm convert imgs/village.jpeg -colorspace Gray tmp/gray_gm.png' \
  'magick convert imgs/village.jpeg -colorspace Gray tmp/gray_magick.png'

run_benchmark "Blur" \
  './flatcv imgs/page_hq.png blur 21 tmp/blur_flatcv.png' \
  'gm convert imgs/page_hq.png -blur 21x7 tmp/blur_gm.png' \
  'magick convert imgs/page_hq.png -blur 21x7 tmp/blur_magick.png'

run_benchmark "Sobel" \
  './flatcv imgs/page_hq.png sobel tmp/sobel_flatcv.png' \
  'magick convert imgs/page_hq.png -define convolve:scale="!" \
          -define morphology:compose=Lighten \
          -morphology Convolve "Sobel:>" tmp/sobel_magick.png'
