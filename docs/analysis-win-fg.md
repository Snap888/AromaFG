# Win-FG analysis for AromaFG

Repository reviewed: https://github.com/Snap888/win-fg
Branch: master
Reviewed components: README, CMake, FrameGen header/implementation, record_impl.inc, Vulkan dispatch table, luma/downsample/flow/expand/synthesis shaders, device bring-up notes, and shader provenance notices.

## Findings from the source

Win-FG has a real Vulkan compute engine, not merely a collection of shaders:

- `FrameGen` owns per-resolution private images, image memory, views, samplers, descriptor pool, shader modules, descriptor-set layouts, pipeline layouts and compute pipelines.
- Its image helper performs `vkCreateImage` → `vkGetImageMemoryRequirements` → `vkAllocateMemory` → `vkBindImageMemory` → `vkCreateImageView`. The memory-type helper prefers device-local memory, although its fallback-to-index-zero behaviour is unsafe when no compatible type exists.
- `makePipe` creates shader modules, descriptor layouts, pipeline layouts and compute pipelines. The reviewed implementation passes `VK_NULL_HANDLE` as the pipeline cache, so pipeline-cache persistence is an AromaFG improvement rather than a direct port.
- `record_impl.inc` lazily creates per-size UBOs and descriptor sets, writes descriptors for internal and host image views, and records the dispatch graph.
- The graph computes luma pyramids, solves coarse-to-fine flow, expands the flow, and synthesizes the requested temporal phase. The same flow should be reused for every generated phase; Win-FG's README itself identifies recomputing flow per generated frame as a remaining performance gap.
- Its image barrier helper uses compute-write → compute-read/write dependencies for internal images. External host-image layout transitions and presentation integration remain host-specific responsibilities.
- A single static `ScratchState` is described in the source as a first-draft, one-swapchain-in-flight design. Win-FG also uses ring slots for global-motion readback. AromaFG uses per-in-flight-slot descriptor pools/query pools and a shared internal graph serialized on the borrowed queue.
- The source's destruction/configuration paths use `vkDeviceWaitIdle`. AromaFG must not stall or idle a host-owned device from a frame callback; teardown and resize require the host to guarantee quiescence.
- The Win-FG source does not contain the GPU timestamp-query instrumentation requested here. AromaFG adds per-slot timestamp queries with host-fence-based collection.

## Device and integration lessons

Win-FG documents hardware validation on an AYANEO Pocket FIT (Snapdragon 8 Gen 3 / Adreno 750 / Turnip) and a Galaxy Fold 8 (Adreno 840 / Turnip). Its reported native-compositor measurements include 45→90 FPS at a 45 FPS cap and 58→115 FPS uncapped. The README explains that the original in-guest Vulkan-layer path could report inserted frames while the downstream compositor discarded the extra buffers. The shipping native path therefore runs inside the host compositor.

This is a critical integration lesson: successful compute dispatch or an in-game FPS counter does not prove that generated frames reach scanout. AromaFG's host contract leaves scheduling and presentation to the host; a native compositor integration must consume the output slots rather than silently dropping them.

Win-FG also documents an Adreno 840 freeze caused by incorrectly forwarding present IDs on generated frames. AromaFG does not hook `vkQueuePresentKHR`, alter present chains, or manufacture swapchain presents, so that class of layer-specific responsibility remains with the host.

## Quality stack observed

Win-FG documents bidirectional flow, confidence/occlusion gating, anti-ghost filtering, Lucas–Kanade global-motion pre-warp (C1), and TV-L1 flow regularization (C2). These are later quality milestones for AromaFG. The current AromaFG runtime milestone implements the baseline luma → pyramid → bidirectional search → forward/backward consistency → phase-specific warp/blend path; it does not yet claim parity with Win-FG's C1/C2/anti-ghost stack.

## What AromaFG adopts conceptually

1. Private per-resolution image resources with explicit memory requirements and image views.
2. One compute pipeline per shader and explicit descriptor layouts matching shader bindings.
3. Per-frame descriptor writes and coarse-to-fine bidirectional flow dispatch.
4. Compute-to-compute image memory barriers after every producer pass.
5. Flow computation once per real-frame pair, followed by one synthesis dispatch per requested phase.
6. Device-specific format/workgroup capability checks and on-device logs.
7. Separate host integration and physical scanout validation.

## Deliberate differences

- No copied Win-FG source, shader, embedded SPIR-V or Winlator-specific layer/present code is required for this milestone.
- AromaFG accepts borrowed `VkInstance`, `VkPhysicalDevice`, `VkDevice`, `VkQueue`, command buffer and host image views.
- AromaFG never loads an ICD, intercepts Vulkan entry points, submits the host queue, waits on host fences, or presents images.
- Descriptor pools and timestamp pools are per in-flight slot and may only be reset after the host has waited for that slot's fence.
- Internal working images are shared between sequential submissions on the same borrowed queue; the host must submit callbacks in recording order. Resize/reallocation requires the host-quiescent flag after all prior submissions complete.
- Pipeline cache import/export is exposed, but the host must key stored cache blobs by vendor/device/driver identity, pipelineCacheUUID and AromaFG shader ABI/hash. Never share a Turnip cache blob blindly with the proprietary Qualcomm driver.
- The baseline runtime currently accepts RGBA8_UNORM and RGBA16_SFLOAT storage output slots. Unsupported formats return `AROMA_FG_ERROR_UNSUPPORTED`; no implicit format conversion is attempted.

## Licensing and provenance

The reviewed Win-FG repository declares MIT. Its `NOTICE_FIDELITYFX_OPTICALFLOW.md` identifies AMD FidelityFX SDK optical-flow shader adaptations and includes the MIT notice. AromaFG does not copy those files. Its own provenance and license notices remain authoritative for AromaFG contributions.


## Reviewed source map

- [README and measured device results](https://github.com/Snap888/win-fg/blob/master/README.md)
- [FrameGen resource, pipeline and teardown implementation](https://github.com/Snap888/win-fg/blob/master/src/framegen.cpp)
- [Per-frame descriptor updates and dispatch graph](https://github.com/Snap888/win-fg/blob/master/src/record_impl.inc)
- [Layer Vulkan dispatch table](https://github.com/Snap888/win-fg/blob/master/src/vk_dispatch.hpp)
- [Device bring-up and compositor failure analysis](https://github.com/Snap888/win-fg/blob/master/docs/BRINGUP.md)
- [FidelityFX optical-flow provenance notice](https://github.com/Snap888/win-fg/blob/master/NOTICE_FIDELITYFX_OPTICALFLOW.md)
