#!/usr/bin/env bash
set -euo pipefail

missing=()
for tool in gcc sqlite3 curl; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    missing+=("$tool")
  fi
done
if ((${#missing[@]} > 0)); then
  printf 'Missing required tool(s): %s\n' "${missing[*]}" >&2
  printf 'Install GCC, SQLite, and curl, then run ./start.sh again.\n' >&2
  exit 1
fi

mkdir -p data
bash app/get-mongoose.sh
gcc -std=gnu11 -D_GNU_SOURCE -Wall -Wextra -Werror -O2 app/main.c app/database.c app/mongoose.c \
  -lsqlite3 -o app/equipment-library

port="${PORT:-8080}"
printf 'Starting Equipment Lending Library at http://localhost:%s/\n' "$port"
exec env PORT="$port" ./app/equipment-library
