#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail

REPO="Snap888/AromaFG"
RUN_ID="${AROMAFG_RUN_ID:-42527383441}"
ARTIFACT_ID="${AROMAFG_ARTIFACT_ID:-13635768192}"
EXPECTED_SHA256="${AROMAFG_ARTIFACT_SHA256:-32e3d1a2aed099687e1e09a7c047a089ef005a69ef49d762802d7a5e4118db42}"
EXPECTED_COMMIT="${AROMAFG_BUILD_COMMIT:-5d1daabee160fffda4a8ffee1c78140003454899}"
RUNNER_COMMIT="${AROMAFG_RUNNER_COMMIT:-d18dd45ca13ffbb2998bacc3b8f60f8e776fa6cd}"
WORK_DIR="${HOME}/aromafg-validation"
BUILD_DIR="${WORK_DIR}/build-android"

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
command -v pkg >/dev/null 2>&1 || fail "Run this script inside Termux."
pkg update -y
pkg install -y git gh android-tools curl coreutils grep unzip jq
termux-setup-storage || true
mkdir -p "$WORK_DIR" "$BUILD_DIR"

if ! gh auth status >/dev/null 2>&1; then
  printf 'GitHub CLI authentication is required to download the pinned Actions artifact.\n'
  gh auth login
fi

printf '\nWireless ADB setup (skip if a target is already connected).\n'
adb devices
if ! adb devices | awk 'NR > 1 && $2 == "device" { found=1 } END { exit !found }'; then
  read -r -p "Android device IP address: " ADB_IP
  read -r -p "Wireless debugging PAIR port: " PAIR_PORT
  read -r -p "Pairing code: " PAIR_CODE
  adb pair "${ADB_IP}:${PAIR_PORT}" "$PAIR_CODE" || fail "ADB pairing failed."
  read -r -p "Wireless debugging CONNECT port: " CONNECT_PORT
  adb connect "${ADB_IP}:${CONNECT_PORT}" || fail "ADB connect failed."
fi

mapfile -t SERIALS < <(adb devices | awk 'NR > 1 && $2 == "device" {print $1}')
if [[ ${#SERIALS[@]} -ne 1 ]]; then
  adb devices -l >&2
  fail "Exactly one authorized ADB target is required; found ${#SERIALS[@]}."
fi
SERIAL="${SERIALS[0]}"
adb_target() { adb -s "$SERIAL" "$@"; }

ABI="$(adb_target shell getprop ro.product.cpu.abi | tr -d '\r')"
ABILIST="$(adb_target shell getprop ro.product.cpu.abilist | tr -d '\r')"
API="$(adb_target shell getprop ro.build.version.sdk | tr -d '\r')"
[[ "$ABI" == "arm64-v8a" || ",$ABILIST," == *",arm64-v8a,"* ]] || fail "Target is not arm64-v8a."
[[ "$API" =~ ^[0-9]+$ ]] && (( API >= 26 )) || fail "Android API must be >= 26; got '$API'."

ARTIFACT_META="$(gh api "/repos/$REPO/actions/artifacts/$ARTIFACT_ID")"
META_RUN_ID="$(printf '%s' "$ARTIFACT_META" | jq -r '.workflow_run.id // empty')"
META_COMMIT="$(printf '%s' "$ARTIFACT_META" | jq -r '.workflow_run.head_sha // empty')"
META_DIGEST="$(printf '%s' "$ARTIFACT_META" | jq -r '.digest // empty')"
META_EXPIRED="$(printf '%s' "$ARTIFACT_META" | jq -r '.expired // true')"
[[ "$META_RUN_ID" == "$RUN_ID" ]] || fail "Artifact belongs to workflow run '$META_RUN_ID', not '$RUN_ID'."
[[ "$META_COMMIT" == "$EXPECTED_COMMIT" ]] || fail "Artifact source commit mismatch: $META_COMMIT."
[[ "$META_DIGEST" == "sha256:$EXPECTED_SHA256" ]] || fail "GitHub artifact digest metadata mismatch."
[[ "$META_EXPIRED" == "false" ]] || fail "GitHub artifact is expired."

printf '\nDownloading artifact %s from workflow run %s...\n' "$ARTIFACT_ID" "$RUN_ID"
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
[[ "$ACTUAL_SHA256" == "$EXPECTED_SHA256" ]] || fail "Artifact SHA-256 mismatch: expected $EXPECTED_SHA256, got $ACTUAL_SHA256."
unzip -tq "$ZIP" >/dev/null || fail "Artifact ZIP integrity test failed."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
unzip -q "$ZIP" -d "$BUILD_DIR"

PROVENANCE="$BUILD_DIR/aromafg-build-commit.txt"
[[ -s "$PROVENANCE" ]] || fail "Artifact is missing build provenance."
ACTUAL_COMMIT="$(tr -d '\r\n' < "$PROVENANCE")"
[[ "$ACTUAL_COMMIT" == "$EXPECTED_COMMIT" ]] || fail "Binary commit mismatch: expected $EXPECTED_COMMIT, got $ACTUAL_COMMIT."

RUNNER="$WORK_DIR/run_android_device_smoke.sh"
curl --fail --location --silent --show-error \
  "https://raw.githubusercontent.com/$REPO/$RUNNER_COMMIT/tools/run_android_device_smoke.sh" \
  -o "$RUNNER"
chmod 700 "$RUNNER"
bash -n "$RUNNER" || fail "Downloaded ADB runner failed bash syntax validation."

REPORT="$HOME/storage/shared/Download/aromafg-device-report.txt"
if [[ ! -d "$HOME/storage/shared/Download" || ! -w "$HOME/storage/shared/Download" ]]; then
  REPORT="$WORK_DIR/aromafg-device-report.txt"
  printf 'Shared Download folder unavailable; report will be saved to %s\n' "$REPORT"
fi

printf '\nTarget: %s | ABI: %s | API: %s | build: %s\n' "$SERIAL" "$ABI" "$API" "$ACTUAL_COMMIT"
bash "$RUNNER" "$BUILD_DIR" "$REPORT" "$SERIAL"
