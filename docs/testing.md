# Testing and validation plan

## Portable contract tests

- ABI size/version rejection and default config.
- Engine create/destroy and callback lifecycle.
- Enable/disable and hot config serialization.
- Strict monotonic frame ordering; busy/concurrent call behavior.
- Scene cut/history reset and bypass behavior.
- Output count never exceeds multiplier-1.
- Distinct chronological phases are asserted by backend integration tests.
- Invalid timing/metrics never enter adaptive controller as trusted values.
- Telemetry duplicate IDs, stale swapchain generation, bounded storage and percentile semantics.

## Shader validation

Compile GLSL offline to SPIR-V for the declared Vulkan target; run spirv-val. Add shader unit/reference tests for image bounds, flow coordinate conventions, search-boundary confidence, forward/backward consistency and fallback.

## Physical device matrix

Test actual Adreno + Mesa Turnip and proprietary Qualcomm driver separately. Record exact SoC, Android build, driver revision, Vulkan capabilities, format support, resolution and refresh. Do not infer physical support from cross-compilation.

## Host integration

X11, Wayland, Winlator/DXVK and native Vulkan are separate adapters. Validate each against its exact host revision. Test queue ownership, command recording, output layout, acquire timeout, successful present, resize, swapchain/device recreation, scene cuts, enable/disable, device loss, audio and long sessions.

## Image quality

Capture real high-frame-rate triplets (previous, true midpoint, current). Do not use generated outputs as ground truth. Split dataset by game/scene/session. Evaluate PSNR/SSIM with color-space correctness, edge/gradient error, occlusion-boundary error, temporal stability and HUD ghosting.

## Performance and delivery

Measure source FPS, flow solves per pair, synthesis phases, successful generated presents, acquire misses, pacing median/P95/P99, latency, per-stage GPU time, CPU overhead and sustained thermal/power. Run repeated alternating A/B tests against other FG systems under identical device, scene, resolution, refresh and thermal conditions. Publish losses and regressions as well as wins.
