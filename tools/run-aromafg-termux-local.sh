#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail

REPO="Snap888/AromaFG"
RUN_ID="${AROMAFG_RUN_ID:-42565464843}"
ARTIFACT_ID="${AROMAFG_ARTIFACT_ID:-13651525532}"
EXPECTED_SHA256="${AROMAFG_ARTIFACT_SHA256:-069b339d2605a91077431db7e38e35c0e7bcf7c67431a759ddd4633110f7167a}"
EXPECTED_COMMIT="${AROMAFG_BUILD_COMMIT:-da1e0ca636f41b4997222a3ae0c3e6b5feb1f120}"
WORK_DIR="${HOME}/aromafg-validation"
BUILD_DIR="${WORK_DIR}/build-android"

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
command -v pkg >/dev/null 2>&1 || fail "Run this script inside Termux."
ARCH="$(uname -m)"
[[ "$ARCH" == "aarch64" ]] || fail "This artifact requires an aarch64 Android device; detected $ARCH."
command -v getprop >/dev/null 2>&1 || fail "Android getprop is unavailable."
API="$(getprop ro.build.version.sdk | tr -d '\r')"
[[ "$API" =~ ^[0-9]+$ ]] && (( API >= 26 )) || fail "Android API must be >= 26; got '$API'."
ABI="$(getprop ro.product.cpu.abi | tr -d '\r')"
[[ "$ABI" == "arm64-v8a" ]] || fail "Primary Android ABI is '$ABI', expected arm64-v8a."

pkg update -y
pkg install -y gh curl coreutils grep unzip jq
termux-setup-storage || true
mkdir -p "$WORK_DIR" "$BUILD_DIR"

if ! gh auth status >/dev/null 2>&1; then
  printf '\nGitHub CLI authentication is required to download the Actions artifact.\n'
  gh auth login
fi

ARTIFACT_META="$(gh api "/repos/$REPO/actions/artifacts/$ARTIFACT_ID")"
META_RUN_ID="$(printf '%s' "$ARTIFACT_META" | jq -r '.workflow_run.id // empty')"
META_COMMIT="$(printf '%s' "$ARTIFACT_META" | jq -r '.workflow_run.head_sha // empty')"
META_DIGEST="$(printf '%s' "$ARTIFACT_META" | jq -r '.digest // empty')"
META_EXPIRED="$(printf '%s' "$ARTIFACT_META" | jq -r '.expired // true')"
[[ "$META_RUN_ID" == "$RUN_ID" ]] || fail "Artifact belongs to run '$META_RUN_ID', not '$RUN_ID'."
[[ "$META_COMMIT" == "$EXPECTED_COMMIT" ]] || fail "Artifact source commit mismatch: $META_COMMIT."
[[ "$META_DIGEST" == "sha256:$EXPECTED_SHA256" ]] || fail "GitHub artifact digest metadata mismatch."
[[ "$META_EXPIRED" == "false" ]] || fail "GitHub artifact is expired."

printf '\nDownloading verified Android arm64 artifact...\n'
ZIP="$WORK_DIR/aromafg-android-arm64.zip"
GH_TOKEN="$(gh auth token)"
[[ -n "$GH_TOKEN" ]] || fail "GitHub CLI did not return an authentication token."
curl --fail --location --silent --show-error \
  -H "Accept: application/vnd.github+json" \
  -H "Authorization: Bearer $GH_TOKEN" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  "https://api.github.com/repos/$REPO/actions/artifacts/$ARTIFACT_ID/zip" \
  -o "$ZIP"
unset GH_TOKEN
ACTUAL_SHA256="$(sha256sum "$ZIP" | awk '{print $1}')"
[[ "$ACTUAL_SHA256" == "$EXPECTED_SHA256" ]] || fail "ZIP SHA-256 mismatch: $ACTUAL_SHA256."
unzip -tq "$ZIP" >/dev/null || fail "Artifact ZIP integrity check failed."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
unzip -q "$ZIP" -d "$BUILD_DIR"

PROVENANCE="$BUILD_DIR/aromafg-build-commit.txt"
[[ -s "$PROVENANCE" ]] || fail "Artifact has no build provenance."
ACTUAL_COMMIT="$(tr -d '\r\n' < "$PROVENANCE")"
[[ "$ACTUAL_COMMIT" == "$EXPECTED_COMMIT" ]] || fail "Embedded build commit mismatch."
for file in libaroma_fg.so aroma_fg_contract_tests aroma_fg_vulkan_plan_tests aroma_fg_device_smoke; do
  [[ -s "$BUILD_DIR/$file" ]] || fail "Artifact is missing $file."
done
chmod 700 "$BUILD_DIR/aroma_fg_contract_tests" "$BUILD_DIR/aroma_fg_vulkan_plan_tests" "$BUILD_DIR/aroma_fg_device_smoke"

REPORT="$WORK_DIR/aromafg-local-device-report.txt"
{
  echo "AromaFG Termux direct-device Vulkan report"
  echo "Execution mode: direct local process; no ADB"
  echo "Android model: $(getprop ro.product.model)"
  echo "Manufacturer: $(getprop ro.product.manufacturer)"
  echo "Device: $(getprop ro.product.device)"
  echo "SoC manufacturer: $(getprop ro.soc.manufacturer)"
  echo "SoC model: $(getprop ro.soc.model)"
  echo "Board platform: $(getprop ro.board.platform)"
  echo "Hardware: $(getprop ro.hardware)"
  echo "Android release: $(getprop ro.build.version.release)"
  echo "Android API: $API"
  echo "Primary ABI: $ABI"
  echo "Build fingerprint: $(getprop ro.build.fingerprint)"
  echo "Security patch: $(getprop ro.build.version.security_patch)"
  echo "Kernel: $(uname -a)"
  echo "AromaFG build commit: $ACTUAL_COMMIT"
  echo "Artifact SHA-256: $ACTUAL_SHA256"
  echo "Capture time: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo
  echo "Vulkan loader candidates:"
  for loader in /system/lib64/libvulkan.so /vendor/lib64/libvulkan.so; do
    if [[ -r "$loader" ]]; then echo "AVAILABLE: $loader"; else echo "NOT_ACCESSIBLE: $loader"; fi
  done
  echo
} > "$REPORT"

# Prefer the Android system Vulkan loader while retaining Termux's C++ runtime.
export LD_LIBRARY_PATH="$BUILD_DIR:/system/lib64:/vendor/lib64:${PREFIX}/lib:${LD_LIBRARY_PATH:-}"

overall=0
run_test() {
  local name="$1"
  shift
  printf '\n===== %s =====\n' "$name" | tee -a "$REPORT"
  set +e
  "$@" 2>&1 | tee -a "$REPORT"
  local status=${PIPESTATUS[0]}
  set -e
  printf '%s exit status: %s\n' "$name" "$status" | tee -a "$REPORT"
  if (( status != 0 )); then overall=1; fi
}

run_test "aroma_fg_contract_tests" "$BUILD_DIR/aroma_fg_contract_tests"
run_test "aroma_fg_vulkan_plan_tests" "$BUILD_DIR/aroma_fg_vulkan_plan_tests"

printf '\n===== Vulkan loader preflight =====\n' | tee -a "$REPORT"
if [[ ! -r /system/lib64/libvulkan.so && ! -r /vendor/lib64/libvulkan.so ]]; then
  echo "No readable system/vendor Vulkan loader path. Direct Termux Vulkan execution may be blocked by Android linker namespace." | tee -a "$REPORT"
fi

run_test "aroma_fg_device_smoke" "$BUILD_DIR/aroma_fg_device_smoke"

{
  echo
  echo "Overall result: $overall"
  echo "Report: $REPORT"
} | tee -a "$REPORT"

DOWNLOAD_DIR="$HOME/storage/shared/Download"
if [[ -d "$DOWNLOAD_DIR" && -w "$DOWNLOAD_DIR" ]]; then
  cp "$REPORT" "$DOWNLOAD_DIR/aromafg-local-device-report.txt"
  printf '\nReport copied to: %s/aromafg-local-device-report.txt\n' "$DOWNLOAD_DIR"
else
  printf '\nReport saved at: %s\n' "$REPORT"
  printf 'If needed, run termux-setup-storage and rerun to copy it to Downloads.\n'
fi
exit "$overall"
