# FlatCV

Simple image processing and computer vision library in pure C.

<table>
  <tr>
    <td><img src=imgs/elevation_2_basins_receipt.png width=500></td>
    <td><img src=imgs/receipt_corners.png width=500/></td>
    <td><img src=imgs/elevation_2_basins_receipt_watershed.png width=500></td>
  </tr>
</table>

"Simple" means:

- Color images are a flat array of 8-bit per channel RGBA row-major top-to-bottom
- Grayscale images are a flat array of 8-bit GRAY row-major top-to-bottom
- All operations are done on the raw sRGB pixel values
- Minimal usage of macros and preprocessor
- Available as an amalgamation where all code is combined into one file. \
    (Each [release](https://github.com/ad-si/FlatCV/releases)
    includes a `flatcv.h` and `flatcv.c` file.)
- No fusing of image transformations
- No multithreading \
    You're more likely to process one file per core
    than one file over multiple cores anyways.
    Yet, it's still often faster than GraphicsMagick with multiple threads.
    (See [benchmark](#benchmark) below.)
- No GPU acceleration
- Only uses functions from the C standard library.

<br/>

**You can find the code [on GitHub](https://github.com/ad-si/FlatCV)**


## Documentation

All examples in this documentation are executed during testing.
So you can be confident that they work as described!
