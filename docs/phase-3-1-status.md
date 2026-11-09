# Phase 3.1 — Host Integration Validation

**Branch:** `feature/phase-3-1-host-validation-final`  
**Base:** `feature/phase-3-host-lifecycle`  
**Scope:** exercise the host-side command-buffer transaction contract in the Vulkan device smoke test.

## Added integration scenario

The Vulkan smoke test exercises the real adapter and core transaction callbacks in this order:

1. Begin recording a host-owned Vulkan command buffer and record source-image setup.
2. Call `aroma_fg_process` for frame 1, allowing AromaFG to append compute work.
3. Abandon that command buffer by resetting the host command pool.
4. Call `aroma_fg_cancel_frame(engine, 1)` only after the reset.
5. Begin a fresh command buffer and re-record the source/output setup that was discarded.
6. Retry the same frame ID (1), then end and submit the host command buffer.
7. Call `aroma_fg_commit_frame(engine, 1)` only after successful `vkQueueSubmit`.
8. Wait for the fence before collecting timing/readback and destroying Vulkan resources.

This covers the critical cancellation/retry path against an actual Vulkan implementation, rather than only a mock backend. The existing smoke test continues to validate generated output phases and synchronization validation when the validation layer is available.

## Physical smoke evidence

The standalone Vulkan smoke scenario (including reset-before-cancel and submit-before-commit) passed on ASUS AI2205 / SM8550 / Adreno 740 using the system Vulkan implementation, binary commit `fef098f`. See [physical device report](physical-device-validation-2026-11-09.md). Khronos validation and synchronization validation were disabled. This does not prove the same scenario on Turnip or through a borrowed game/Winlator device.

## What this does not prove

- It is not physical Qualcomm Adreno / Turnip / proprietary-driver validation.
- It does not establish multi-queue support; the runtime still assumes one borrowed queue and host submission order.
- It does not yet test repeated frame-slot reuse after multiple independent in-flight submissions.
- It does not test a separately compiled legacy ABI client binary.
- It does not test device-loss recovery or resize while work is in flight.
- A passing lavapipe run is software Vulkan evidence only.

## Exit gates

- [ ] CI build and contract tests pass on this branch.
- [ ] Full Vulkan graph and cancel/retry smoke test pass on Mesa lavapipe with synchronization validation.
- [ ] Review confirms reset-before-cancel and submit-before-commit ordering.
- [x] Physical Adreno 740 system-Vulkan standalone smoke on target Android device (commit `fef098f`; validation layers off).\n- [ ] Physical Mesa Turnip run on the target Android device.
- [ ] Physical Qualcomm proprietary-driver run where available.
- [ ] Separate legacy ABI binary compatibility test.
- [ ] Fence-backed repeated slot reuse and resize/quiescence test.

Phase 3.1 remains **in validation** until the applicable gates above are satisfied. No PR is merged by this work.
