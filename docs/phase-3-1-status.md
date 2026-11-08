# Phase 3.1 — Host integration validation

**Branch:** `feature/phase-3-1-host-validation`  
**Base:** `feature/phase-3-host-lifecycle`  
**Purpose:** exercise the host transaction contract with real Vulkan command buffers and independently compiled legacy ABI client code.

## Added validation

### Vulkan host transaction integration test

`tests/vulkan_host_transaction.cpp` creates a real Vulkan instance, compute-capable queue, device buffer, command pool, command buffer and fence. A small transactional test backend records `vkCmdFillBuffer` into the host command buffer.

The test exercises this sequence:

1. Begin a host command buffer and call `aroma_fg_process()` for frame 1.
2. End and reset the command pool without submitting the command buffer.
3. Call `aroma_fg_cancel_frame()` only after reset/abandonment.
4. Begin a new command buffer and retry frame ID 1.
5. Submit successfully, then call `aroma_fg_commit_frame()`.
6. Wait for the host fence.
7. Reset the fence and command pool, record frame ID 2 into the same host slot, submit, commit and wait again.

This validates the API/host handshake and actual Vulkan command-buffer abandonment/reuse on the CI software driver. The backend is deliberately a test double; this test does **not** exercise AromaFG's full Vulkan optical-flow runtime during the cancellation/retry path.

### Independent legacy ABI client

- `tests/legacy_abi/legacy_aroma_fg_v1.h` freezes the pre-transaction public declarations and original backend layout.
- `tests/legacy_abi/legacy_abi_client.c` is compiled as a separate C translation unit against that old header and links to the current library.
- The client creates an engine and processes a frame using the old V1 struct, without knowing the appended transaction callbacks.

This is a real separately compiled compatibility check, not merely setting an old `struct_size` in a test compiled against the new header.

### CI

The workflow registers both tests in CTest and explicitly selects Mesa lavapipe for the test step. The full Vulkan runtime smoke remains a separate CI step.

## Evidence boundaries

A passing lavapipe run demonstrates the transaction protocol on a software Vulkan implementation. It does not prove:
- physical Adreno/Turnip or proprietary Qualcomm driver behavior;
- full AromaFG Vulkan runtime cancel/retry correctness on a device;
- Winlator, DXVK, WineD3D or game-host integration;
- device-loss recovery, multi-queue execution or presentation correctness.

## Remaining Phase 3.1 gates

- [ ] CI passes for the independent legacy client and Vulkan host transaction test.
- [ ] Review CTest output and synchronization/validation diagnostics.
- [ ] Run full AromaFG runtime on a physical Vulkan host, including abandon/cancel/retry.
- [ ] Confirm slot reuse with real driver fences and timestamp-query retirement.
- [ ] Test target Adreno with a pinned Turnip build and proprietary driver.
- [ ] Close remaining API review comments and resolve Phase 2/Phase 1 dependency PRs before merging the stack.

**Exit decision:** test implementation is prepared. Phase 3.1 remains open until the new CI run is green and real-device host validation is completed.
