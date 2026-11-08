# Optical Flow Core — baseline contract

This milestone establishes five offline-compiled compute passes. It is a transparent classical baseline, not a claim of production quality.

## Pass order

1. `luma.comp`: RGBA8 source to R16F luma.
2. `downsample.comp`: 2×2 luma pyramid reduction.
3. `search.comp`: local 3×3 SAD block matching. Dispatch A→B and B→A. At coarse levels search a broad radius; at finer levels seed with the previous level's upscaled flow and narrow the radius.
4. `refine.comp`: forward/backward cycle consistency; dispatch both directions.
5. `interp.comp`: one dispatch per phase. For multiplier M, dispatch t=k/M for k=1..M-1.

## Image contract

- Luma: `VK_FORMAT_R16_SFLOAT`.
- Flow: `VK_FORMAT_R16G16_SFLOAT`, displacement in pixels of the currently bound pyramid level.
- Confidence: `VK_FORMAT_R16_SFLOAT`, heuristic [0,1].
- Interpolation source/output: `VK_FORMAT_R16G16B16A16_SFLOAT`.
- All storage images use `VK_IMAGE_LAYOUT_GENERAL` during compute.
- Host/backend inserts explicit write→read compute barriers between dependent dispatches. GENERAL layout does not remove memory dependencies.
- Workgroup size is 8×8; dispatch dimensions are ceil(width/8), ceil(height/8).
- Integer imageLoad sampling means no sampler objects are required.

## Limitations

Search is exhaustive in its configured local window and intentionally a correctness-oriented baseline. Runtime grows with search area and 3×3 patch cost. The seed flow must already be rescaled/upscaled into the current pyramid level's pixel coordinate system. Confidence uses best/second-best separation and a boundary penalty; it is not calibrated occlusion probability. Interpolation is a baseline symmetric warp; C1, C2 TV-L1, parabolic refinement, 5-tap median and content-difference disocclusion logic are not implemented yet. Color/HDR conversion is not generalized. No physical Adreno/Turnip measurements have been performed.

## Adreno notes

Start with 8×8 local size and low-resolution flow. Profile search radius and pyramid depth on actual devices; the nested search loop is expensive. Keep intermediate images at reduced flow resolution where quality permits and avoid unnecessary full-resolution intermediates. Do not assume GMEM residency for storage-image compute. Use per-pass timestamp queries and explicit barriers. Tune from measured occupancy, bandwidth and thermal data.
