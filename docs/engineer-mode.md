# Engineer Mode and telemetry

## Goals

Engineer Mode is opt-in diagnostics and must not add dispatches, allocations or readback to the normal path when disabled.

## GPU metrics

- Per-stage Vulkan timestamp queries, correlated to source-pair/frame IDs.
- Asynchronous small scalar reductions through a bounded staging/query ring.
- CPU reads only completed query slots; no render-thread waits.
- Report median, P90 and P95 across a capture window.

Initial reductions: mean normalized flow magnitude, magnitude deviation (complexity proxy), consistency-weighted confidence and confidence/consistency risk proxy. Label proxies honestly; risk is not calibrated artifact probability.

Planned signals: edge-aware vector disagreement, occlusion/disocclusion ratio, photometric warp residual, search-boundary saturation, scene-cut score, queue backlog, thermal state and WSI delivery ratio.

## Presentation events

Record separate event classes:
- source present accepted;
- generated present accepted;
- acquire attempt/success/timeout/error;
- compute completion.

Do not conflate these. Only successful present is counted as delivered. Physical scanout remains unknown unless a platform-specific trustworthy signal exists.

Each present record should contain logical present ID, frame ID, monotonic timestamp, CPU duration if available, swapchain generation, source/generated stream and API path.

## JSONL

One event per line; stable schema version, explicit units, frame ID, validity flags and source stage. Example:

```json
{"schema":1,"event":"gpu_stage","frame_id":42,"stage":"flow","duration_ms":1.83,"valid":true}
{"schema":1,"event":"present","logical_present_id":9001,"frame_id":42,"stream":"generated","timestamp_ns":1234567890,"swapchain_generation":3,"accepted":true}
{"schema":1,"event":"flow_metrics","frame_id":42,"mean_magnitude":0.12,"magnitude_deviation":0.08,"confidence":0.91,"risk_proxy":0.09,"valid":true}
```

Never serialize unavailable values as valid zeroes. Use explicit validity flags/null fields. Bounded ring buffers; export and percentile sorting on a diagnostics thread.
