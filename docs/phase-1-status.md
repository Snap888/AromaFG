# Phase 1 documentation baseline

## Purpose

This document records the documentation state after the Phase 0 repository and implementation audit. It describes the experimental feature branch separately from the default branch; it does not claim that the feature branch has been merged or that physical hardware has been tested.

## Verified repository state at audit

- Default branch main contains the portable C ABI and lifecycle bootstrap.
- feature/optical-flow-core contains the initial GLSL optical-flow work.
- feature/vulkan-backend adds Vulkan interop and orchestration infrastructure.
- feature/vulkan-engine-runtime is stacked on feature/vulkan-backend and adds the Vulkan compute runtime and device smoke test.
- The three associated pull requests are separate and dependent; preserve their branch topology when reviewing or integrating them.
- CI has passed on Linux. The runtime smoke job uses Mesa lavapipe, not a physical Adreno GPU.

## Implemented runtime scope

The experimental runtime records a baseline Vulkan compute graph for luma, image pyramid, coarse-to-fine bidirectional block search, flow refinement and phase-specific synthesis. It uses private intermediate images, compute pipelines, descriptor pools and timestamp query pools. The host owns the Vulkan device and queue submission.

Supported output formats and required enabled features are documented in docs/vulkan-backend.md. Do not assume broad swapchain-format support.

## Known blockers and limitations

### P0 — command recording and submission lifecycle

The API callback records commands but has no complete explicit commit/cancel handshake with the host. If a host command buffer is abandoned after AromaFG has updated internal slot/resource bookkeeping, internal state can diverge from actual GPU execution. Define and test submission confirmation, cancellation, slot reuse and recovery before production integration.

### P0 — retry semantics

The portable engine advances frame-order state around backend processing. BUSY/error retry behavior must be reviewed so a frame rejected for temporary resource contention can be retried without falsely violating frame order.

### P1 — capability validation

The backend must verify every format feature actually used, including linear filtering when linear samplers are used, plus storage usage, enabled device features, workgroup limits and timestamp support.

### P1 — resource memory

Private images are allocated separately. Memory suballocation, budget-aware pooling and memory-pressure recovery are not implemented.

### P1 — shared resource concurrency

The current runtime assumes one borrowed queue and callback-order submissions. Multi-queue/concurrent execution is unsupported. Resize and destruction require host-established quiescence.

### P1 — baseline quality

The current algorithm is a classical baseline. C1 global-motion pre-warp, C2 edge-preserving regularization, advanced occlusion-aware reconstruction, HUD exclusion and a complete anti-ghost stack are not implemented.

### Not implemented

AI inference/refinement, Android SDK packaging, Winlator/WinNative/DXVK adapters, Vulkan Layer, emulator integrations and production pacing are not complete.

## Evidence boundary

A passing build establishes compilation. A passing lavapipe smoke test establishes that the tested graph executed on that software Vulkan implementation under the CI configuration. Neither establishes:

- compatibility with Adreno or a specific Turnip revision;
- compatibility with Qualcomm proprietary drivers;
- acceptable GPU cost, latency, thermals or memory use on mobile;
- visual quality on real game scenes;
- successful delivery of generated frames to a compositor or physical display.

## Phase 1 exit checklist

- [x] README describes branch-specific implementation maturity.
- [x] Architecture separates CPU callback payload lifetime from GPU object lifetime.
- [x] Vulkan host contract describes host ownership and the submission/cancellation gap.
- [x] Build instructions distinguish portable and Vulkan runtime modes.
- [x] Adreno/Turnip plan distinguishes lavapipe from physical hardware.
- [x] Roadmap contains measurable downstream gates and known blockers.
- [ ] Documentation PR reviewed and accepted.
- [ ] Documentation facts cross-checked against current branch heads immediately before merge.

## Next phase

Phase 2 is research and technology selection. It must independently verify repositories, exact source revisions, file-level licenses, shader provenance, model code/weights/datasets, candidate optical-flow algorithms and mobile inference runtimes. No external source or model should be copied or added merely because its repository-level license appears permissive.
