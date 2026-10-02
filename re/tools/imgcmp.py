"""Tolerant screenshot comparison for screen-state checks (not pixel-exact parity).

Both images are reduced to 160x120 grayscale and compared by mean absolute difference (0..255).
Animated elements (cursor, highlights) stay well under the threshold; a different screen does not.
"""
from __future__ import annotations

import time

import numpy as np
from PIL import Image

SIZE = (160, 120)
DEFAULT_THRESHOLD = 12.0


def _norm(img: Image.Image) -> np.ndarray:
    return np.asarray(img.convert("L").resize(SIZE, Image.BILINEAR), dtype=np.float32)


def load(path) -> Image.Image:
    return Image.open(path).copy()


def distance(a: Image.Image, b: Image.Image) -> float:
    return float(np.abs(_norm(a) - _norm(b)).mean())


def matches(a: Image.Image, b: Image.Image, threshold: float = DEFAULT_THRESHOLD) -> bool:
    return distance(a, b) <= threshold


def wait_for(game, ref: Image.Image, timeout: float = 20, threshold: float = DEFAULT_THRESHOLD, poll: float = 0.5):
    """Poll the game window until it matches ref. Returns the matching screenshot; raises TimeoutError."""
    t0, best = time.time(), None
    while time.time() - t0 < timeout:
        if not game.alive:
            raise RuntimeError(f"game died while waiting for screen: {game.detach_reason}")
        shot = game.shot()
        d = distance(shot, ref)
        best = d if best is None else min(best, d)
        if d <= threshold:
            return shot
        time.sleep(poll)
    raise TimeoutError(f"screen never matched reference (best distance {best:.1f} > {threshold})")


def changed_fraction(a: Image.Image, b: Image.Image, delta: int = 60) -> float:
    """Fraction of pixels whose grayscale value moved by more than `delta` (full resolution).
    For small text (a date), the mean distance dilutes the change; this does not."""
    x = np.asarray(a.convert("L"), dtype=np.int16)
    y = np.asarray(b.convert("L"), dtype=np.int16)
    return float((np.abs(x - y) > delta).mean())
