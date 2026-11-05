#!/usr/bin/env python3
"""Prints gcovr's --json-summary as a Markdown table, overall and per module.

A module is a folder directly under src/ (NFR-8); src/main.cpp is listed as "main".
"""
import json
import sys
from pathlib import PurePosixPath

METRICS = ("line", "function", "branch")


def module_of(filename):
    parts = PurePosixPath(filename).parts
    return parts[1] if len(parts) > 2 else PurePosixPath(filename).stem


def cell(covered, total):
    percent = 100 * covered / total if total else 100
    return f"{percent:.1f}% ({covered}/{total})"


def row(name, counts):
    cells = [cell(counts[f"{m}_covered"], counts[f"{m}_total"]) for m in METRICS]
    return f"| {name} | " + " | ".join(cells) + " |"


def main(path):
    with open(path, encoding="utf-8") as summary:
        report = json.load(summary)

    modules = {}
    for entry in report["files"]:
        counts = modules.setdefault(module_of(entry["filename"]), {})
        for metric in METRICS:
            for kind in ("covered", "total"):
                key = f"{metric}_{kind}"
                counts[key] = counts.get(key, 0) + entry[key]

    print("## Coverage of `src/`\n")
    print("| Module | Line | Function | Branch |")
    print("|---|---|---|---|")
    for name in sorted(modules):
        print(row(name, modules[name]))
    print(row("**Total**", report))


if __name__ == "__main__":
    main(sys.argv[1])
