#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["qrcode", "Pillow", "numpy", "zxing-cpp"]
# ///
"""Generate test images with QR codes on varied backgrounds.

Two modes:
  (default) Random mixed-difficulty dataset in tests/qr_codes_generated
  --difficulty Tiered dataset in tests/qr_codes_difficulty/level_N/

The difficulty pipeline has two execution paths:
  --validate    Use zxing-cpp to validate each candidate, retry rejected
                renders, then record the accept/reject pattern to
                tests/qr_codes_difficulty_manifest.json. Needs zxing-cpp
                (same algorithm as pyzxing's ZXing, C++ instead of Java, so
                no JVM startup cost per image).
  (default)     Read the manifest and replay the exact RNG consumption
                sequence the validated run made, so the same images are
                reproduced byte-for-byte without calling any decoder.
                Only needs qrcode + Pillow.

Workflow: run `--validate` once (whenever the generator code changes), commit
the refreshed manifest, then every subsequent regeneration is decoder-free.
"""

import argparse
import json
import math
import random
import string
from pathlib import Path

import numpy as np
import qrcode
from PIL import Image, ImageDraw, ImageFilter


MANIFEST_PATH = Path("tests/qr_codes_difficulty_manifest.json")


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


# Separate numpy RNG for bulk pixel-level draws. Keeps the Python `random`
# stream reserved for the manifest-relevant decisions (text, level params,
# scale, angle, background choice, color picks) — so those stay lockstep
# with the validated manifest even though the pixel fills are vectorised.
_NP_RNG = np.random.default_rng()


def _seed_rngs(seed: int) -> None:
    random.seed(seed)
    global _NP_RNG
    _NP_RNG = np.random.default_rng(seed)


def _paste_array(img: Image.Image, arr: np.ndarray) -> None:
    img.paste(Image.fromarray(arr, "RGB"))


def bg_solid(img: Image.Image) -> None:
    color = tuple(random.randint(128, 255) for _ in range(3))
    img.paste(color, (0, 0, img.width, img.height))


def bg_gradient_horizontal(img: Image.Image) -> None:
    c1 = np.array([random.randint(128, 255) for _ in range(3)], dtype=np.float32)
    c2 = np.array([random.randint(128, 255) for _ in range(3)], dtype=np.float32)
    t = np.linspace(0.0, 1.0, img.width, dtype=np.float32)[None, :, None]
    row = (c1 + (c2 - c1) * t).astype(np.uint8)
    arr = np.broadcast_to(row, (img.height, img.width, 3)).copy()
    _paste_array(img, arr)


def bg_gradient_vertical(img: Image.Image) -> None:
    c1 = np.array([random.randint(128, 255) for _ in range(3)], dtype=np.float32)
    c2 = np.array([random.randint(128, 255) for _ in range(3)], dtype=np.float32)
    t = np.linspace(0.0, 1.0, img.height, dtype=np.float32)[:, None, None]
    col = (c1 + (c2 - c1) * t).astype(np.uint8)
    arr = np.broadcast_to(col, (img.height, img.width, 3)).copy()
    _paste_array(img, arr)


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
    arr = _NP_RNG.integers(
        160, 256, size=(img.height, img.width, 3), dtype=np.uint8
    )
    _paste_array(img, arr)


def bg_radial(img: Image.Image) -> None:
    cx, cy = img.width / 2, img.height / 2
    max_dist = math.hypot(cx, cy)
    c1 = np.array([random.randint(200, 255) for _ in range(3)], dtype=np.float32)
    c2 = np.array([random.randint(128, 200) for _ in range(3)], dtype=np.float32)
    xs = np.arange(img.width, dtype=np.float32) - cx
    ys = np.arange(img.height, dtype=np.float32) - cy
    dist = np.clip(np.hypot(xs[None, :], ys[:, None]) / max_dist, 0.0, 1.0)
    arr = (c1 + (c2 - c1) * dist[..., None]).astype(np.uint8)
    _paste_array(img, arr)


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
    text: str,
    box_size: int = 10,
    border: int = 4,
    white_bg: bool = True,
) -> Image.Image:
    """QR as RGBA. If white_bg is True the quiet zone is opaque white and
    rotates together with the QR modules; if False the white pixels are
    converted to transparent so the photo background shows through between
    the dark modules (no white square frame at all)."""
    qr = qrcode.QRCode(
        error_correction=qrcode.constants.ERROR_CORRECT_M,
        box_size=box_size,
        border=border,
    )
    qr.add_data(text)
    qr.make(fit=True)
    img = qr.make_image(fill_color="black", back_color="white").convert("RGBA")
    if not white_bg:
        pixels = img.load()
        for y in range(img.height):
            for x in range(img.width):
                if pixels[x, y][:3] == (255, 255, 255):
                    pixels[x, y] = (0, 0, 0, 0)
    return img


def add_gaussian_noise(img: Image.Image, sigma: float) -> Image.Image:
    """Add gaussian noise to an RGB image (one sample per pixel, applied to
    all three channels like the original scalar implementation)."""
    arr = np.asarray(img, dtype=np.int16)
    noise = _NP_RNG.normal(0.0, sigma, size=arr.shape[:2]).astype(np.int16)
    arr = np.clip(arr + noise[..., None], 0, 255).astype(np.uint8)
    return Image.fromarray(arr, "RGB")


def reduce_contrast(img: Image.Image, factor: float) -> Image.Image:
    """Compress intensity range toward gray. factor in (0, 1]."""
    arr = np.asarray(img, dtype=np.float32)
    arr = 128.0 + (arr - 128.0) * factor
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")


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
        # Transparent fill so the rotated QR's bounding-box corners show the
        # photo background, not a white axis-aligned halo — the QR's own
        # quiet zone (if any) rotates with the modules.
        qr_scaled = qr_scaled.rotate(
            angle,
            resample=Image.BICUBIC,
            expand=True,
            fillcolor=(255, 255, 255, 0),
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
    # Randomly decide whether the QR gets an opaque white quiet zone
    # (that rotates with the modules) or a transparent quiet zone (so the
    # photo background shows directly between the dark modules).
    white_bg = random.random() < 0.5

    if level == 1:
        # Trivial: large QR, white background, axis aligned.
        w, h = random.choice([(400, 400), (512, 512), (640, 480)])
        bg = Image.new("RGB", (w, h), (255, 255, 255))
        qr = make_qr_rgba(text, box_size=10, white_bg=white_bg)
        return _place_simple(bg, qr, scale=random.uniform(0.55, 0.8), angle=0.0)

    if level == 2:
        # Easy: solid color bg, small rotation.
        w, h = random.choice([(480, 480), (640, 480), (800, 600)])
        bg = Image.new("RGB", (w, h))
        bg_solid(bg)
        qr = make_qr_rgba(text, box_size=10, white_bg=white_bg)
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
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
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
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
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
    qr = make_qr_rgba(text, box_size=8, border=4, white_bg=white_bg)
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


def _zxing_decodes(path: Path, expected: str) -> bool:
    """Return True iff zxing-cpp decodes `expected` from the image."""
    import zxingcpp

    try:
        img = Image.open(path)
        results = zxingcpp.read_barcodes(img)
    except Exception:
        return False
    for r in results:
        if r.text == expected:
            return True
    return False


def _clear_level_dir(output_dir: Path, level: int) -> Path:
    level_dir = output_dir / f"level_{level}"
    level_dir.mkdir(parents=True, exist_ok=True)
    for old in level_dir.glob("*.png"):
        old.unlink()
    return level_dir


def _validate_difficulty(
    seed: int,
    per_level: int,
    levels: list[int],
    output_dir: Path,
    max_attempts: int = 8,
) -> dict:
    """Generate + zxing-cpp validate + record the accept/reject pattern.

    The returned dict is the manifest: every text tried (accepted or dropped)
    is logged along with the number of render attempts it consumed and which
    attempt (if any) the validator accepted. Replaying this manifest reseeds
    the RNG with the recorded seed and consumes RNG in the same order, so
    the validator-free regeneration produces byte-identical images.
    """
    manifest: dict = {
        "seed": seed,
        "per_level": per_level,
        "max_attempts": max_attempts,
        "levels": {},
    }

    for level in levels:
        level_dir = _clear_level_dir(output_dir, level)
        entries: list[dict] = []
        accepted = 0
        safety = 0
        while accepted < per_level:
            safety += 1
            if safety > per_level * max_attempts * 3:
                break
            text = random_alnum_text()
            accepted_at = -1
            attempts = 0
            for attempt in range(max_attempts):
                attempts += 1
                try:
                    img = _render_level(level, text)
                except Exception:
                    continue
                fname = f"{accepted:04d}_{text}.png"
                fpath = level_dir / fname
                img.save(fpath)
                if _zxing_decodes(fpath, text):
                    accepted_at = attempt
                    accepted += 1
                    break
                fpath.unlink(missing_ok=True)
            # Default case (1 render, accepted on attempt 0) is encoded as
            # a bare string so the manifest doesn't carry 145 copies of the
            # same `"attempts": 1, "accepted_at": 0` boilerplate. Retries
            # and drops use an object with the explicit counts.
            entry: str | dict
            if attempts == 1 and accepted_at == 0:
                entry = text
            else:
                entry = {
                    "text": text,
                    "attempts": attempts,
                    "accepted_at": accepted_at,
                }
            entries.append(entry)

        manifest["levels"][str(level)] = entries
        kept = sum(
            1
            for e in entries
            if isinstance(e, str) or e.get("accepted_at", 0) >= 0
        )
        dropped = len(entries) - kept
        print(
            f"Level {level}: {kept} accepted, {dropped} texts dropped "
            f"(zxing-cpp-validated) -> {level_dir}"
        )

    return manifest


def _replay_difficulty(
    levels: list[int],
    output_dir: Path,
    manifest: dict,
) -> None:
    """Replay a validated manifest: consume RNG in the recorded order and
    save only the attempts the validator originally accepted. No decoder
    needed on the replay path."""
    level_entries = manifest["levels"]
    for level in levels:
        entries = level_entries.get(str(level))
        if entries is None:
            raise RuntimeError(
                f"Manifest has no entry for level_{level}; re-run with --validate"
            )
        level_dir = _clear_level_dir(output_dir, level)
        accepted = 0
        for entry in entries:
            if isinstance(entry, str):
                expected_text = entry
                attempts = 1
                accepted_at = 0
            else:
                expected_text = entry["text"]
                attempts = entry.get("attempts", 1)
                accepted_at = entry.get("accepted_at", 0)
            text = random_alnum_text()
            if text != expected_text:
                raise RuntimeError(
                    f"Manifest desync at level_{level} index {accepted}: "
                    f"expected text {expected_text!r}, got {text!r}. "
                    f"Re-run with --validate to refresh the manifest."
                )
            for attempt in range(attempts):
                img = _render_level(level, text)
                if attempt == accepted_at:
                    fname = f"{accepted:04d}_{text}.png"
                    img.save(level_dir / fname)
            if accepted_at >= 0:
                accepted += 1
        print(
            f"Level {level}: {accepted} regenerated (manifest-replay) -> {level_dir}"
        )


def generate_difficulty(
    seed: int,
    per_level: int,
    levels: list[int],
    output_dir: Path,
    validate: bool = False,
    max_attempts: int = 8,
) -> None:
    """Dispatch: validate with zxing-cpp (and write manifest) or replay."""
    if validate:
        _seed_rngs(seed)
        manifest = _validate_difficulty(
            seed, per_level, levels, output_dir, max_attempts
        )
        MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
        with MANIFEST_PATH.open("w") as f:
            json.dump(manifest, f, indent=2)
            f.write("\n")
        print(f"\nWrote validation manifest to {MANIFEST_PATH}")
        return
    if not MANIFEST_PATH.exists():
        raise RuntimeError(
            f"No validation manifest at {MANIFEST_PATH}. "
            f"Run once with --validate to create it."
        )
    with MANIFEST_PATH.open() as f:
        manifest = json.load(f)
    # Reseed from the manifest so replay always matches the validated run,
    # regardless of what --seed was passed on the command line.
    _seed_rngs(manifest["seed"])
    _replay_difficulty(levels, output_dir, manifest)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--difficulty",
        action="store_true",
        help="Generate tiered difficulty dataset (default: replay manifest).",
    )
    p.add_argument(
        "--validate",
        action="store_true",
        help="Re-run zxing-cpp validation and rewrite the manifest. "
        "Without this flag the generator replays the stored manifest.",
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

    if args.difficulty:
        # generate_difficulty owns RNG seeding: --validate uses args.seed,
        # replay uses the seed stored in the manifest.
        levels = [int(x) for x in args.levels.split(",") if x.strip()]
        generate_difficulty(
            args.seed,
            args.per_level,
            levels,
            DIFFICULTY_DIR,
            validate=args.validate,
        )
        print(f"\nDone. Difficulty dataset in {DIFFICULTY_DIR}/")
    else:
        _seed_rngs(args.seed)
        generate(args.count, OUTPUT_DIR)
        print(f"\nDone. {args.count} images in {OUTPUT_DIR}/")
