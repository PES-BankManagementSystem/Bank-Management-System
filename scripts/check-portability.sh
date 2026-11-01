#!/usr/bin/env bash
# Fails if business logic contains platform-specific code paths (NFR-9, S-31).
# Platform differences belong in src/common (shared helpers) or src/api
# (sockets); the modules listed below must build the same on every OS.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

modules=(src/account src/audit src/auth src/customer src/reporting src/staff src/transaction)
pattern='_WIN32|_WIN64|_MSC_VER|__MINGW|__CYGWIN__|__linux__|__unix__|__APPLE__|__MACH__|<windows\.h>|<unistd\.h>|<sys/'

found=$(git grep -nE "$pattern" -- "${modules[@]}" || true)
if [ -n "$found" ]; then
    echo "Platform-specific code found in business logic (move it to src/common):"
    echo "$found"
    exit 1
fi
echo "No platform-specific code paths in business logic."
