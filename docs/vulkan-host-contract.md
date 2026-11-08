# Vulkan host interop and ownership contract

## Ownership

| Resource | Owner | AromaFG access |
|---|---|---|
| VkInstance / VkPhysicalDevice / VkDevice | Host | Borrowed; query/use during backend lifetime |
| VkQueue / queue family | Host | Queue metadata only; never submit |
| VkCommandBuffer | Host | Record commands only |
| Source image/view | Host | Read according to declared layout/access |
| Generated output images/views | Host | Write within declared capacity |
| Swapchain / acquire/present semaphores | Host | Never owned or destroyed by AromaFG |
| Pipelines, descriptors, samplers, private intermediate images | AromaFG | Owned and destroyed by AromaFG |

## Two distinct lifetimes

1. **CPU callback payload:** pointers to AromaFGVulkanFrameV2, arrays and other host-owned structs are borrowed only during `aroma_fg_process()`. AromaFG must not retain those CPU pointers after return.
2. **Vulkan objects referenced by recorded commands:** source/output images, image views, device and command buffer must remain valid until the host submission containing AromaFG commands has completed. The host must not recycle or destroy them merely because `aroma_fg_process()` returned.

Before destroying AromaFG, the host must ensure all submissions referencing AromaFG resources have completed and no recording transaction is pending. Before resizing/reallocating backend resources, the host must establish the documented quiescent point.

## Frame transaction protocol

Transactional Vulkan backends expose an explicit recording transaction:

1. Call `aroma_fg_process(engine, input, output)`. The backend validates the payload and records commands; a successful return means **recorded**, not submitted.
2. If the host successfully submits that command buffer to the borrowed queue (`vkQueueSubmit == VK_SUCCESS`), call `aroma_fg_commit_frame(engine, frame_id)`.
3. If recording is abandoned, first reset/discard the command buffer according to Vulkan rules, then call `aroma_fg_cancel_frame(engine, frame_id)`.
4. Do not call cancel for a command buffer that has been submitted. Do not call commit if process returned an error.
5. The frame ID is not consumed until commit. A cancelled/failed recording may be retried with the same frame ID.
6. Only one CPU recording transaction may be outstanding per AromaFG engine. Commit releases the CPU transaction; it does not mean GPU completion.
7. Before reusing a Vulkan frame slot, wait for the host submission fence. Collect or otherwise retire the slot's timestamp query results before reuse.

The core rejects another process/config call while a transaction is pending. This prevents frame-order advancement and config mutation from racing a host decision about a recorded command buffer. Legacy backends that do not provide both transaction callbacks retain commit-on-success behavior; they do not get command-buffer cancellation guarantees.

## Callback lifecycle

- initialize: validate device capabilities/formats and allocate private resources.
- update_config: apply settings at a serialized safe point; returns BUSY while a recording transaction is pending.
- reset_history: invalidate temporal state after scene cut, frame gap, resize, format/device/swapchain recreation or enable transition.
- process_frame: record bounded compute work into a host command buffer.
- commit_frame: backend marks successfully submitted recording state; no queue submission occurs here.
- cancel_frame: backend releases recording-only state after the host has abandoned the command buffer.
- shutdown: release private resources before host device teardown, after host-established quiescence.

## Submission and command-buffer failure

AromaFG never submits the host command buffer and cannot know whether the host later submits, resets or abandons it. Transactional callbacks make this decision explicit, but cannot verify the host's claim. A host that submits after cancel, cancels after submit, or commits a failed recording violates the contract.

If `aroma_fg_process()` returns an error for a transactional backend, the host must still cancel that frame ID after abandoning/resetting the command buffer. A backend may have recorded a partial command sequence before detecting a later allocation/descriptor error. Never submit such a command buffer.

## Queue and resource ordering

The current Vulkan runtime borrows one host queue and uses shared private intermediate images across frame slots. The host must submit AromaFG command buffers to that same queue in callback order and provide the required queue external synchronization. Do not record/submit work on multiple queues or reorder submissions. Frame-slot descriptors and timestamp pools are separate, but this does not make the intermediate image graph multi-queue or independently concurrent.

## Forbidden behavior

- Creating a second device or queue for the same host renderer.
- Submitting the host command buffer or waiting synchronously for GPU completion.
- Full-frame CPU readback in the frame path.
- Destroying host objects or retaining borrowed CPU pointers.
- Taking ownership of WSI semaphores, acquire/present or pacing.
- Claiming a generated frame was displayed before successful present confirmation.

## Layout, capabilities and synchronization

AromaFG records internal barriers and transitions source/output images according to the Vulkan interop frame payload. The host supplies a command buffer recording on the configured queue family and supplies accurate initial/restore layouts, stages and access masks.

The source format must support sampled-image access **and linear filtering**, because the runtime's source sampler uses linear filtering. Internal R16_SFLOAT/R16G16_SFLOAT storage support and output storage-image format support are queried at initialization/per frame. The host must enable `shaderStorageImageExtendedFormats` when required by the runtime.

VK_IMAGE_LAYOUT_GENERAL is not a substitute for execution or memory dependencies. Layout, access masks, pipeline stages, queue-family ownership and external synchronization must all be correct. Do not assume that placing every image in GENERAL eliminates barriers.

## Failure behavior

On unsupported format/capability, invalid interop payload, frame discontinuity, backend failure or device loss, stop generating and preserve the source-frame path. Resize and destruction require host-guaranteed quiescence. A failed recording must be cancelled before retry.
