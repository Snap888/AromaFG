# Adreno + Mesa Turnip device test plan

## Status and evidence boundary

The experimental Vulkan runtime branch has a CI smoke test that executes the compute graph on Mesa lavapipe with Khronos synchronization validation and verifies output readback. This is a software Vulkan result. It does **not** prove Adreno, Turnip, Qualcomm proprietary-driver, Android compositor or game-host compatibility.

No physical Adreno/Turnip or Qualcomm proprietary-driver test has been completed for this milestone. Do not publish hardware-support, latency, quality or performance claims until the exact device/driver matrix below has been exercised.

## Required test report

Record at minimum:

- Device model, SoC and Adreno GPU model.
- Android version/build fingerprint and kernel version.
- Mesa Turnip build/version/commit, or proprietary Qualcomm driver version.
- Vulkan API version, driver properties, vendor/device IDs and pipelineCacheUUID.
- Queue family flags and image format feature bits.
- Screen resolution, source resolution, refresh rate, game/scene and thermal state.
- AromaFG commit SHA, build flags, shader hashes and test configuration.

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

1. Build arm64-v8a with the pinned Android NDK and Vulkan headers.
2. Run C ABI and flow-plan tests on the target.
3. Enable Khronos Validation Layers, including synchronization validation where available.
4. Run one pass at a time with minimal patterns; check image bounds and barriers.
5. Verify source/output layout transitions and confirm the host remains the only queue submitter.
6. Exercise successful submission, command-buffer reset/abandonment, slot reuse and resize quiescence.
7. Run the complete graph: luma, pyramid, coarse-to-fine search, flow upsample, bidirectional refinement and each synthesis phase.
8. Compare generated frames with captured high-frame-rate ground truth; inspect occlusion boundaries, HUD elements, fast motion and scene cuts.
9. Run sustained 30-minute sessions at multiple resolutions and presets. Record GPU time, CPU overhead, memory use, temperature, clocks and device-loss/hang events.
10. Repeat on a proprietary Qualcomm driver and at least two pinned Turnip revisions.

## Pass criteria

- Zero validation errors and synchronization hazards.
- No host handle is destroyed, retained beyond its documented GPU-use lifetime or submitted by AromaFG.
- No GPU hang, device loss or unbounded memory growth.
- Generated phase count equals multiplier minus one and every phase is distinct and chronological.
- Correct real-frame fallback on unsupported formats, scene cuts, resize, cancellation and device loss.
- Performance and image-quality results include raw measurements and exact environment metadata.

## Diagnostic comparison

Test direct host-to-ICD first. If the environment uses bionic-vulkan-wrapper, test it as a separate host configuration. AromaFG must not detect, load, configure or depend on that wrapper. Record wrapper/driver versions to attribute compatibility issues.

## Standalone smoke executable

The aroma_fg_device_smoke target is a host-side test executable. It creates its own Vulkan device and queue, records and submits its own command buffer, waits on its own fence and reads back a test output. It tests the selected system Vulkan driver for that process; it does not attach to an existing Winlator VkDevice and does not prove an in-host compositor integration.

For Winlator/Turnip validation, a host-side adapter must pass Winlator's borrowed context, command buffer and frame resources into AromaFG. Do not load or replace an ICD from inside AromaFG.
