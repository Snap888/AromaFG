# Algorithm and architecture decisions

This document records the design choices selected after reviewing SnapFrameBackendV1. These are AromaFG requirements, not imported implementation code.

## 1. Portable engine and backend boundary

- Expose a versioned C ABI with `struct_size` and API version.
- Keep graphics handles opaque in the portable core.
- Backend callbacks: initialize, update configuration, reset history, process frame, shutdown.
- Never allow C++ exceptions to cross the C ABI.
- Reject out-of-order frame IDs and concurrent process calls explicitly.
- Every error path must preserve a real-frame bypass path.

## 2. Temporal phase contract

For multiplier M in {1,2,3,4}, maximum generated phase count is M-1. Phases must be distinct and chronological. Multiplier 1 is bypass. Flow estimation is done once for a consecutive source pair and reused by every synthesis phase. Host delivery count is tracked separately from backend-generated count.

## 3. Flow pipeline

Baseline stages:
1. Luma extraction and pyramid.
2. Forward and backward local/block motion estimation.
3. Motion fusion.
4. Forward/backward cycle consistency.
5. Confidence and scalar metrics.
6. Phase-specific warp and reconstruction.

Block matching should expose best cost, second-best separation, boundary saturation and validity. A low confidence score is not proof of occlusion; keep these signals distinct.

## 4. Candidate reconstruction

After baseline warp is stable, evaluate a bounded candidate set from primary forward/backward estimates and edge-compatible neighbours. Score candidates using confidence and photometric disagreement. Invalid coordinates receive zero weight. Use numerically bounded normalization. If all candidates fail validation, select a sharp real-frame fallback. Candidate count and scoring cost must be profile-controlled.

## 5. Adaptive workload policy

Implement a deterministic controller, not a mandatory neural model. Inputs carry frame ID, validity, GPU time, motion magnitude/complexity, flow confidence, artifact-risk proxy and scene-cut flag. Smooth metrics and apply hysteresis. Decisions are advisory and clamped to host/backend capability. Missing signals stay invalid; never replace them with fabricated zeroes or perfect confidence. Decisions affect subsequent frames only.

Future features: edge-aware vector disagreement, explicit occlusion ratio, photometric residual, search-boundary saturation, scene-cut score, queue backlog, thermal state and WSI delivery ratio.

## 6. Presentation telemetry

Record only confirmed successful source/generated presents at a host-defined boundary. Include logical event ID, frame ID, monotonic timestamp, stream, API and swapchain generation. Reject duplicate IDs and stale generations. Use bounded storage. Snapshot sorting and percentile calculation belong on a diagnostics thread.

## 7. WSI and host ownership

Host exclusively owns queue submission, semaphores, swapchain acquire/present and pacing. Backend must not decide WSI waits or infer scanout. If generated-slot acquire misses, host may fall back to source frame. Any feedback into adaptive policy is based on validated rolling statistics, never single noisy events.

## 8. Quality and performance gates

Measure each stage with Vulkan timestamps correlated to source pair/frame ID. Report median, P90/P95. Compare against real high-frame-rate midpoint ground truth. Evaluate PSNR/SSIM with correct color-space handling, edge/gradient error, occlusion boundary, temporal stability, pacing, latency, power and thermal behavior. Never claim superiority from requested multiplier or generated compute count alone.

## 9. Adreno/Turnip policy

Start with conservative 8x8 compute workgroups as a baseline, then profile on actual target devices. Keep intermediate flow resolution configurable. Avoid assuming GMEM benefits for compute storage images; minimize unnecessary full-resolution intermediates and memory traffic. Use explicit Vulkan capability/format checks and barriers. Do not hardcode X11/Wayland timeout values from another host.