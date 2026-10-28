#!/usr/bin/env bash
# Fails if the repository contains secrets that must stay out of version control
# (SEC-3): private keys, certificates, .env files or local databases. Run by CI.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

status=0

# Files that are always secret or local.
tracked=$(git ls-files | grep -E '(^|/)(\.env(\..*)?|.*\.(pem|key|crt|p12|pfx|db|sqlite3?))$|(^|/)config\.local\.' || true)
if [ -n "$tracked" ]; then
    echo "These files must not be committed:"
    echo "$tracked"
    status=1
fi

# Private key material inside any tracked file (this script is excluded).
keys=$(git grep -lE -e '-----BEGIN ([A-Z]+ )?PRIVATE KEY-----' -- . ':!scripts/check-secrets.sh' || true)
if [ -n "$keys" ]; then
    echo "Private key material found in:"
    echo "$keys"
    status=1
fi

[ "$status" -eq 0 ] && echo "No committed secrets found."
exit "$status"
