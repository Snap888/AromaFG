# AromaFG architecture

## Scope and maturity

AromaFG is a host-integrated optical-flow frame-generation library. The portable C ABI/lifecycle skeleton is on main; the experimental Vulkan compute runtime is on feature branches. A successful software Vulkan smoke test does not establish physical Adreno compatibility, production quality or real-game delivery.

## Layers

1. **Portable core:** versioned C ABI, configuration, committed frame IDs, recording transactions, lifecycle, bypass and backend callbacks.
2. **Compute backend:** Vulkan pipelines, private intermediate resources, flow estimation and synthesis.
3. **Host adapter:** supplies borrowed resources and a recording command buffer; owns queue submission, swapchain, semaphores and presentation.
4. **Policy/telemetry:** deterministic adaptive recommendations and passive performance/event recording. These are planned interfaces, not all implemented.
5. **Tools/platform adapters:** offline shader build, JSONL analysis, benchmark harness, Android and host integrations.

## Frame flow

~~~mermaid
flowchart TD
  A[Host real source pair] --> B[Validate frame IDs and metadata]
  B --> C[Luma and image pyramid]
  C --> D[Forward and backward motion search]
  D --> E[Consistency refinement]
  E --> F[One flow solution for the pair]
  F --> G[Phase-specific synthesis k / M]
  G --> H[Host-owned output slots]
  H --> I[Host submits command buffer]
  I --> J{Submit succeeded?}
  J -->|yes| K[Host calls AromaFG commit]
  J -->|no / abandoned| L[Host resets command buffer and calls cancel]
  K --> M[Host handles acquire and present]
  M --> N[Confirmed event telemetry, if adapter supplies it]
~~~

For multiplier M, the runtime may generate at most M−1 distinct chronological phases. Motion is computed once for the real-frame pair and reused by each synthesis phase.

## Vulkan ownership and transaction boundary

Host owns the Vulkan instance, physical device, logical device, queue, command buffers, swapchain, WSI semaphores, submission and presentation. AromaFG owns only its private pipelines, descriptors, samplers, intermediate images and allocations.

Two lifetimes must not be conflated:

- **Callback payload lifetime:** borrowed pointers/struct addresses are consumed during the API callback and must not be retained as host memory pointers.
- **GPU object lifetime:** Vulkan handles referenced by recorded commands (source/output images, views, device, command buffer) must remain valid until those commands have completed. The host must wait for the relevant submission before destroying/recycling them.

A transactional backend separates recording from submission confirmation. A successful `aroma_fg_process()` means commands were recorded. The host calls `aroma_fg_commit_frame()` only after successful queue submission, or abandons/resets the command buffer and calls `aroma_fg_cancel_frame()`. The frame ID advances only on commit. A cancelled recording may be retried with the same ID. Commit does not mean GPU completion; slot reuse still requires the host fence.

The current Vulkan runtime uses one borrowed queue and shared private intermediate images. Host submissions must follow callback order on that queue. Per-slot descriptors/query pools do not imply multi-queue or independent intermediate-resource concurrency.

## Failure model

Invalid frame order, scene discontinuity, unsupported formats/capabilities, backend failure or device loss must preserve the real-frame path. Frame gaps reset temporal history. Resize and destruction require host-guaranteed quiescence. AromaFG must not perform synchronous device/queue waits or full-frame CPU readback in the render path.

## Policy boundary

An adaptive controller may consume validated scalar metrics and recommend bounded workload changes for later frames. It must not control barriers, queue ownership, semaphore waits, swapchain acquisition or present scheduling.
