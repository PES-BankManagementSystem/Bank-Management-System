"""Small deterministic helpers shared by the local performance scripts."""

from __future__ import annotations

import math
import os
import random
import tempfile
from pathlib import Path


def safe_database_path(value: str | Path) -> Path:
    """Only allow disposable perf databases under the operating system temp dir."""
    path = Path(value).expanduser().resolve()
    temp_root = Path(tempfile.gettempdir()).resolve()
    try:
        path.relative_to(temp_root)
    except ValueError as error:
        raise ValueError(f"database must be under the temporary directory {temp_root}") from error
    if not path.name.startswith("bms-perf-") or path.suffix != ".sqlite":
        raise ValueError("database filename must start with 'bms-perf-' and end with '.sqlite'")
    return path


def generated_history(account_index: int, seed: int = 61) -> list[tuple[str, int]]:
    """Return deterministic (transaction type, amount in paise) entries."""
    rng = random.Random(seed + account_index)
    count = 300 if account_index < 5 else 12
    entries: list[tuple[str, int]] = [("deposit", rng.randrange(20_000_000, 50_000_001))]
    for index in range(1, count):
        kind = "deposit" if index % 3 else "withdrawal"
        entries.append((kind, rng.randrange(10_000, 100_001)))
    return entries


def percentile(samples: list[float], fraction: float) -> float:
    """Nearest-rank percentile; input is expected to be non-empty."""
    if not samples:
        raise ValueError("at least one sample is required")
    if not 0 < fraction <= 1:
        raise ValueError("fraction must be in (0, 1]")
    ordered = sorted(samples)
    return ordered[math.ceil(fraction * len(ordered)) - 1]


def summarize(samples: list[float], threshold_ms: float) -> dict[str, float | int | str]:
    if not samples:
        raise ValueError("at least one sample is required")
    ordered = sorted(samples)
    middle = len(ordered) // 2
    median = ordered[middle] if len(ordered) % 2 else (ordered[middle - 1] + ordered[middle]) / 2
    return {
        "samples": len(samples),
        "min_ms": min(samples),
        "mean_ms": sum(samples) / len(samples),
        "median_ms": median,
        "p95_ms": percentile(samples, 0.95),
        "p99_ms": percentile(samples, 0.99),
        "max_ms": max(samples),
        "threshold_ms": threshold_ms,
        "verdict": "Pass" if max(samples) < threshold_ms else "Fail",
    }
