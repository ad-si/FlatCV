#!/bin/dash

print_header() {
  printf "\n===== ▶️ %s =====\n\n" "$1";
}


mkdir -p tmp

print_header "Grayscale"

hyperfine --style color \
  './flatcv imgs/village.jpeg grayscale tmp/gray_flatcv.png' \
  'gm convert imgs/village.jpeg -colorspace Gray tmp/gray_gm.png' \
  'magick convert imgs/village.jpeg -colorspace Gray tmp/gray_magick.png' \
| sed -n '/Summary/,$p'


print_header "Blur"

hyperfine --style color \
  './flatcv imgs/page_hq.png blur 21 tmp/blur_flatcv.png' \
  'gm convert imgs/page_hq.png -blur 21x7 tmp/blur_gm.png' \
  'magick convert imgs/page_hq.png -blur 21x7 tmp/blur_magick.png' \
| sed -n '/Summary/,$p'
