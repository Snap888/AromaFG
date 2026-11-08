# Adreno + Mesa Turnip device test plan

## Status
No physical-device test has been performed for this milestone. The current Vulkan code provides host-handle validation, cache/descriptor helpers, barriers and a CPU dispatch planner; it does not yet record the complete Frame Generation graph. Run this plan only after the full dispatch backend is implemented.

## Required test report
Record at minimum:
- Device model, SoC and Adreno GPU model.
- Android version/build fingerprint and kernel version.
- Mesa Turnip build/version/commit, or proprietary Qualcomm driver version.
- Vulkan API version, driver properties, vendor/device IDs and pipelineCacheUUID.
- Queue family flags and image format feature bits.
- Screen resolution, source resolution, refresh rate, game/scene and thermal state.

## Capability gate
Before creating compute pipelines, query and record:
- Compute queue availability for the host-provided queue family.
- Storage-image support for R16_SFLOAT, R16G16_SFLOAT and R16G16B16A16_SFLOAT.
- Required image tiling/usage support for every private intermediate image.
- Format compatibility of the host source and output images.
- maxComputeWorkGroupInvocations, maxComputeWorkGroupSize and maxComputeWorkGroupCount.
- Timestamp query support and valid timestampPeriod.

Do not assume that a format or extension is available because another Vulkan driver on the same SoC supports it.

## Validation sequence
1. Build arm64-v8a with the Android NDK and Vulkan headers.
2. Run C ABI and flow-plan tests on the target.
3. Enable Khronos Validation Layers, including synchronization validation where available.
4. Run one pass at a time with a minimal test pattern; check image bounds and barriers.
5. Verify source/output layout transitions and confirm the host remains the only queue submitter.
6. Run the complete flow graph after it exists: luma, pyramid, coarse-to-fine search, flow upsample, bidirectional refine and each synthesis phase.
7. Compare generated frames with captured high-frame-rate ground truth; inspect occlusion boundaries, HUD elements, fast motion and scene cuts.
8. Run sustained 30-minute sessions at multiple resolutions and presets. Record GPU time, CPU overhead, memory use, temperature, clocks and device-loss/hang events.
9. Repeat the exact test on a proprietary Qualcomm driver and on at least two Turnip revisions.

## Pass criteria
- Zero validation errors and zero synchronization hazards.
- No host handle is destroyed, retained beyond its documented lifetime or submitted by AromaFG.
- No GPU hang, device loss or unbounded memory growth.
- Generated phase count equals multiplier minus one and every phase is distinct and chronological.
- Frame fallback remains correct on unsupported formats, scene cuts, resize and device loss.
- Performance and image-quality results are reported with raw measurements; do not infer support from CI or cross-compilation.

## Diagnostic comparison
Test a direct host-to-ICD path first. If the host environment uses bionic-vulkan-wrapper, test it as a separate host configuration. AromaFG must not detect, load, configure or depend on that wrapper. Record wrapper/driver versions in the test report so compatibility issues can be attributed correctly.


## Executable smoke test

Build the aroma_fg_device_smoke target with an Android NDK toolchain and AROMA_FG_BUILD_DEVICE_TESTS=ON. The executable creates a Vulkan compute device, allocates two source images and one storage output, runs the public AromaFG Engine adapter, submits the host command buffer, waits its own fence, collects timestamp results and verifies the midpoint pixel against a known color blend.

Example arm64 build:

~~~sh
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DAROMA_FG_ENABLE_VULKAN=ON -DAROMA_FG_BUILD_DEVICE_TESTS=ON
cmake --build build-android --target aroma_fg_device_smoke
~~~

For a standalone shell environment, push both libaroma_fg.so and aroma_fg_device_smoke to the device and run with LD_LIBRARY_PATH set to that directory. If the target host selects Turnip, run the test in that same Turnip environment; repeat on the stock Qualcomm proprietary driver. Do not load or replace an ICD from inside AromaFG. If Turnip is available only through a Winlator/host driver selector rather than as the system Vulkan driver, run this smoke workload through that host's Vulkan context; launching the standalone binary outside that environment would test the system driver instead.

~~~sh
adb push build-android/libaroma_fg.so /data/local/tmp/
adb push build-android/aroma_fg_device_smoke /data/local/tmp/
adb shell 'chmod 755 /data/local/tmp/aroma_fg_device_smoke && LD_LIBRARY_PATH=/data/local/tmp /data/local/tmp/aroma_fg_device_smoke'
~~~

Record the printed device name/vendor/device/driver version and preserve the complete log. A passing smoke test proves the minimal compute graph and output readback on that driver; it does not replace sustained game/compositor tests or synchronization validation with the Khronos Validation Layer. The device executable is not run by hosted CI.
