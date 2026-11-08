# Vulkan runtime backend (feature/vulkan-engine-runtime)

## Implemented in this milestone

The runtime adapter is a real Vulkan compute implementation, not just a pass planner.

- Owns private optimal-tiled VkImages, VkDeviceMemory and VkImageViews for both luma pyramids, forward/backward flow, confidence, seed flow, zero seed and refined full-resolution flow.
- Checks device queue capabilities, workgroup limits and required R16/RG16 storage formats before pipeline creation.
- Builds shader modules, descriptor-set layouts, pipeline layouts and compute pipelines from SPIR-V embedded at build time.
- Uses an owned VkPipelineCache. Cache blobs can be supplied at backend creation and exported later; if a supplied blob is rejected by the driver, initialization retries with an empty cache. The host is responsible for associating a blob with the exact device/driver and shader ABI.
- Allocates a descriptor pool and timestamp query pool per in-flight slot. The host must wait for that slot's submission fence before AromaFG reuses it.
- Writes descriptors per invocation and records luma, downsample, coarse-to-fine upsample/search in both directions, forward/backward consistency refinement and one synthesis dispatch per requested phase.
- Inserts explicit image barriers between producer and consumer passes. External input/output transitions include host-provided old access/stage and restore access/stage/layout.
- Records three timestamp ranges: preprocess, flow/refine and synthesis. Results are read only after the host confirms the slot fence has completed. The timing fields returned in AromaFGFrameOutput are the most recently completed measurements for that reused slot (therefore delayed); aroma_fg_vulkan_backend_collect_timing() exposes the completed frame ID explicitly.
- Exposes a C adapter through aroma_fg_vulkan_backend_create(); pass the returned AromaFGBackendV1 to aroma_fg_create(). The generic Engine calls the Vulkan adapter through its existing process_frame callback.

## Public API setup

1. Create or obtain the host Vulkan instance, physical device, device and compute-capable queue. AromaFG does not create or own them.
2. Fill AromaFGVulkanBackendCreateInfoV1 with the context, AROMA_FG_VULKAN_INTEROP_VERSION, and 2–4 in-flight slots. Optionally provide a previously exported pipeline-cache blob.
3. Call aroma_fg_vulkan_backend_create(), then pass the returned AromaFGBackendV1 to aroma_fg_create().
4. For each source pair, fill AromaFGVulkanFrameV2 and pass its address through AromaFGFrameInput.host_frame. The frame contains previous/current sampled image views and exactly multiplier-1 output storage images/views.
5. Call aroma_fg_process() while recording the host command buffer, outside a render pass. AromaFG records commands only; the host ends/submits the command buffer and owns all semaphores/fences/presentation.
6. Before reusing a frame_slot, wait for the fence associated with its previous submission. Descriptor pools and timestamp queries are per slot.
7. Call aroma_fg_vulkan_backend_collect_timing() only after the host has waited for that slot's fence. Pipeline cache export uses a size-query call followed by a buffer call.
8. Before a resize reallocates private images, wait for all submissions that used the old extent and set AROMA_FG_VULKAN_FRAME_HOST_QUIESCENT on that resize frame.

## Image and synchronization contract

- Source images must support sampled-image access in the selected format. AromaFG transitions each source from its declared layout to SHADER_READ_ONLY_OPTIMAL, samples it for luma and synthesis, then restores the caller-specified layout/stage/access.
- Output images must have storage usage and an image view whose format matches output_format. Currently supported output formats are VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_R16G16B16A16_SFLOAT. Unsupported formats return AROMA_FG_ERROR_UNSUPPORTED; no implicit format conversion is performed.
- Output images are transitioned from output_layouts[i] to GENERAL, written by compute, then restored to output_restore_layouts[i] using the provided output restore stage/access.
- Required private formats are R16_SFLOAT and R16G16_SFLOAT with optimal-tiling storage support. The device must expose a compute-capable queue and 8×8 workgroups.
- The command buffer must be recording and outside a render pass/dynamic rendering scope. The host must submit callbacks to the same borrowed queue in callback recording order. The internal working pyramid is shared across slots and serialized by queue-order barriers.
- No queue submission, queue/device idle, fence wait, swapchain acquire/present, queue-family ownership transfer, driver loading or host-handle destruction occurs in AromaFG.
- The host must keep all borrowed handles alive until their recorded commands complete. Before aroma_fg_destroy(), the host must wait for all AromaFG submissions to complete; shutdown does not call vkDeviceWaitIdle.

## Flow and synthesis

For each pair of real frames, the runtime computes the luma pyramid once, then performs coarse-to-fine flow search in both directions. Coarse flow is bilinearly upsampled to seed the next finer level. Forward/backward consistency creates confidence masks. The final synthesis pass is dispatched once per phase k / multiplier; flow is not recomputed for every generated frame.

This is the baseline quality path. It does not yet include the Win-FG C1 Lucas–Kanade global-motion pre-warp, C2 TV-L1 regularization, HUD exclusion or its full anti-ghost stack.

## Build and CI

Vulkan runtime builds require Vulkan headers/loader, glslangValidator and Python 3. CMake compiles GLSL for Vulkan 1.1 and embeds SPIR-V into the library; there is no runtime shader-file lookup.

~~~sh
cmake -S . -B build -DAROMA_FG_ENABLE_VULKAN=ON -DAROMA_FG_COMPILE_SHADERS=ON
cmake --build build
ctest --test-dir build --output-on-failure
~~~

To compile the physical-device smoke executable without running it in CI:

~~~sh
cmake -S . -B build -DAROMA_FG_BUILD_DEVICE_TESTS=ON
cmake --build build --target aroma_fg_device_smoke
~~~

## Known limitations

- Only RGBA8_UNORM and RGBA16_SFLOAT storage outputs are supported. Many swapchain formats (especially BGRA8) require the host to provide a compatible intermediate output image or a future conversion pass.
- Internal images are separately allocated; suballocation/budget-aware pooling is not implemented yet.
- The descriptor/query slot contract assumes the host has waited its own slot fence. AromaFG cannot inspect or wait for that fence.
- Shared internal resources assume same-queue, callback-order submission. Concurrent multi-queue use is unsupported.
- The runtime has not yet been run on a physical Adreno/Turnip or proprietary Qualcomm driver in this development session.
