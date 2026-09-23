#!/usr/bin/env bash
set -euo pipefail

# ---- 0. Emscripten ---------------------------------------------------------
if ! command -v emcc >/dev/null 2>&1; then
  if [[ -n "${EMSDK:-}" ]]; then source "$EMSDK/emsdk_env.sh"
  else echo "emcc not found. Source your emsdk_env.sh first." >&2; exit 1; fi
fi

# ---- 1. Paths --------------------------------------------------------------
SRC_ROOT="${SRC_ROOT:-./c-code}"          # <-- change if your tree differs
LIB_COM="$SRC_ROOT/lib_com"
LIB_DEC="$SRC_ROOT/lib_dec"

# Some releases nest headers one level deeper; add both spellings.
INCLUDES=(-I "$LIB_COM" -I "$LIB_DEC")
for d in "$LIB_COM"/*/ "$LIB_DEC"/*/; do
  [[ -d "$d" ]] && INCLUDES+=(-I "$d")
done

# ---- 2. Copy our glue next to the reference sources ------------------------
cp -f wasm_main.c "$LIB_DEC/wasm_main.c"

# ---- 3. Compile ------------------------------------------------------------
emcc -O3 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=64MB \
  -s STACK_SIZE=4MB \
  -s FORCE_FILESYSTEM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createEvsDecModule \
  -s INVOKE_RUN=0 \
  -s EXIT_RUNTIME=0 \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","FS","HEAPU8","callMain"]' \
  -s EXPORTED_FUNCTIONS='["_main","_malloc","_free","_evs_mime_decode"]' \
  -s ENVIRONMENT='web,worker,node' \
  -Wno-everything \
  "${INCLUDES[@]}" \
  "$LIB_COM"/*.c \
  "$LIB_DEC"/*.c \
  -o evs_dec.js

echo
echo "Build complete:"
ls -la evs_dec.js evs_dec.wasm