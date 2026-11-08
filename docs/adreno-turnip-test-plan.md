# Adreno + Mesa Turnip device test plan

## Status and evidence boundary

The experimental Vulkan runtime branch has a CI smoke test that executes the compute graph on Mesa lavapipe with Khronos synchronization validation and verifies output readback. This is software Vulkan evidence only. It does **not** prove Adreno, Turnip, Qualcomm proprietary-driver, Android compositor or game-host compatibility.

Phase 3.3 adds a pinned Android arm64-v8a cross-build and an ADB runner for the standalone Vulkan smoke executable. A successful cross-build proves only that the Android target compiles. A successful ADB smoke run proves only that the standalone process executed the compute graph on the selected system Vulkan implementation; it does not prove borrowed-device integration with a game or Winlator.

No physical Adreno/Turnip or Qualcomm proprietary-driver test is claimed until a device report has been collected and reviewed. Do not publish hardware-support, latency, quality or performance claims before the exact device/driver matrix has been exercised.

## Required test report

Record at minimum:

- Device manufacturer/model, product codename, SoC manufacturer/model and board platform.
- Android version/API level/build fingerprint, security patch and kernel version.
- Adreno GPU model and driver identity/version (Turnip build/commit or proprietary Qualcomm driver).
- Vulkan API version, driver version, vendor/device IDs and pipelineCacheUUID.
- Queue family flags and image format feature bits.
- Screen resolution, source resolution, refresh rate, game/scene and thermal state.
- AromaFG commit SHA, build flags, shader hashes and test configuration.
- Whether Khronos validation and synchronization validation were actually enabled.

The runner captures Android properties and kernel metadata. The smoke executable prints Vulkan device name, vendor/device IDs, driver version and validation-layer status. Complete any missing driver/format/thermal fields manually from the exact test environment.

## Capability gate

Before creating compute pipelines, query and record:

- Compute queue availability for the host-provided queue family.
- Storage-image support for R16_SFLOAT, R16G16_SFLOAT and R16G16B16A16_SFLOAT.
- Confirm shaderStorageImageExtendedFormats was enabled at vkCreateDevice; record enabled features, not only supported features.
- Required image tiling/usage support for every private intermediate image.
- Format compatibility of host source and output images.
- maxComputeWorkGroupInvocations, maxComputeWorkGroupSize and maxComputeWorkGroupCount.
- Timestamp query support and valid timestampPeriod.
- Linear-filter support for formats used with linear samplers.

Never infer support from a different driver revision or another Vulkan implementation on the same SoC.

## Validation sequence

1. Build arm64-v8a with the pinned Android NDK revision from docs/build.md.
2. Run tools/run_android_device_smoke.sh build-android on a physical target; preserve the generated report.
3. Run C ABI and flow-plan tests on the target (a later device-test runner must execute the cross-compiled test binaries).
4. Enable Khronos Validation Layers, including synchronization validation where available; record when unavailable.
5. Verify the standalone graph's output readback and all emitted validation errors.
6. Verify source/output layout transitions and confirm the host remains the only queue submitter in a future in-host integration.
7. Exercise successful submission, command-buffer reset/abandonment and slot reuse; resize quiescence remains a separate test.
8. Compare generated frames with captured high-frame-rate ground truth; inspect occlusion boundaries, HUD elements, fast motion and scene cuts.
9. Run sustained 30-minute sessions at multiple resolutions and presets. Record GPU time, CPU overhead, memory use, temperature, clocks and device-loss/hang events.
10. Repeat on a proprietary Qualcomm driver and at least two pinned Turnip revisions when available.

## Pass criteria

- Zero validation errors and synchronization hazards when validation is available.
- No host handle is destroyed, retained beyond its documented GPU-use lifetime or submitted by AromaFG.
- No GPU hang, device loss or unbounded memory growth.
- Generated phase count equals multiplier minus one and every phase is distinct and chronological.
- Correct real-frame fallback on unsupported formats, scene cuts, resize, cancellation and device loss.
- Performance and image-quality results include raw measurements and exact environment metadata.

## Diagnostic comparison

Test direct host-to-ICD first. If the environment uses bionic-vulkan-wrapper, test it as a separate host configuration. AromaFG must not detect, load, configure or depend on that wrapper. Record wrapper/driver versions to attribute compatibility issues.

## Standalone smoke executable

The aroma_fg_device_smoke target creates its own Vulkan device and queue, records and submits its own command buffer, waits on its own fence and reads back a test output. It tests the selected system Vulkan driver for that process; it does not attach to an existing Winlator VkDevice and does not prove an in-host compositor integration.

For Winlator/Turnip validation, a host-side adapter must pass Winlator's borrowed context, command buffer and frame resources into AromaFG. Do not load or replace an ICD from inside AromaFG.
