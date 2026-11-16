# Phase 3.4 — borrowed-device host integration validation

## Current branch

- Branch: `feature/phase-3-4-borrowed-host-validation`
- Base: `feature/phase-3-3-android-device-validation`
- Starting source: `68590acf5a0b4f2a1f5ad0beccb5af642840e91c`
- Scope: strengthen the explicit host-ownership regression before attempting a Winlator/DXVK adapter.

## Implemented in this slice

- Extended `tests/device_vulkan_smoke.cpp` to verify that the host `VkQueue` handle remains the same after `aroma_fg_destroy()`.
- The smoke test creates a small host-owned Vulkan buffer using the same `VkDevice` after AromaFG shutdown, then destroys that buffer through the host device.
- This guards against accidental destruction/ownership transfer of the borrowed device and queue in the AromaFG backend destructor.

This is a regression test for borrowed-handle ownership, not yet a real Winlator/game-device integration. The test still creates its own Vulkan instance/device as the host fixture. It does not prove compositor integration, DXVK compatibility, external synchronization correctness under a live renderer, or Turnip behavior.

## Verified facts from the current runtime

- `AromaFGVulkanContextV2` receives host-owned instance, physical device, device and queue.
- The runtime creates only private pipelines, descriptors, samplers, query pools and intermediate images.
- `Runtime::destroy()` destroys AromaFG-owned Vulkan resources and nulls its local copy of the borrowed device handle; it does not call `vkDestroyDevice`, `vkDestroyInstance`, `vkDestroyQueue` (which does not exist), `vkQueueWaitIdle` or `vkDeviceWaitIdle`.
- The host owns queue submission, fences, command-pool reset, swapchain acquire/present and final teardown.
- Shared intermediate images are serialized by same-queue callback-order submission. Multi-queue concurrent use is unsupported.
- The current device smoke covers record, abandon/reset/cancel/retry, submit/commit, fence wait, timing collection and slot reuse.

## Remaining Phase 3.4 gates

1. Add a host sample that accepts externally created Vulkan handles rather than creating its own device internally; keep all device/queue/command-pool creation in the sample host layer.
2. Run the sample against a real renderer-owned device/queue on Android.
3. Add an integration shim for a pinned WinNative/Winlator revision only after the standalone borrowed-host contract passes.
4. Validate command-buffer recording inside the host's legal command-recording scope and verify all source/output layouts and stage/access masks.
5. Run Khronos core and synchronization validation where the target image supports it.
6. Exercise resize, format change, scene reset, abandoned command buffers, device loss and shutdown with outstanding work.
7. Confirm the host waits for the slot fence before descriptor/query reuse and before backend destruction.

## Memory audit and proposed implementation

The current runtime allocates flow, confidence and seed images at every luma-pyramid level, including full-resolution level 0. It then allocates full-resolution refined forward/backward flow and confidence images.

For a 1920x1080 RGBA input, assuming the current R16/RG16 internal formats and an eight-level pyramid, the private image payload is approximately 91.2 MB decimal (about 87 MiB), excluding external images, allocation alignment, descriptors, pipeline objects and driver overhead.

A low-risk first optimization is to stop block-search flow estimation at pyramid level 1 (half resolution), then upsample the bidirectional flow and confidence to full resolution before the existing refinement and synthesis stages. This removes the level-0 forward/backward flow, confidence and seed allocations (about 41.5 MB at 1080p in the current format layout), while preserving full-resolution refined flow and synthesis inputs.

Expected payload reduction is approximately 45% for the current private image set at 1080p. This is an estimate, not a measured device result.

Implementation requirements:

- Keep full-resolution luma input/pyramid behavior unchanged initially.
- Allocate flow/seed/confidence images only for levels 1..N-1.
- Search and coarse-to-fine upsample only within those flow levels.
- Add a dedicated R16 confidence upsample pass, or equivalent format-correct sampling path.
- Upsample vector magnitude into full-resolution pixel units; verify odd extents and non-uniform x/y scale.
- Run the existing forward/backward consistency refinement at full resolution.
- Preserve the current path as a test/reference mode until quality and GPU timing comparisons pass.
- Handle tiny extents with fewer than two usable pyramid levels by bypassing safely.

Do not repurpose `flow_scale_percent`: it scales displacement magnitude and is not a resolution setting. A new public resolution control requires an ABI-compatible config extension and must not silently reinterpret existing V1 fields.

## Required test matrix

### Correctness
- Static image: zero flow and no phase drift.
- Integer translations: horizontal, vertical and diagonal motion.
- Subpixel translation and slow motion.
- Fast motion near and beyond the search radius.
- Odd sizes: 1919x1079, 1279x719 and small dimensions.
- Repeated textures, flat regions and occlusion boundaries.
- Forward/backward consistency and confidence bounds.
- ×2/×3/×4 phase monotonicity and flow reuse.

### Vulkan correctness
- Lavapipe with core + synchronization validation.
- Adreno proprietary driver with driver properties captured.
- Pinned Mesa Turnip revision on the same device where possible.
- Storage format support for R16_SFLOAT and R16G16_SFLOAT.
- Descriptor pool exhaustion and command-buffer abandon/retry.
- Resize only after host-established quiescence.

### Performance
- 720p, 1080p and 1440p.
- Stage GPU timestamps, peak private image payload and allocation count.
- Workgroup 8x8 baseline; test alternatives only with measured occupancy/latency.
- Sustained 30-minute thermal run.
- Compare image quality and flicker against the existing full-resolution reference.

## Acceptance criteria

- No AromaFG-owned device/queue/WSI creation or destruction.
- No queue/device idle or fence wait in the render path.
- No validation/synchronization errors in the supported test matrix.
- No frame-order, descriptor lifetime or resource reuse errors.
- Low-resolution mode reduces private image payload by at least 35% at 1080p.
- No material quality regression on the deterministic translation/occlusion corpus.
- Turnip and proprietary driver results are reported separately.
- No production or real-time performance claim until measured on named hardware.
