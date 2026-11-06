# Code Coverage Report — S-42 (#118)

## Required S-40 CI result

The S-40 coverage job completed successfully on **`31f1cf48659dbe1a559d6e5eeb321b1d30e16281`** on 2026-11-05. It used `gcovr==8.8` and published the `coverage-report` artifact (ID `13387499446`) on the [workflow run page](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/41966709060). The associated [job](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/41966709060/job/127045274980) is successful.

**S-40 CI line and function percentages: NOT AVAILABLE: the public artifact metadata is readable, but downloading its archive returned HTTP 401; download `coverage-report` from the linked run page using an authenticated GitHub session and read `summary.json` to fill the CI column.** No CI percentages are inferred from the local run.

| Module | S-40 CI line coverage | S-40 CI function coverage |
|---|---:|---:|
| account | NOT AVAILABLE | NOT AVAILABLE |
| api | NOT AVAILABLE | NOT AVAILABLE |
| audit | NOT AVAILABLE | NOT AVAILABLE |
| auth | NOT AVAILABLE | NOT AVAILABLE |
| common | NOT AVAILABLE | NOT AVAILABLE |
| customer | NOT AVAILABLE | NOT AVAILABLE |
| main | NOT AVAILABLE | NOT AVAILABLE |
| reporting | NOT AVAILABLE | NOT AVAILABLE |
| staff | NOT AVAILABLE | NOT AVAILABLE |
| transaction | NOT AVAILABLE | NOT AVAILABLE |
| **Overall** | **NOT AVAILABLE** | **NOT AVAILABLE** |

The HTML report generated locally was **5.4 MB**, above the requested ~5 MB limit, so it is not included in the repository. Use the linked CI artifact for the HTML report.

## Tool, command and exclusions

- **Coverage tool:** gcovr 8.8 (pinned by the S-40 workflow).
- **Local compiler/toolchain:** AppleClang 17.0.0 on arm64 macOS, with `llvm-cov gcov`; these are not the Ubuntu CI toolchain. Local percentages below are supplemental and can differ from CI.
- **CI command:** install with `pipx install gcovr==8.8`, then run `./scripts/coverage.sh`.
- **Coverage instrumentation:** `-DBMS_COVERAGE=ON`, which adds `--coverage -fprofile-update=atomic -O0 -g` to first-party targets.
- **gcovr options:** `--root . --filter 'src/' --gcov-executable 'gcov' --exclude-throw-branches --exclude-unreachable-branches`; CI writes HTML, `summary.json`, and `summary.md` under `build-coverage/coverage/`.
- **Exclusions:** `src/` is the only inclusion filter. Therefore tests, fetched dependencies, and system headers outside `src/` are not counted. Throw branches and compiler-marked unreachable branches are explicitly excluded. There are no per-module exclusions.
- **CI compiler version:** **NOT AVAILABLE: the workflow uses moving `ubuntu-latest` and does not pin GCC/gcov; retrieve the S-40 job log or pin and report the runner toolchain version.**

The local gcovr 8.8 setup and run used:

```sh
python3 -m pip install --target /private/tmp/bms-gcovr-8-8 gcovr==8.8
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DBMS_COVERAGE=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_CXX_FLAGS='-isystem /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1'
env PATH=/private/tmp/bms-gcovr-8-8/bin:/opt/anaconda3/bin:/Library/Developer/CommandLineTools/usr/bin:/usr/bin:/bin \
  PYTHONPATH=/private/tmp/bms-gcovr-8-8 GCOV='llvm-cov gcov' COVERAGE_JOBS=2 \
  ./scripts/coverage.sh
```

CMake needed the active Xcode SDK's libc++ include path on this shell. No coverage flags, production source, or exclusions were changed.

## Local macOS coverage, before and after added tests

Both measurements use the same source commit, AppleClang/LLVM gcov and gcovr 8.8. **Before** is the original 29-target suite. **After** includes the S-42 test-only additions (31 targets). Counts and percentages below are copied from gcovr's `summary.md`/`summary.json`; percentages are not recomputed.

| Module | Line before | Function before | Line after | Function after |
|---|---:|---:|---:|---:|
| account | 91.9% (679/739) | 100.0% (54/54) | 92.0% (680/739) | 100.0% (54/54) |
| api | 87.2% (163/187) | 95.7% (44/46) | 88.2% (165/187) | 95.7% (44/46) |
| audit | 100.0% (91/91) | 90.0% (9/10) | 100.0% (91/91) | 90.0% (9/10) |
| auth | 93.6% (294/314) | 100.0% (33/33) | 93.6% (294/314) | 100.0% (33/33) |
| common | 77.7% (286/368) | 85.1% (63/74) | 80.2% (295/368) | 87.8% (65/74) |
| customer | 96.7% (614/635) | 100.0% (56/56) | 96.7% (614/635) | 100.0% (56/56) |
| main | 12.7% (7/55) | 20.0% (1/5) | 18.2% (10/55) | 20.0% (1/5) |
| reporting | 95.8% (504/526) | 100.0% (33/33) | 95.8% (504/526) | 100.0% (33/33) |
| staff | 97.2% (243/250) | 100.0% (15/15) | 97.2% (243/250) | 100.0% (15/15) |
| transaction | 96.8% (333/344) | 100.0% (15/15) | 97.4% (335/344) | 100.0% (15/15) |
| **Overall** | **91.6% (3214/3509)** | **94.7% (323/341)** | **92.1% (3231/3509)** | **95.3% (325/341)** |

Common moved above the 80% line threshold after the added closed-database tests. The `main` entry point remains below 80%; its server startup and signal-driven serving lifecycle are integration behavior, and the new help check covers only a safe, deterministic CLI branch. No module's local coverage is presented as the required S-40 CI number.

## NOT AVAILABLE values and how to fill them

- **S-40 line/function values by module and overall:** download the linked artifact while signed in, then copy those values from `summary.json` into the CI column above.
- **CI GCC/gcov version:** inspect the linked S-40 build log; the workflow currently does not pin that toolchain.
