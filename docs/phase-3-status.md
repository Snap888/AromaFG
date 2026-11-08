# Phase 3 — API and host lifecycle hardening status

**Branch:** `feature/phase-3-host-lifecycle`  
**Base:** `research/phase-2-technology-selection`  
**Scope:** API/core lifecycle, Vulkan recording transaction state, capability checks, tests and documentation.

## Implemented in this branch

### Public API and compatibility

- Appended optional `commit_frame` and `cancel_frame` callbacks to `AromaFGBackendV1`.
- Existing V1 backends whose `struct_size` ends at the original structure size remain accepted and retain commit-on-success behavior.
- Transactional backends must provide both callbacks; partial callback pairs and partial structure sizes are rejected.
- Added `aroma_fg_commit_frame(engine, frame_id)` and `aroma_fg_cancel_frame(engine, frame_id)`.
- A successful `aroma_fg_process()` for a transactional backend means commands were recorded, not submitted.
- Only a successful commit advances the committed frame ID. Cancelled or failed recordings may retry the same frame ID.
- Only one CPU recording transaction may be pending per engine. Process/config calls return BUSY while it is pending.
- Commit is rejected if the backend process callback failed or returned invalid output. Cancel remains available to release a possibly partially recorded command buffer.
- Frame-ID gaps and explicit scene cuts reset backend temporal history.

### Vulkan backend

- Vulkan backend opts into the transactional callback pair.
- Each slot now distinguishes a CPU recording reservation from a successfully submitted/timestamp-pending state.
- Slot timestamp state is marked pending only on commit, not merely because commands were recorded.
- Cancel releases the recording reservation and resets the slot descriptor pool after the host has abandoned/reset the command buffer.
- Automatic timing readback from the next process call was removed. Timing collection is explicit and must follow the host's submission-fence wait.
- Source format validation now checks both sampled-image access and linear-filter support, matching the sampler configuration.
- Existing format/feature checks for internal storage images and outputs remain.

### Tests and documentation

- Expanded portable contract tests for legacy auto-commit behavior, BUSY/error retry, commit, cancel, duplicate IDs and config mutation while pending.
- Updated the README, architecture and Vulkan host contract.
- Updated the roadmap and this status document.

## Host integration protocol

1. Record: call `aroma_fg_process()`.
2. If it returns an error, abandon/reset the command buffer and call `aroma_fg_cancel_frame()`.
3. If it succeeds, submit the command buffer to the configured borrowed queue.
4. Only if queue submission succeeds, call `aroma_fg_commit_frame()`.
5. Before reusing a Vulkan frame slot, wait for its host-owned submission fence. Then collect or retire timestamp queries for that slot.
6. Never cancel a submitted command buffer; never commit a failed/abandoned recording.

Commit releases the core's CPU transaction. It does not mean GPU completion or physical presentation.

## Known limitations / not solved by this phase

- The core can enforce API call order, but cannot prove that the host actually submitted, abandoned or reset the command buffer as claimed.
- The Vulkan runtime still uses shared private intermediate images across slots and one borrowed queue. Submission order on that queue is mandatory; multi-queue execution and independent per-slot intermediate graphs are not implemented.
- The host must guarantee queue external synchronization, object lifetime, slot fences, quiescence for resize and shutdown.
- Device-loss recovery and full output format conversion/fallback policy remain open.
- The callback API serializes recording transactions per engine. This is intentional for now; parallel CPU recording is not supported.
- No physical Adreno, Turnip or Qualcomm proprietary-driver test is claimed.
- GitHub Actions run [#42521641315](https://github.com/Snap888/AromaFG/actions/runs/42521641315) passed build, contract tests, SPIR-V validation, and the full Vulkan graph on Mesa lavapipe with synchronization validation. This is software-driver evidence only; it does not replace a real host integration or Adreno/Turnip validation.

## Review checklist

- [x] Preserve old backend struct-size compatibility.
- [x] Reject incomplete transactional callback pairs.
- [x] Ensure BUSY/error does not consume frame ID.
- [x] Require explicit commit/cancel for the Vulkan adapter.
- [x] Make descriptor/timestamp slot state reflect transaction outcome.
- [x] Gate source linear sampler use on format feature support.
- [x] CI build, contract tests, SPIR-V validation and lavapipe smoke test pass (run #42521641315).
- [ ] Add host integration test for successful submit/commit and abandon/cancel.
- [ ] Validate slot reuse, query collection and resize with actual fences.
- [ ] Review ABI compatibility with a separately compiled legacy client.
- [ ] Test on physical Adreno + Turnip and Qualcomm proprietary drivers.

**Exit decision:** implementation draft is prepared, but Phase 3 is not complete until CI, host integration, ABI compatibility and physical-device review gates are satisfied.
