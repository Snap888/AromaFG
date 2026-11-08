# AromaFG architecture

## Scope and maturity

AromaFG is a host-integrated optical-flow frame-generation library. The portable C ABI/lifecycle skeleton is on main; the experimental Vulkan compute runtime is on feature/vulkan-engine-runtime. A successful software Vulkan smoke test does not establish physical Adreno compatibility, production quality or real-game delivery.

## Layers

1. **Portable core:** versioned C ABI, configuration, frame IDs, lifecycle, bypass and backend callback contract.
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
  I --> J[Host handles acquire and present]
  J --> K[Confirmed event telemetry, if adapter supplies it]
~~~

For multiplier M, the runtime may generate at most M−1 distinct chronological phases. Motion is computed once for the real-frame pair and reused by each synthesis phase.

## Vulkan ownership

Host owns the Vulkan instance, physical device, logical device, queue, command buffers, swapchain, WSI semaphores, submission and presentation. AromaFG owns only its private pipelines, descriptors, samplers, intermediate images and allocations.

Two lifetimes must not be conflated:

- **Callback payload lifetime:** borrowed pointers/struct addresses are consumed during the API callback and must not be retained as host memory pointers.
- **GPU object lifetime:** Vulkan handles referenced by recorded commands (source/output images, views, device, command buffer) must remain valid until those commands have completed. The host must wait for the relevant submission before destroying/recycling them.

A callback returning does not mean GPU use has completed.

## Submission and cancellation boundary

The host is the only queue submitter. The current callback-oriented API does not yet expose a fully explicit submission-commit/cancel handshake. The runtime therefore needs a reviewed contract for command-buffer recording failure, reset/abandonment and query-slot reuse before production integration. Do not assume that recording commands is equivalent to successful submission.

## Failure model

Invalid frame order, scene discontinuity, unsupported formats/capabilities, backend failure or device loss must preserve the real-frame path. Resize and destruction require host-guaranteed quiescence. AromaFG must not perform synchronous device/queue waits or full-frame CPU readback in the render path.

## Policy boundary

An adaptive controller may consume validated scalar metrics and recommend bounded workload changes for later frames. It must not control barriers, queue ownership, semaphore waits, swapchain acquisition or present scheduling.
