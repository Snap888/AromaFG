#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build-android}"
DEVICE_DIR="/data/local/tmp/aromafg"
REPORT="${2:-aromafg-device-report.txt}"
ADB_SERIAL="${3:-${ADB_SERIAL:-}}"
BINARIES=(aroma_fg_contract_tests aroma_fg_vulkan_plan_tests aroma_fg_device_smoke)

if ! command -v adb >/dev/null 2>&1; then
  echo "adb is required (Android Platform Tools)." >&2
  exit 2
fi
for binary in libaroma_fg.so "${BINARIES[@]}"; do
  if [[ ! -f "$BUILD_DIR/$binary" || ( "$binary" != "libaroma_fg.so" && ! -x "$BUILD_DIR/$binary" ) ]]; then
    echo "Expected $BUILD_DIR/$binary (executable for test binaries)." >&2
    exit 2
  fi
done

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

# Resolve one authorized device explicitly; never let adb choose ambiguously.
mapfile -t CONNECTED_SERIALS < <(adb devices | awk 'NR > 1 && $2 == "device" { print $1 }')
if [[ -z "$ADB_SERIAL" ]]; then
  if [[ "${#CONNECTED_SERIALS[@]}" -ne 1 ]]; then
    echo "Expected exactly one authorized ADB device; found ${#CONNECTED_SERIALS[@]}." >&2
    adb devices -l >&2
    echo "Pass the target serial as argument 3 or set ADB_SERIAL." >&2
    exit 2
  fi
  ADB_SERIAL="${CONNECTED_SERIALS[0]}"
fi
if ! printf '%s\\n' "${CONNECTED_SERIALS[@]}" | grep -Fxq -- "$ADB_SERIAL"; then
  echo "ADB serial '$ADB_SERIAL' is not an authorized connected device." >&2
  adb devices -l >&2
  exit 2
fi
adb_target() { adb -s "$ADB_SERIAL" "$@"; }
adb_target get-state >/dev/null

DEVICE_ABI="$(adb_target shell getprop ro.product.cpu.abi | tr -d '\\r')"
DEVICE_ABILIST="$(adb_target shell getprop ro.product.cpu.abilist | tr -d '\\r')"
DEVICE_API="$(adb_target shell getprop ro.build.version.sdk | tr -d '\\r')"
if [[ "$DEVICE_ABI" != "arm64-v8a" && ",$DEVICE_ABILIST," != *",arm64-v8a,"* ]]; then
  echo "Device does not advertise arm64-v8a (primary='$DEVICE_ABI', list='$DEVICE_ABILIST')." >&2
  exit 2
fi
if [[ ! "$DEVICE_API" =~ ^[0-9]+$ ]] || (( DEVICE_API < 26 )); then
  echo "Android API $DEVICE_API is below the build minimum API 26." >&2
  exit 2
fi

{
  echo "AromaFG Android Vulkan device report"
  echo "ADB serial: $ADB_SERIAL"
  echo "Primary ABI: $DEVICE_ABI"
  echo "ABI list: $DEVICE_ABILIST"
  echo "Android API: $DEVICE_API"
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
    adb_target shell getprop "$key" | tr -d '\r'
  done
  echo "Kernel:"
  adb_target shell uname -a | tr -d '\r'
  echo
  echo "Device test output:"
} > "$REPORT"

adb_target shell "mkdir -p '$DEVICE_DIR'"
for binary in libaroma_fg.so "${BINARIES[@]}"; do
  adb_target push "$BUILD_DIR/$binary" "$DEVICE_DIR/$binary" >/dev/null
done
adb_target shell "chmod 700 '$DEVICE_DIR/aroma_fg_contract_tests' '$DEVICE_DIR/aroma_fg_vulkan_plan_tests' '$DEVICE_DIR/aroma_fg_device_smoke' && chmod 600 '$DEVICE_DIR/libaroma_fg.so'"

overall_status=0
run_device_test() {
  local binary="$1"
  local status
  echo >> "$REPORT"
  echo "===== $binary =====" | tee -a "$REPORT"
  set +e
  adb_target shell "cd '$DEVICE_DIR' && LD_LIBRARY_PATH='$DEVICE_DIR' ./'$binary'" 2>&1 | tee -a "$REPORT"
  status=${PIPESTATUS[0]}
  set -e
  echo "$binary exit status: $status" | tee -a "$REPORT"
  if [[ "$status" -ne 0 ]]; then
    overall_status=1
  fi
}

for binary in "${BINARIES[@]}"; do
  run_device_test "$binary"
done

echo >> "$REPORT"
echo "Overall device test status: $overall_status" | tee -a "$REPORT"
echo "Report saved to: $REPORT"
exit "$overall_status"
