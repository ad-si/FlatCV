#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["qrcode", "Pillow", "pyzxing"]
# ///
"""Generate test images with QR codes on varied backgrounds.

Two modes:
  (default) Random mixed-difficulty dataset in tests/qr_codes_generated
  --difficulty Tiered dataset in tests/qr_codes_difficulty/level_N/
              with each image validated via pyzxing.
"""

import argparse
import math
import random
import string
from pathlib import Path

import qrcode
from PIL import Image, ImageDraw, ImageFilter


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


DIFFICULTY_DIR = Path("tests/qr_codes_difficulty")

ALNUM = string.ascii_letters + string.digits


def random_alnum_text(min_len: int = 3, max_len: int = 12) -> str:
    length = random.randint(min_len, max_len)
    return "".join(random.choice(ALNUM) for _ in range(length))


def make_qr_rgba(
    text: str, box_size: int = 10, border: int = 4
) -> Image.Image:
    """QR as RGBA with white kept (no transparency) so backgrounds stay clean."""
    qr = qrcode.QRCode(
        error_correction=qrcode.constants.ERROR_CORRECT_M,
        box_size=box_size,
        border=border,
    )
    qr.add_data(text)
    qr.make(fit=True)
    return qr.make_image(fill_color="black", back_color="white").convert("RGBA")


def add_gaussian_noise(img: Image.Image, sigma: float) -> Image.Image:
    """Add gaussian noise in-place to an RGB image."""
    pixels = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = pixels[x, y][:3]
            n = int(random.gauss(0.0, sigma))
            pixels[x, y] = (
                max(0, min(255, r + n)),
                max(0, min(255, g + n)),
                max(0, min(255, b + n)),
            )
    return img


def reduce_contrast(img: Image.Image, factor: float) -> Image.Image:
    """Compress intensity range toward gray. factor in (0, 1]."""
    pixels = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = pixels[x, y][:3]
            pixels[x, y] = (
                int(128 + (r - 128) * factor),
                int(128 + (g - 128) * factor),
                int(128 + (b - 128) * factor),
            )
    return img


def perspective_warp(
    img: Image.Image, magnitude: float
) -> Image.Image:
    """Apply perspective warp. magnitude is max corner offset fraction (0..0.3)."""
    w, h = img.size
    m = magnitude
    dst = [
        (random.uniform(0, m) * w, random.uniform(0, m) * h),
        (w - random.uniform(0, m) * w, random.uniform(0, m) * h),
        (w - random.uniform(0, m) * w, h - random.uniform(0, m) * h),
        (random.uniform(0, m) * w, h - random.uniform(0, m) * h),
    ]
    src = [(0, 0), (w, 0), (w, h), (0, h)]
    coeffs = _find_perspective_coeffs(dst, src)
    return img.transform(
        (w, h),
        Image.PERSPECTIVE,
        coeffs,
        resample=Image.BILINEAR,
        fillcolor=(255, 255, 255),
    )


def _find_perspective_coeffs(
    src: list[tuple[float, float]], dst: list[tuple[float, float]]
) -> list[float]:
    """Compute 8 coefficients for PIL's Image.PERSPECTIVE transform."""
    a = []
    b = []
    for (x, y), (X, Y) in zip(src, dst):
        a.append([X, Y, 1, 0, 0, 0, -x * X, -x * Y])
        a.append([0, 0, 0, X, Y, 1, -y * X, -y * Y])
        b.append(x)
        b.append(y)
    return _solve_8x8(a, b)


def _solve_8x8(a: list[list[float]], b: list[float]) -> list[float]:
    """Tiny Gaussian elimination solver for 8x8 systems."""
    n = 8
    m = [row[:] + [bv] for row, bv in zip(a, b)]
    for i in range(n):
        pivot = i
        for k in range(i + 1, n):
            if abs(m[k][i]) > abs(m[pivot][i]):
                pivot = k
        m[i], m[pivot] = m[pivot], m[i]
        for k in range(i + 1, n):
            factor = m[k][i] / m[i][i]
            for j in range(i, n + 1):
                m[k][j] -= factor * m[i][j]
    x = [0.0] * n
    for i in range(n - 1, -1, -1):
        s = m[i][n]
        for j in range(i + 1, n):
            s -= m[i][j] * x[j]
        x[i] = s / m[i][i]
    return x


def _place_simple(
    bg: Image.Image,
    qr: Image.Image,
    scale: float,
    angle: float,
) -> Image.Image:
    target = int(min(bg.width, bg.height) * scale)
    qr_scaled = qr.resize((target, target), Image.NEAREST)
    if abs(angle) > 0.01:
        qr_scaled = qr_scaled.rotate(
            angle, resample=Image.BICUBIC, expand=True, fillcolor=(255, 255, 255)
        )
    max_x = max(bg.width - qr_scaled.width, 0)
    max_y = max(bg.height - qr_scaled.height, 0)
    x = random.randint(0, max_x) if max_x > 0 else 0
    y = random.randint(0, max_y) if max_y > 0 else 0
    out = bg.copy().convert("RGBA")
    out.paste(qr_scaled, (x, y), mask=qr_scaled)
    return out.convert("RGB")


def _render_level(level: int, text: str) -> Image.Image:
    """Render one QR image at the given difficulty level (1-5)."""
    if level == 1:
        # Trivial: large QR, white background, axis aligned.
        w, h = random.choice([(400, 400), (512, 512), (640, 480)])
        bg = Image.new("RGB", (w, h), (255, 255, 255))
        qr = make_qr_rgba(text, box_size=10)
        return _place_simple(bg, qr, scale=random.uniform(0.55, 0.8), angle=0.0)

    if level == 2:
        # Easy: solid color bg, small rotation.
        w, h = random.choice([(480, 480), (640, 480), (800, 600)])
        bg = Image.new("RGB", (w, h))
        bg_solid(bg)
        qr = make_qr_rgba(text, box_size=10)
        return _place_simple(
            bg,
            qr,
            scale=random.uniform(0.4, 0.65),
            angle=random.uniform(-15, 15),
        )

    if level == 3:
        # Moderate: varied backgrounds, full rotation.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8)
        return _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )

    if level == 4:
        # Hard: blur + noise + smaller QR.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.25, 0.4),
            angle=random.uniform(0, 360),
        )
        img = img.filter(ImageFilter.GaussianBlur(radius=random.uniform(0.8, 1.6)))
        img = add_gaussian_noise(img, sigma=random.uniform(6.0, 12.0))
        return img

    # Level 5: very hard — perspective warp, low contrast, blur, small size.
    w, h = random.choice([(640, 480), (800, 600), (1024, 768)])
    bg = Image.new("RGB", (w, h))
    random.choice(BACKGROUNDS)(bg)
    qr = make_qr_rgba(text, box_size=8, border=4)
    img = _place_simple(
        bg,
        qr,
        scale=random.uniform(0.25, 0.35),
        angle=random.uniform(0, 360),
    )
    img = perspective_warp(img, magnitude=random.uniform(0.06, 0.14))
    img = img.filter(ImageFilter.GaussianBlur(radius=random.uniform(1.0, 1.8)))
    img = reduce_contrast(img, factor=random.uniform(0.55, 0.75))
    img = add_gaussian_noise(img, sigma=random.uniform(4.0, 9.0))
    return img


def _pyzxing_decodes(reader, path: Path, expected: str) -> bool:
    """Return True iff pyzxing decodes `expected` from the image."""
    try:
        results = reader.decode(str(path.resolve()))
    except Exception:
        return False
    if not results:
        return False
    for r in results:
        raw = r.get("parsed") or r.get("raw")
        if isinstance(raw, (bytes, bytearray)):
            try:
                raw = raw.decode("utf-8")
            except UnicodeDecodeError:
                continue
        if raw == expected:
            return True
    return False


def generate_difficulty(
    per_level: int,
    levels: list[int],
    output_dir: Path,
    max_attempts: int = 8,
) -> dict:
    """Generate tiered images, each validated by pyzxing. Returns stats.

    Filename format: <4-digit-index>_<text>.png. The expected text is always
    alphanumeric (no underscores), so splitting on the first underscore yields
    the index; everything after is the expected text.
    """
    from pyzxing import BarCodeReader

    reader = BarCodeReader()
    stats: dict[int, dict] = {}

    for level in levels:
        level_dir = output_dir / f"level_{level}"
        level_dir.mkdir(parents=True, exist_ok=True)
        for old in level_dir.glob("*.png"):
            old.unlink()

        accepted = 0
        rejected = 0
        safety = 0
        while accepted < per_level:
            safety += 1
            if safety > per_level * max_attempts * 3:
                break
            text = random_alnum_text()
            accepted_this_text = False
            for _ in range(max_attempts):
                try:
                    img = _render_level(level, text)
                except Exception:
                    continue
                fname = f"{accepted:04d}_{text}.png"
                fpath = level_dir / fname
                img.save(fpath)
                if _pyzxing_decodes(reader, fpath, text):
                    accepted += 1
                    accepted_this_text = True
                    break
                fpath.unlink(missing_ok=True)
                rejected += 1
            if not accepted_this_text:
                rejected += 1

        stats[level] = {
            "accepted": accepted,
            "rejected": rejected,
            "dir": str(level_dir),
        }
        print(
            f"Level {level}: {accepted} accepted, {rejected} rejected "
            f"(pyzxing-validated) -> {level_dir}"
        )
    return stats


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--difficulty",
        action="store_true",
        help="Generate tiered, pyzxing-validated dataset.",
    )
    p.add_argument(
        "--per-level",
        type=int,
        default=30,
        help="Images per difficulty level (default: 30).",
    )
    p.add_argument(
        "--levels",
        type=str,
        default="1,2,3,4,5",
        help="Comma-separated list of levels to generate (default: 1,2,3,4,5).",
    )
    p.add_argument("--seed", type=int, default=42)
    p.add_argument(
        "--count",
        type=int,
        default=20,
        help="Count for random mode (ignored in --difficulty).",
    )
    return p.parse_args()


if __name__ == "__main__":
    args = parse_args()
    random.seed(args.seed)

    if args.difficulty:
        levels = [int(x) for x in args.levels.split(",") if x.strip()]
        generate_difficulty(args.per_level, levels, DIFFICULTY_DIR)
        print(f"\nDone. Difficulty dataset in {DIFFICULTY_DIR}/")
    else:
        generate(args.count, OUTPUT_DIR)
        print(f"\nDone. {args.count} images in {OUTPUT_DIR}/")
