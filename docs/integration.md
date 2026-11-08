# Integration guide

## Preferred path: native host-side integration

The host should call AromaFG from its serialized render/compositor command path and provide:
- source frame metadata and image;
- compatible Vulkan device/queue family and recording command buffer;
- preallocated output image slots;
- source/output layouts and supported format details;
- monotonically increasing frame ID and explicit history-reset events.

AromaFG records bounded compute work only. Host submits the command buffer and handles generated outputs and presentation.

## WinNative

Treat X11 renderer and Wayland compositor as separate integrations. They may share portable core and compute code but have different output formats, surface behavior, synchronization and UI. Validate each independently.

## Winlator / DXVK

No universal hook is assumed. Select and pin the exact Winlator port and DXVK fork/version. A DXVK adapter covers only the rendering path routed through that DXVK build; compositor integration may be required for WineD3D or native Vulkan. Never transplant private DXVK classes from a different fork without adapting and validating.

## Vulkan Layer path

A layer is secondary and optional. It must not create a competing presenter or steal WSI ownership. Keep host command submission and synchronization semantics intact. Start with native integration to prove the backend before adding interception.

## Driver/runtime boundaries

AromaFG operates on a valid Vulkan ICD (Turnip or proprietary). Driver loading belongs to the host (for example, a host using adrenotools). AromaFG does not load drivers, translate DXIL, decompress BCn or depend on Vortek/bcn_layer/bionic-vulkan-wrapper.

## Required tests

Verify device/queue ownership, format support, layouts/barriers, frame ordering, phase chronology, resize, swapchain/device recreation, scene cuts, device loss, acquire misses, successful presents, fallback, audio and sustained thermal behavior.