# Vulkan host interop and ownership contract

## Ownership table

| Resource | Owner | AromaFG access |
|---|---|---|
| VkInstance / VkPhysicalDevice / VkDevice | Host | Borrowed/read-only during callback |
| VkQueue / queue family | Host | Queue metadata only; no submit |
| VkCommandBuffer | Host | Record commands only |
| Source image/view | Host | Borrowed for callback; preserve declared layout |
| Generated output slots | Host | Write only within declared capacity |
| Swapchain / acquire/present semaphores | Host | Never owned or destroyed by AromaFG |
| Pipelines, descriptors, samplers, private intermediate images | AromaFG | Owned and destroyed by AromaFG |

## Callback lifecycle

- initialize: validate capabilities/formats and allocate private resources.
- update_config: apply settings at a serialized safe point.
- reset_history: invalidate temporal history after scene cut, frame gap, resize, format/device/swapchain recreation or enable transition.
- process_frame: record bounded compute work into host command buffer.
- shutdown: release private resources before host device teardown.

All borrowed handles expire at callback return unless a future ABI explicitly grants a longer lifetime.

## Forbidden behavior

- Creating a second device or queue for the same host renderer.
- Submitting the host command buffer or waiting synchronously for GPU completion.
- Full-frame CPU readback in the frame path.
- Destroying host objects or retaining borrowed handles.
- Taking ownership of WSI semaphores, acquire/present or pacing.
- Claiming a generated frame was displayed before successful present confirmation.

## Layout and synchronization

AromaFG documents the required source and output layouts and records all internal barriers in its command stream. The host supplies a command buffer already recording on a compatible queue family. Initial ABI should avoid queue-family ownership transfers. Use capability/format checks before pipeline creation. Internal compute images may use GENERAL where appropriate, but sampled/storage transitions and access masks must be explicit and validated; do not assume GENERAL alone removes synchronization requirements.

## Failure behavior

On unsupported format, invalid interop payload, frame discontinuity, backend failure or device loss, stop generating and preserve the source-frame path. Resource recreation is performed only at a host-defined safe lifecycle point.
