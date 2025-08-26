## Benchmark

Use the [benchmark script](https://github.com/ad-si/FlatCV/blob/main/benchmark.sh)
to run the benchmark yourself.

```txt
===== ▶️ Rotate =====
vips rot imgs/parrot_hq.jpeg tmp/rotate_vips.jpeg d90 ran
1.12 ± 0.08 times faster than gm convert imgs/parrot_hq.jpeg -rotate 90 tmp/rotate_gm.jpeg
2.30 ± 0.15 times faster than magick convert imgs/parrot_hq.jpeg -rotate 90 tmp/rotate_magick.jpeg

===== ▶️ Resize =====
vips thumbnail imgs/parrot_hq.jpeg tmp/resize_vips.jpeg 256 --height 256 --size force ran
1.27 ± 0.10 times faster than ./flatcv imgs/parrot_hq.jpeg resize 256x256 tmp/resize_flatcv.jpeg
1.50 ± 0.12 times faster than gm convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_gm.jpeg
2.72 ± 0.21 times faster than magick convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_magick.jpeg

===== ▶️ Resize 50% =====
vips resize imgs/parrot_hq.jpeg tmp/resize_50_vips.jpeg 0.5 ran
1.39 ± 0.05 times faster than gm convert imgs/parrot_hq.jpeg -resize 50% tmp/resize_50_gm.jpeg
1.60 ± 0.05 times faster than ./flatcv imgs/parrot_hq.jpeg resize 50% tmp/resize_50_flatcv.jpeg
2.71 ± 0.10 times faster than magick convert imgs/parrot_hq.jpeg -resize 50% tmp/resize_50_magick.jpeg

===== ▶️ Crop =====
vips crop imgs/parrot_hq.jpeg tmp/crop_vips.jpeg 1200 1200 500 500 ran
1.10 ± 0.17 times faster than gm convert imgs/parrot_hq.jpeg -crop 500x500+1200+1200 tmp/crop_gm.jpeg
1.40 ± 0.09 times faster than magick convert imgs/parrot_hq.jpeg -crop 500x500+1200+1200 tmp/crop_magick.jpeg
1.43 ± 0.09 times faster than ./flatcv imgs/parrot_hq.jpeg crop 500x500+1200+1200 tmp/crop_flatcv.jpeg

===== ▶️ Grayscale =====
vips colourspace imgs/village.jpeg tmp/gray_vips.jpeg b-w ran
2.07 ± 0.11 times faster than gm convert imgs/village.jpeg -colorspace Gray tmp/gray_gm.jpeg
2.79 ± 0.25 times faster than magick convert imgs/village.jpeg -colorspace Gray tmp/gray_magick.jpeg
4.31 ± 0.28 times faster than ./flatcv imgs/village.jpeg grayscale tmp/gray_flatcv.jpeg

===== ▶️ Blur =====
vips gaussblur imgs/page_hq.png tmp/blur_vips.jpeg 7 ran
1.51 ± 0.04 times faster than gm convert imgs/page_hq.png -blur 21x7 tmp/blur_gm.jpeg
1.96 ± 0.07 times faster than ./flatcv imgs/page_hq.png blur 21 tmp/blur_flatcv.jpeg
3.61 ± 0.09 times faster than magick convert imgs/page_hq.png -blur 21x7 tmp/blur_magick.jpeg

===== ▶️ Sobel =====
./flatcv imgs/page_hq.png sobel tmp/sobel_flatcv.jpeg ran
1.02 ± 0.07 times faster than vips sobel imgs/page_hq.png tmp/sobel_vips.jpeg
3.99 ± 0.29 times faster than magick convert imgs/page_hq.png -define convolve:scale="!" \
        -define morphology:compose=Lighten \
        -morphology Convolve "Sobel:>" tmp/sobel_magick.jpeg

===== ▶️ Histogram RGB =====
./flatcv imgs/parrot_hq.jpeg histogram tmp/histogram_rgb_flatcv.jpeg ran
1.13 ± 0.03 times faster than magick convert imgs/parrot_hq.jpeg histogram:tmp/histogram_rgb_magick.jpeg

===== ▶️ Histogram Gray =====
./flatcv imgs/parrot_grayscale.jpeg histogram tmp/histogram_gray_flatcv.jpeg ran
14.88 ± 2.93 times faster than magick convert imgs/parrot_grayscale.jpeg histogram:tmp/histogram_gray_magick.jpeg
```
