# Phase 3.3 — Android arm64 build and physical-device validation harness

## Scope

- Pin an Android NDK revision and API floor for reproducible arm64-v8a cross-compilation.
- Compile the shared library, contract/planner test binaries and standalone Vulkan device smoke executable for Android.
- Add an ADB runner that records Android product/SoC/build/kernel properties, deploys the shared library and smoke executable, and captures the Vulkan smoke output.
- Preserve the exact source commit alongside CI-built Android binaries to avoid reporting the SHA of an unrelated checkout.
- Keep physical driver validation separate from cross-compilation and Linux software CI evidence.

## Build configuration

- Android NDK: r27d, revision 27.2.12479018.
- ABI: arm64-v8a.
- Android API: 26.
- Vulkan loader: provided by Android system; AromaFG does not bundle or load an ICD.
- GLSL/SPIR-V tools: host-side glslangValidator and spirv-val during cross-build.

## Device runner

Run with an authorized USB-debugging device connected:

~~~sh
tools/run_android_device_smoke.sh build-android
# Optional: select an exact ADB serial (also accepted via ADB_SERIAL)
tools/run_android_device_smoke.sh build-android aromafg-device-report.txt SERIAL
~~~

The runner requires exactly one authorized ADB device unless an explicit serial is supplied (argument 3 or `ADB_SERIAL`). It rejects targets without arm64-v8a support and Android API 26 or newer before deployment. All device-side commands are scoped to the selected serial.

The CI artifact contains `aromafg-build-commit.txt` beside the binaries. The runner reads that file and records it as the binary provenance. If running a locally built binary, set `AROMAFG_BUILD_COMMIT` to its exact 40-character source commit. If neither is supplied, the report explicitly records unknown; it never substitutes the current checkout SHA.

The script captures product manufacturer/model, SoC manufacturer/model, board platform, Android release/API, build fingerprint/security patch and kernel version. It then pushes `libaroma_fg.so` and `aroma_fg_device_smoke` to `/data/local/tmp/aromafg` and executes the standalone Vulkan smoke test.

The smoke executable creates its own Vulkan instance/device/queue. It is not an adapter for an existing game's or Winlator's VkDevice and does not validate compositor/present integration. The test may run without Khronos validation layers if the Android image does not expose them; the report must preserve the emitted validation status.

## CI evidence

- Run [#42565368872](https://github.com/Snap888/AromaFG/actions/runs/42565368872) completed successfully: Linux lavapipe graph with synchronization validation and Android arm64-v8a NDK cross-build.
- Newer run [#42565464843](https://github.com/Snap888/AromaFG/actions/runs/42565464843) also completed successfully. Both `linux` and `android-arm64` jobs passed, including helper script validation, Linux build/contract/full Vulkan graph, Android NDK configure, arm64 cross-compilation, provenance and artifact upload.
- These are CI results. Linux Vulkan execution used Mesa lavapipe; Android CI cross-compiles but does not execute on the physical phone.

## Termux launcher

The optional `tools/run-aromafg-termux.sh` helper runs from Termux on the Android target. It installs Termux packages, authenticates GitHub CLI, pairs/connects wireless ADB when needed, requires exactly one authorized arm64-v8a target with API 26+, downloads a pinned GitHub Actions artifact, verifies the ZIP SHA-256 and embedded source commit, then downloads and syntax-checks the matching ADB runner.

~~~sh
bash tools/run-aromafg-termux.sh
~~~

The default run/artifact/hash/commit values are reproducibility pins for the recorded CI artifact. When a newer CI artifact is published, override `AROMAFG_RUN_ID`, `AROMAFG_ARTIFACT_ID`, `AROMAFG_ARTIFACT_SHA256`, `AROMAFG_BUILD_COMMIT` and `AROMAFG_RUNNER_COMMIT` together. Do not update only one pin. The helper has not been executed on a physical device in CI; review its output and attach the generated report. The current pinned artifact was produced by CI run #42565368872; Linux and Android arm64 jobs both completed successfully, including script syntax/ShellCheck, Linux lavapipe graph, and Android cross-compilation.

## Physical device evidence (2026-11-09)

A direct local Termux run (no ADB) completed successfully on the ASUS AI2205 device. The tested artifact was built from commit `fef098fa96a82a4a5c09fd50d901593f2ddf50cc`; ZIP SHA-256: `41c0882db8bc8d7e2772f0f7fc3814143173e317118a306f8a6be648c3f3abf8`. The report identifies Qualcomm SM8550 / Kalama, Adreno 740, Android 15 / API 35, Vulkan 1.3.128, vendor ID `5143`, device ID `43050a01`, and driverVersion `2150252610`. Contract tests, Vulkan planner tests, standalone Vulkan device smoke and fence-backed slot reuse all exited with status 0. See [physical-device-validation-2026-11-09.md](physical-device-validation-2026-11-09.md).

**Evidence limits:** `VALIDATION_LAYER=off` and `SYNC_VALIDATION=off`; the smoke output did not expose `driverName` / `driverInfo`, so this is not evidence of a specific Turnip or Qualcomm proprietary-driver build. The test creates its own Vulkan instance/device/queue; it is not a borrowed Winlator/game device. The tested binary predates the subsequent timing-output fix commits; those commits still require CI and device regression validation.

## Outstanding physical validation

- Repeat the smoke test with validation + synchronization validation enabled on a named physical Adreno device and capture `VK_EXT_driver_properties` where available.
- Repeat with pinned Mesa Turnip revisions and identify/test the Qualcomm proprietary driver separately where available.
- Collect missing Vulkan driver properties, format feature bits, limits, timestamp support and thermal/performance data.
- Run 30-minute stability and resize/device-loss tests.
- Add a separate borrowed-device host integration test; standalone smoke is not sufficient for Winlator/game compatibility.

## Exit status

Harness and CI are implemented. The Android arm64 cross-build and one physical Adreno 740 system-driver smoke run are confirmed. Phase 3.3 remains open for exact driver identification, Turnip/proprietary-driver matrix, validation-layer run and borrowed-device host integration. No broad Android/Adreno/Turnip compatibility claim is made.
