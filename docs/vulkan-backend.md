# Vulkan backend — interop and orchestration milestone

## Scope
This milestone adds a host-borrowed Vulkan ABI contract and reusable primitives: borrowed context validation, owned pipeline cache, owned descriptor pool, explicit image barriers, CPU-side pyramid/schedule planning, and a bilinear flow-upsample shader. It does not yet allocate private VkImages, create shader modules/pipelines, or record the complete GPU graph.

## Ownership
AromaFGVulkanContextV1 and AromaFGVulkanFrameV1 are borrowed structures. The host owns all handles and keeps them valid during the process callback. AromaFG owns only helper-created pipeline cache and descriptor pool objects. No helper submits queues, waits on fences, acquires/presents swapchain images, or destroys host handles. The frame payload may be passed through AromaFGFrameInput.host_frame; validate struct_size and api_version before casting. The public aroma_fg_vulkan_validate_context() and aroma_fg_vulkan_validate_frame() entrypoints perform the initial structural/capability checks. These functions do not create pipelines or execute dispatches.

## Synchronization
GENERAL is a layout, not a memory dependency. Compute storage-image writes followed by compute reads require a compute-to-compute barrier with SHADER_WRITE to SHADER_READ access. Internal images use QUEUE_FAMILY_IGNORED; this ABI does not transfer queue-family ownership. Host adapter must define source/output transitions and restore agreed layouts.

## Cache and descriptor lifetime
Pipeline cache data is device/driver specific. Persist it with vendor ID, device ID, driver version, pipelineCacheUUID, AromaFG shader ABI/version and shader-set hash; reject mismatched blobs. Never assume cache portability between Turnip and proprietary Adreno. Reset descriptor pools only after the host confirms all command buffers using their sets have completed.

## Flow graph
make_pyramid rounds odd extents upward and stops before either dimension falls below min_dimension. make_flow_schedule emits luma, downsample, coarse-to-fine upsample/search in both directions, bidirectional refine, then k/multiplier synthesis phases. Flow is computed once per source pair and reused by every phase. The schedule is a CPU plan, not evidence of GPU dispatch.

## Adreno / Turnip
Start with 8x8 workgroups and reduced flow scale. Keep flow/confidence images compact. Measure every pass with GPU timestamp queries. Record GPU model, driver/Turnip build, formats, workgroup size, thermal state and per-pass time. No device support claim is valid until validation-layer checks and sustained physical-device tests pass.
