#!/usr/bin/env bash
# End-to-end test: proves that a valid order COMMITS and an oversell order ROLLS
# BACK, leaving the database exactly as it was. This is the manual acceptance
# check from development, automated. Exits non-zero if any assertion fails.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MYSQL="/opt/homebrew/opt/mysql/bin/mysql"
SOCK="$ROOT/db/mysql.sock"

# Start the project-local DB (idempotent) and reset to a known clean state.
"$ROOT/db/start-db.sh" >/dev/null
"$MYSQL" --socket="$SOCK" -u root ordersdb < "$ROOT/db/schema.sql"
"$MYSQL" --socket="$SOCK" -u root ordersdb < "$ROOT/db/seed.sql"

# Build the app.
cmake -S "$ROOT" -B "$ROOT/build" >/dev/null
cmake --build "$ROOT/build" >/dev/null

# Helper: run a scalar SQL query and print the raw value.
q() { "$MYSQL" --socket="$SOCK" -u root -N -e "$1"; }

# Seed baseline: product 1 (USB-C) stock 100, product 4 (Webcam) stock 3.
webcam_before="$(q "SELECT stock_quantity FROM ordersdb.products WHERE id = 4")"

# 1) A VALID order: customer 1 buys 2x product 1 and 1x product 3.  Should COMMIT.
"$ROOT/build/oms" >/dev/null 2>&1 <<'INPUT'
5
1
1
2
3
1
0
0
INPUT

# 2) An OVERSELL order: customer 1 wants 10x product 4 (only 3 in stock).
#    Should FAIL the stock check and ROLL BACK — no order row, no stock change.
"$ROOT/build/oms" >/dev/null 2>&1 <<'INPUT'
5
1
4
10
0
0
INPUT

# Collect final state.
usbc_after="$(q "SELECT stock_quantity FROM ordersdb.products WHERE id = 1")"
webcam_after="$(q "SELECT stock_quantity FROM ordersdb.products WHERE id = 4")"
order_count="$(q "SELECT COUNT(*) FROM ordersdb.orders")"
order_total="$(q "SELECT total_amount FROM ordersdb.orders ORDER BY id LIMIT 1")"

fail=0
check() { # description  expected  actual
    if [ "$2" = "$3" ]; then
        printf '  PASS  %-46s (%s)\n' "$1" "$3"
    else
        printf '  FAIL  %-46s expected %s, got %s\n' "$1" "$2" "$3"
        fail=1
    fi
}

echo "Assertions:"
check "valid order committed: USB-C stock 100 -> 98" 98 "$usbc_after"
check "oversell rolled back: Webcam stock unchanged"  "$webcam_before" "$webcam_after"
check "exactly one order persisted"                   1 "$order_count"
check "order total (2*9.99 + 249.00) is exact"        268.98 "$order_total"

echo
if [ "$fail" -eq 0 ]; then
    echo "OK — commit and rollback both behave correctly."
else
    echo "FAILED — see assertions above."
fi
exit "$fail"
