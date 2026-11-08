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

1. **CPU callback payload:** pointers to AromaFGVulkanFrameV2, arrays and other host-owned structs are borrowed for the callback. AromaFG must not retain those CPU pointers after return.
2. **Vulkan objects referenced by recorded commands:** source/output images, image views, device and command buffer must remain valid until the host submission containing AromaFG commands has completed. The host must not recycle or destroy them merely because aroma_fg_process() returned.

Before destroying AromaFG, the host must ensure all submissions referencing AromaFG resources have completed. Before resizing/reallocating backend resources, the host must establish the documented quiescent point.

## Callback lifecycle

- initialize: validate device capabilities/formats and allocate private resources.
- update_config: apply settings at a serialized safe point.
- reset_history: invalidate temporal state after scene cut, frame gap, resize, format/device/swapchain recreation or enable transition.
- process_frame: record bounded compute work into a host command buffer.
- shutdown: release private resources before host device teardown, after host-established quiescence.

## Submission and command-buffer failure

AromaFG never submits the host command buffer and cannot know whether the host later submits, resets or abandons it. The current API does not yet provide a complete explicit commit/cancel notification for recorded work. This is a production blocker for robust slot/query/resource state tracking.

Until that contract is extended and validated:

- host must treat a callback that records partial commands and then returns an error as an invalid command-buffer recording; do not submit that command buffer;
- host must reset/re-record the command buffer according to Vulkan rules;
- do not reuse a slot or destroy resources based only on callback return;
- slot reuse requires the host fence for the corresponding successful submission to be signaled;
- cancellation/recovery behavior must be explicitly tested before enabling this backend in a general-purpose host.

## Forbidden behavior

- Creating a second device or queue for the same host renderer.
- Submitting the host command buffer or waiting synchronously for GPU completion.
- Full-frame CPU readback in the frame path.
- Destroying host objects or retaining borrowed CPU pointers.
- Taking ownership of WSI semaphores, acquire/present or pacing.
- Claiming a generated frame was displayed before successful present confirmation.

## Layout and synchronization

AromaFG records internal barriers and transitions source/output images according to the Vulkan interop frame payload. The host supplies a command buffer recording on a compatible queue family and supplies accurate initial/restore layouts, stages and access masks.

VK_IMAGE_LAYOUT_GENERAL is not a substitute for execution or memory dependencies. Layout, access masks, pipeline stages, queue-family ownership and external synchronization must all be correct. Do not assume that placing every image in GENERAL eliminates barriers.

## Failure behavior

On unsupported format, invalid interop payload, frame discontinuity, backend failure or device loss, stop generating and preserve the source-frame path. Resource recreation is allowed only at a host-defined safe lifecycle point.
