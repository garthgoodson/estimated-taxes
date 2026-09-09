#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")" && pwd)
"$root/build.sh"

"$root/build/backend/estimated_taxes_backend" &
backend_pid=$!
frontend_pid=''

cleanup() {
  kill "$backend_pid" "$frontend_pid" 2>/dev/null || true
  wait "$backend_pid" 2>/dev/null || true
  wait "$frontend_pid" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT TERM

port=8080
settings="$HOME/.fi-estaxes/settings.json"
if [[ -f "$settings" ]]; then
  port=$(node -e '
    const settings = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
    if (!Number.isInteger(settings.port) || settings.port < 1 || settings.port > 65535) process.exit(1);
    process.stdout.write(String(settings.port));
  ' "$settings")
fi

cd "$root/frontend"
NUXT_BACKEND_ORIGIN="http://127.0.0.1:$port" pnpm dev &
frontend_pid=$!
wait "$frontend_pid"
