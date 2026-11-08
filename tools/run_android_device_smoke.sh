#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build-android}"
DEVICE_DIR="/data/local/tmp/aromafg"
REPORT="${2:-aromafg-device-report.txt}"

if ! command -v adb >/dev/null 2>&1; then
  echo "adb is required (Android Platform Tools)." >&2
  exit 2
fi
if [[ ! -f "$BUILD_DIR/libaroma_fg.so" || ! -x "$BUILD_DIR/aroma_fg_device_smoke" ]]; then
  echo "Expected $BUILD_DIR/libaroma_fg.so and executable aroma_fg_device_smoke." >&2
  exit 2
fi

# Prefer provenance generated alongside the exact binaries. Never substitute
# the current checkout SHA: the user may be running a downloaded CI artifact.
BUILD_COMMIT="unknown"
BUILD_COMMIT_SOURCE="not embedded"
if [[ -s "$BUILD_DIR/aromafg-build-commit.txt" ]]; then
  BUILD_COMMIT="$(tr -d '\r\n' < "$BUILD_DIR/aromafg-build-commit.txt")"
  BUILD_COMMIT_SOURCE="build artifact metadata"
elif [[ -n "${AROMAFG_BUILD_COMMIT:-}" ]]; then
  BUILD_COMMIT="$AROMAFG_BUILD_COMMIT"
  BUILD_COMMIT_SOURCE="AROMAFG_BUILD_COMMIT environment variable"
fi
if [[ ! "$BUILD_COMMIT" =~ ^[0-9a-fA-F]{40}$ && "$BUILD_COMMIT" != "unknown" ]]; then
  echo "Invalid AromaFG build commit '$BUILD_COMMIT' (expected 40-character SHA)." >&2
  exit 2
fi

adb get-state >/dev/null

{
  echo "AromaFG Android Vulkan device report"
  echo "AromaFG binary commit: $BUILD_COMMIT"
  echo "Commit provenance: $BUILD_COMMIT_SOURCE"
  echo "Build directory: $BUILD_DIR"
  echo "Capture time (host UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo
  for key in \
    ro.product.manufacturer ro.product.model ro.product.device \
    ro.soc.manufacturer ro.soc.model ro.board.platform ro.hardware \
    ro.build.version.release ro.build.version.sdk ro.build.fingerprint \
    ro.build.version.security_patch; do
    printf '%s=' "$key"
    adb shell getprop "$key" | tr -d '\r'
  done
  echo "Kernel:"
  adb shell uname -a | tr -d '\r'
  echo
  echo "Vulkan smoke output:"
} > "$REPORT"

adb shell "mkdir -p '$DEVICE_DIR'"
adb push "$BUILD_DIR/libaroma_fg.so" "$DEVICE_DIR/libaroma_fg.so" >/dev/null
adb push "$BUILD_DIR/aroma_fg_device_smoke" "$DEVICE_DIR/aroma_fg_device_smoke" >/dev/null
adb shell "chmod 700 '$DEVICE_DIR/aroma_fg_device_smoke' && chmod 600 '$DEVICE_DIR/libaroma_fg.so'"

set +e
adb shell "cd '$DEVICE_DIR' && LD_LIBRARY_PATH='$DEVICE_DIR' ./aroma_fg_device_smoke" 2>&1 | tee -a "$REPORT"
status=${PIPESTATUS[0]}
set -e

echo >> "$REPORT"
echo "Smoke exit status: $status" | tee -a "$REPORT"
echo "Report saved to: $REPORT"
exit "$status"
