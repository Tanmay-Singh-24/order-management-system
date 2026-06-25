#!/usr/bin/env bash
# Starts the PROJECT-LOCAL MySQL instance.
# Every bit of state (data files, socket, logs, pid) stays inside this folder,
# so nothing leaks into the rest of your system and nothing auto-starts at login.
set -euo pipefail

# Resolve the directory THIS script lives in (the db/ folder), regardless of
# where it's called from. Quoting handles the space in "DBMS PROJ".
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MYSQLD="/opt/homebrew/opt/mysql/bin/mysqld"          # the server binary
MYSQLADMIN="/opt/homebrew/opt/mysql/bin/mysqladmin"  # admin/ping client
DATADIR="$HERE/data"        # where InnoDB stores tables -> inside the project
SOCK="$HERE/mysql.sock"     # Unix-domain socket file for local clients
PIDFILE="$HERE/mysql.pid"   # holds the running server's process id
ERRLOG="$HERE/mysql.err"    # server error log
PORT=3307                   # non-default port so we never clash with a 3306 install

# If a server already answers on our socket, do nothing (idempotent start).
if [ -S "$SOCK" ] && "$MYSQLADMIN" --socket="$SOCK" ping >/dev/null 2>&1; then
  echo "MySQL already running (socket: $SOCK)"
  exit 0
fi

# nohup + & detaches the server so it survives this script exiting.
# --bind-address=127.0.0.1 keeps the CLASSIC protocol loopback-only.
# We KEEP the X Protocol enabled (port 33060) because MySQL Connector/C++'s
# X DevAPI -- the API our C++ app uses -- speaks that protocol.
# --mysqlx-bind-address=127.0.0.1 keeps the X protocol loopback-only too.
nohup "$MYSQLD" --no-defaults \
  --datadir="$DATADIR" \
  --socket="$SOCK" \
  --pid-file="$PIDFILE" \
  --log-error="$ERRLOG" \
  --port="$PORT" \
  --bind-address=127.0.0.1 \
  --mysqlx-port=33060 \
  --mysqlx-bind-address=127.0.0.1 >/dev/null 2>&1 &

# Poll up to ~30s until the server accepts connections.
for _ in $(seq 1 30); do
  if "$MYSQLADMIN" --socket="$SOCK" ping >/dev/null 2>&1; then
    echo "MySQL is UP on 127.0.0.1:$PORT  (socket: $SOCK)"
    exit 0
  fi
  sleep 1
done

echo "MySQL failed to start. Check the error log: $ERRLOG" >&2
exit 1
