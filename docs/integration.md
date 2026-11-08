# Integration guide

## Preferred path: native host-side integration

The host should call AromaFG from its serialized render/compositor command path and provide:
- source frame metadata and image;
- compatible Vulkan device/queue family and recording command buffer;
- preallocated output image slots;
- source/output layouts and supported format details;
- monotonically increasing frame ID and explicit history-reset events.

AromaFG records bounded compute work only. Host submits the command buffer and handles generated outputs and presentation.

## Transaction protocol (Vulkan backend)

The Vulkan adapter is transactional. The host must implement this exact sequence:

~~~cpp
AromaFGFrameOutput output{};
output.struct_size = sizeof(output);

AromaFGStatus status = aroma_fg_process(engine, &input, &output);
if (status == AROMA_FG_BYPASSED) {
    // No recording transaction was created. Use the real frame path.
} else if (status != AROMA_FG_OK) {
    // For a valid, serialized frame call, backend errors may follow partial recording.
    // Discard/reset the command buffer before releasing that transaction.
    vkResetCommandPool(device, command_pool, 0);
    (void)aroma_fg_cancel_frame(engine, input.frame_id);
    // Present the real frame / use host fallback.
} else {
    VkResult submit = vkQueueSubmit(queue, 1, &submit_info, frame_fence);
    if (submit == VK_SUCCESS) {
        // Commit only after the host queue accepted the command buffer.
        status = aroma_fg_commit_frame(engine, input.frame_id);
        if (status != AROMA_FG_OK) {
            // Submission already happened: do NOT cancel. Treat as fatal backend/
            // integration error, wait for the fence, then tear down after quiescence.
        }
    } else {
        vkResetCommandPool(device, command_pool, 0);
        (void)aroma_fg_cancel_frame(engine, input.frame_id);
        // Present the real frame / use host fallback.
    }
}
~~~

The snippet is illustrative: reset only a command pool/command buffer that the host owns and can legally reset; ensure it is not pending. Do not submit a command buffer after cancellation. Do not call commit after a failed recording or failed queue submission.

A successful `aroma_fg_process()` means **recorded**, not submitted or displayed. A successful commit means the host queue accepted the submission, not that GPU execution or presentation completed. Wait for the host fence before reusing a frame slot. Call `aroma_fg_vulkan_backend_collect_timing()` only after the corresponding slot fence is signaled. If the call returns BUSY, do not reuse that slot yet.

The current core permits one outstanding CPU recording transaction per engine. Commit/cancel must be called before the next process/config call. The Vulkan runtime borrows one queue and uses shared intermediate images; submit command buffers to that queue in callback order. Multi-queue recording/submission is unsupported.

Legacy non-transactional backends keep commit-on-success behavior. They do not gain command-buffer abandonment guarantees.

## WinNative

Treat X11 renderer and Wayland compositor as separate integrations. They may share portable core and compute code but have different output formats, surface behavior, synchronization and UI. Validate each independently.

## Winlator / DXVK

No universal hook is assumed. Select and pin the exact Winlator port and DXVK fork/version. A DXVK adapter covers only the rendering path routed through that DXVK build; compositor integration may be required for WineD3D or native Vulkan. Never transplant private DXVK classes from a different fork without adapting and validating.

## Vulkan Layer path

A layer is secondary and optional. It must not create a competing presenter or steal WSI ownership. Keep host command submission and synchronization semantics intact. Start with native integration to prove the backend before adding interception.

## Driver/runtime boundaries

AromaFG operates on a valid Vulkan ICD (Turnip or proprietary). Driver loading belongs to the host (for example, a host using adrenotools). AromaFG does not load drivers, translate DXIL, decompress BCn or depend on Vortek/bcn_layer/bionic-vulkan-wrapper.

## Required tests

Verify device/queue ownership, format support (including linear source filtering), layouts/barriers, frame ordering, phase chronology, submit/commit, command-buffer abandon/cancel, failed recording, slot fences/query retirement, resize, swapchain/device recreation, scene cuts, device loss, acquire misses, successful presents, fallback, audio and sustained thermal behavior.
