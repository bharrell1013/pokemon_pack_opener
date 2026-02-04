#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUTPUT_DIR=${1:-"${ROOT_DIR}/docs"}

cd "$ROOT_DIR"

mkdir -p "$OUTPUT_DIR"

emcc $(find src -name '*.cpp') \
  -std=c++17 \
  -O2 \
  -s USE_SDL=2 \
  -s FULL_ES3=1 \
  -s MIN_WEBGL_VERSION=2 \
  -s MAX_WEBGL_VERSION=2 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -Iinclude \
  -Isrc \
  --preload-file shaders \
  --preload-file textures \
  --preload-file models \
  --shell-file web/shell.html \
  -o "${OUTPUT_DIR}/index.html"

cp web/style.css "${OUTPUT_DIR}/style.css"

: > "${OUTPUT_DIR}/.nojekyll"

echo "Web build created in ${OUTPUT_DIR}"
