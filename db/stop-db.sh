#!/usr/bin/env bash
# Cleanly shuts down the project-local MySQL instance.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MYSQLADMIN="/opt/homebrew/opt/mysql/bin/mysqladmin"
SOCK="$HERE/mysql.sock"

# Ask the server to shut down via the local socket as root (no password locally).
if "$MYSQLADMIN" --socket="$SOCK" -u root shutdown >/dev/null 2>&1; then
  echo "MySQL stopped."
else
  echo "MySQL was not running."
fi
