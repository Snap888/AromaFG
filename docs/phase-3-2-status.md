# Phase 3.2 — ABI and fence-backed runtime slot validation

## Scope

This step closes two software-validation gaps around the Phase 3 transaction API:

1. Verify the pre-transaction V1 ABI from a separately compiled C translation unit.
2. Verify frame-slot and descriptor/query-pool reuse in the actual Vulkan Frame Generation runtime after host fence completion.

## Existing ABI compatibility test

- `tests/legacy_abi/legacy_aroma_fg_v1.h` freezes the original V1 public structure layout before `commit_frame` and `cancel_frame` were appended.
- `tests/legacy_abi/legacy_abi_client.c` is compiled as C against that frozen header and linked to the current C++ library.
- The client creates the engine and processes a frame through the legacy backend callback contract.
- This is a separate translation unit and real old-header ABI boundary, not only a current-header `struct_size` simulation.

## Existing host transaction test

`tests/vulkan_host_transaction.cpp` uses a small test backend that records a real `vkCmdFillBuffer` command. It validates:

- command-buffer recording followed by host reset/abandon and `cancel_frame`;
- retry using the same frame ID;
- queue submit before `commit_frame`;
- fence wait before command-pool reuse;
- a subsequent frame after the fence.

This validates core transaction semantics with real Vulkan command buffers, but it is a test backend double; it does not execute AromaFG's optical-flow compute runtime.

## Added actual-runtime slot test

The device smoke test now extends the full Vulkan compute graph sequence:

1. Run the existing cancel/reset/retry path for frame 1 on slot 0.
2. Submit frame 1, commit it, and wait for its fence.
3. Collect slot 0 timing data (when supported) before query-pool reuse.
4. Run frame 2 on slot 1, submit, commit and wait for its fence.
5. Run frame 3 on slot 0, proving reuse of the same runtime slot after its earlier fence completed.
6. Collect timing data after each fence; `AROMA_FG_BYPASSED` is accepted when timestamp queries are unavailable/disabled.

The test explicitly updates output image layouts and transfer access state after the first frame, then uses transfer-source state as the next frame's input state for those output images.

## CI evidence

- Existing ABI + host transaction CTest coverage passed in GitHub Actions run [#42522045599](https://github.com/Snap888/AromaFG/actions/runs/42522045599).
- The new full-runtime slot 1 / slot 0 reuse extension is included in this branch. Its CI result is pending until the new run completes.
- CI uses Mesa lavapipe. A pass is software Vulkan evidence only.

## Limitations / remaining gates

- This is sequential fence-backed slot reuse, not simultaneous multi-queue execution.
- Runtime intermediate images are shared across slots; host submission order remains mandatory.
- No claim is made for physical Adreno, Mesa Turnip on Android, or Qualcomm proprietary drivers.
- Device-loss recovery, resize under load, and sustained multi-frame in-flight stress remain open.
- PRs remain stacked; no merge into `main` is performed by this step.

## Exit status

Phase 3.2 implementation is prepared. It is complete only after the new CI run passes and the host/driver review gates are satisfied.
