#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COMPILER="${GLSLANG_VALIDATOR:-glslangValidator}"
OUT="${1:-$ROOT/build/shaders}"
mkdir -p "$OUT"
command -v "$COMPILER" >/dev/null 2>&1 || {
  echo "glslangValidator not found; install Vulkan SDK or set GLSLANG_VALIDATOR." >&2
  exit 2
}
for shader in "$ROOT"/shaders/glsl/*.comp; do
  name="$(basename "$shader" .comp)"
  "$COMPILER" -V --target-env vulkan1.1 -S comp "$shader" -o "$OUT/$name.spv"
  if command -v spirv-val >/dev/null 2>&1; then
    spirv-val --target-env vulkan1.1 "$OUT/$name.spv"
  fi
done
echo "Compiled shaders into $OUT"
