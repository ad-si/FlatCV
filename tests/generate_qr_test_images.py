#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["qrcode", "Pillow"]
# ///
"""Generate test images with QR codes on varied backgrounds."""

import math
import random
import string
import sys
from pathlib import Path

import qrcode
from PIL import Image, ImageDraw


CHARSET = string.ascii_letters + string.digits + "_-+"
OUTPUT_DIR = Path("tests/qr_codes_generated")

SIZES = [
    (400, 400),
    (640, 480),
    (800, 600),
    (1024, 768),
    (480, 640),
    (600, 800),
    (1920, 1080),
    (300, 300),
]


def random_text() -> str:
    length = random.randint(1, 30)
    return "".join(random.choice(CHARSET) for _ in range(length))


def bg_solid(img: Image.Image) -> None:
    color = tuple(random.randint(128, 255) for _ in range(3))
    img.paste(color, (0, 0, img.width, img.height))


def bg_gradient_horizontal(img: Image.Image) -> None:
    c1 = tuple(random.randint(128, 255) for _ in range(3))
    c2 = tuple(random.randint(128, 255) for _ in range(3))
    for x in range(img.width):
        t = x / max(img.width - 1, 1)
        r = int(c1[0] + (c2[0] - c1[0]) * t)
        g = int(c1[1] + (c2[1] - c1[1]) * t)
        b = int(c1[2] + (c2[2] - c1[2]) * t)
        ImageDraw.Draw(img).line([(x, 0), (x, img.height)], fill=(r, g, b))


def bg_gradient_vertical(img: Image.Image) -> None:
    c1 = tuple(random.randint(128, 255) for _ in range(3))
    c2 = tuple(random.randint(128, 255) for _ in range(3))
    for y in range(img.height):
        t = y / max(img.height - 1, 1)
        r = int(c1[0] + (c2[0] - c1[0]) * t)
        g = int(c1[1] + (c2[1] - c1[1]) * t)
        b = int(c1[2] + (c2[2] - c1[2]) * t)
        ImageDraw.Draw(img).line([(0, y), (img.width, y)], fill=(r, g, b))


def bg_checkerboard(img: Image.Image) -> None:
    draw = ImageDraw.Draw(img)
    tile = random.randint(20, 60)
    c1 = tuple(random.randint(180, 255) for _ in range(3))
    c2 = tuple(random.randint(180, 255) for _ in range(3))
    for y in range(0, img.height, tile):
        for x in range(0, img.width, tile):
            color = c1 if (x // tile + y // tile) % 2 == 0 else c2
            draw.rectangle([x, y, x + tile, y + tile], fill=color)


def bg_stripes(img: Image.Image) -> None:
    draw = ImageDraw.Draw(img)
    stripe_w = random.randint(10, 40)
    c1 = tuple(random.randint(180, 255) for _ in range(3))
    c2 = tuple(random.randint(180, 255) for _ in range(3))
    for x in range(0, img.width, stripe_w):
        color = c1 if (x // stripe_w) % 2 == 0 else c2
        draw.rectangle([x, 0, x + stripe_w, img.height], fill=color)


def bg_noise(img: Image.Image) -> None:
    pixels = img.load()
    for y in range(img.height):
        for x in range(img.width):
            pixels[x, y] = tuple(random.randint(160, 255) for _ in range(3))


def bg_radial(img: Image.Image) -> None:
    cx, cy = img.width / 2, img.height / 2
    max_dist = math.hypot(cx, cy)
    c1 = tuple(random.randint(200, 255) for _ in range(3))
    c2 = tuple(random.randint(128, 200) for _ in range(3))
    pixels = img.load()
    for y in range(img.height):
        for x in range(img.width):
            t = min(math.hypot(x - cx, y - cy) / max_dist, 1.0)
            pixels[x, y] = tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3))


BACKGROUNDS = [
    bg_solid,
    bg_gradient_horizontal,
    bg_gradient_vertical,
    bg_checkerboard,
    bg_stripes,
    bg_noise,
    bg_radial,
]


def make_qr(text: str) -> Image.Image:
    qr = qrcode.QRCode(
        error_correction=qrcode.constants.ERROR_CORRECT_L,
        box_size=10,
        border=0,
    )
    qr.add_data(text)
    qr.make(fit=True)
    img = qr.make_image(fill_color="black", back_color="white").convert("RGBA")
    pixels = img.load()
    for y in range(img.height):
        for x in range(img.width):
            if pixels[x, y][:3] == (255, 255, 255):
                pixels[x, y] = (0, 0, 0, 0)
    return img


def place_qr(
    bg: Image.Image, qr_img: Image.Image
) -> tuple[Image.Image, list[tuple[int, int]]]:
    """Place QR on background. Returns (image, corners) where corners are
    the 4 boundary points of the QR code in clockwise order starting top-left."""
    angle = random.uniform(0, 360)
    rad = math.radians(angle % 90)
    expand_factor = math.cos(rad) + math.sin(rad)

    target = min(bg.width, bg.height) * random.uniform(0.3, 0.8)
    qr_side = int(target / expand_factor)
    qr_side = max(qr_side, 1)

    qr_scaled = qr_img.resize((qr_side, qr_side), Image.NEAREST)
    qr_rotated = qr_scaled.rotate(angle, resample=Image.BICUBIC, expand=True)

    max_x = bg.width - qr_rotated.width
    max_y = bg.height - qr_rotated.height
    x = random.randint(0, max(max_x, 0))
    y = random.randint(0, max(max_y, 0))

    result = bg.copy().convert("RGBA")
    result.paste(qr_rotated, (x, y), mask=qr_rotated)

    s = qr_side
    cx_local = s / 2.0
    cy_local = s / 2.0
    cx_img = x + qr_rotated.width / 2.0
    cy_img = y + qr_rotated.height / 2.0
    theta = math.radians(angle)
    cos_a = math.cos(theta)
    sin_a = math.sin(theta)
    raw_corners = [
        (-cx_local, -cy_local),
        (cx_local, -cy_local),
        (cx_local, cy_local),
        (-cx_local, cy_local),
    ]
    corners = []
    for dx, dy in raw_corners:
        rx = dx * cos_a - dy * sin_a + cx_img
        ry = dx * sin_a + dy * cos_a + cy_img
        corners.append((round(rx), round(ry)))

    return result.convert("RGB"), corners


def generate(n: int, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    for i in range(n):
        text = random_text()
        w, h = random.choice(SIZES)
        bg_fn = random.choice(BACKGROUNDS)

        bg = Image.new("RGB", (w, h))
        bg_fn(bg)

        qr_img = make_qr(text)
        result, corners = place_qr(bg, qr_img)

        coords = "_".join(f"{x}-{y}" for x, y in corners)
        fname = f"{text}_{coords}.png"
        result.save(output_dir / fname)
        print(f"[{i+1}/{n}] {w}x{h} {bg_fn.__name__:25s} -> {fname}")


if __name__ == "__main__":
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    random.seed(seed)
    generate(count, OUTPUT_DIR)
    print(f"\nDone. {count} images in {OUTPUT_DIR}/")
