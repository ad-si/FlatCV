## Benchmark

Use the [benchmark script](https://github.com/ad-si/FlatCV/blob/main/benchmark.sh)
to run the benchmark yourself.

```txt
===== ▶️ Resize =====

./flatcv imgs/parrot_hq.jpeg resize 256x256 tmp/resize_flatcv.png ran
1.42 ± 0.01 times faster than gm convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_gm.png
2.39 ± 0.05 times faster than magick convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_magick.png

===== ▶️ Crop =====

gm convert imgs/parrot_hq.jpeg -crop 100x100+200+200 tmp/crop_gm.png ran
1.30 ± 0.04 times faster than magick convert imgs/parrot_hq.jpeg -crop 100x100+200+200 tmp/crop_magick.png
1.30 ± 0.04 times faster than ./flatcv imgs/parrot_hq.jpeg crop 100 100 100 100 tmp/crop_flatcv.png

===== ▶️ Grayscale =====

gm convert imgs/village.jpeg -colorspace Gray tmp/gray_gm.png ran
1.76 ± 0.02 times faster than ./flatcv imgs/village.jpeg grayscale tmp/gray_flatcv.png
3.83 ± 0.04 times faster than magick convert imgs/village.jpeg -colorspace Gray tmp/gray_magick.png

===== ▶️ Blur =====

./flatcv imgs/page_hq.png blur 21 tmp/blur_flatcv.png ran
1.66 ± 0.01 times faster than gm convert imgs/page_hq.png -blur 21x7 tmp/blur_gm.png
2.20 ± 0.01 times faster than magick convert imgs/page_hq.png -blur 21x7 tmp/blur_magick.png

===== ▶️ Sobel =====

./flatcv imgs/page_hq.png sobel tmp/sobel_flatcv.png ran
3.07 ± 0.05 times faster than magick convert imgs/page_hq.png -define convolve:scale="!" \
        -define morphology:compose=Lighten \
        -morphology Convolve "Sobel:>" tmp/sobel_magick.png
```
