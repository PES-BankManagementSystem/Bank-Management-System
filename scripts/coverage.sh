#!/usr/bin/env bash
# Builds with BMS_COVERAGE=ON, runs the unit tests and measures line, function
# and branch coverage of src/ with gcovr. Third-party code (build-coverage/_deps)
# and tests/ are left out. Run by CI; needs GCC or Clang and gcovr.
#
# With Clang, set GCOV="llvm-cov gcov". Reports are written to
# build-coverage/coverage/: index.html, summary.json and summary.md.
#
# The job count is fixed because a bare --parallel means unlimited jobs with
# Make, and these files are heavy enough to run a 16 GB machine out of memory.
# Set COVERAGE_JOBS to change it.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

build_dir=build-coverage
report_dir=$build_dir/coverage

cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DBMS_COVERAGE=ON
cmake --build "$build_dir" --parallel "${COVERAGE_JOBS:-2}"

# Counts from an earlier run would otherwise be added to this one.
find "$build_dir" -name '*.gcda' -delete
ctest --test-dir "$build_dir" --output-on-failure

rm -rf "$report_dir"
mkdir -p "$report_dir"
gcovr "$build_dir" \
    --root . \
    --filter 'src/' \
    --gcov-executable "${GCOV:-gcov}" \
    --exclude-throw-branches \
    --exclude-unreachable-branches \
    --html-details "$report_dir/index.html" \
    --html-title 'Bank Management System coverage' \
    --json-summary "$report_dir/summary.json" \
    --json-summary-pretty \
    --print-summary

python3 scripts/coverage-summary.py "$report_dir/summary.json" | tee "$report_dir/summary.md"
