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
