#!/usr/bin/env bash
set -euo pipefail
PORT="${PORT:-8080}"
echo "Serving on http://localhost:$PORT  (Ctrl-C to stop)"
python3 -m http.server "$PORT"