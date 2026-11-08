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
~~~

The CI artifact contains `aromafg-build-commit.txt` beside the binaries. The runner reads that file and records it as the binary provenance. If running a locally built binary, set `AROMAFG_BUILD_COMMIT` to its exact 40-character source commit. If neither is supplied, the report explicitly records unknown; it never substitutes the current checkout SHA.

The script captures product manufacturer/model/device, SoC manufacturer/model, board platform, Android release/API, build fingerprint/security patch and kernel version. It then pushes `libaroma_fg.so` and `aroma_fg_device_smoke` to `/data/local/tmp/aromafg` and executes the standalone Vulkan smoke test.

The smoke executable creates its own Vulkan instance/device/queue. It is not an adapter for an existing game's or Winlator's VkDevice and does not validate compositor/present integration. The test may run without Khronos validation layers if the Android image does not expose them; the report must preserve the emitted validation status.

## CI evidence

- Linux lavapipe workflow passed in run #42524916969; this remains software Vulkan evidence.
- Android arm64-v8a cross-build passed in run #42524916969, including the shared library, contract/planner test binaries and standalone Vulkan smoke executable.
- The Android artifact bundle includes the exact source commit metadata. The cross-build cannot execute binaries on a physical device; no Android hardware result is claimed.

## Outstanding physical validation

- Run the ADB smoke on a named physical Adreno device with exact driver details.
- Repeat with pinned Mesa Turnip revisions and Qualcomm proprietary driver where available.
- Collect missing Vulkan driver properties, format feature bits, limits, timestamp support and thermal/performance data.
- Run 30-minute stability and resize/device-loss tests.
- Add a separate borrowed-device host integration test; standalone smoke is not sufficient for Winlator/game compatibility.

## Exit status

Harness and CI are implemented. Phase 3.3 is not complete until Android cross-build passes and a physical-device report is reviewed. Adreno/Turnip and Qualcomm support remain unverified.
