# Contributing to AromaFG

## Development model

- `main`: stable, reviewed changes.
- `develop`: integration branch when introduced.
- `feature/*`: focused work branches.
- `fix/*`: focused bug fixes.

## Rules

1. Keep the implementation clean-room. Do not copy proprietary, obfuscated or unreviewed third-party code.
2. Keep public C ABI changes versioned and append-compatible where possible. Document ABI changes.
3. Keep host Vulkan ownership explicit. No hidden queue submission, WSI takeover or synchronous waits.
4. Every algorithm change must include correctness tests and benchmark methodology.
5. Update docs and ROADMAP in the same change as behavior/API changes.
6. For major algorithm or ABI changes, create a backup tag before merging.
7. Use descriptive commits: `feat:`, `fix:`, `perf:`, `docs:`, `test:`, `build:`.

## Pull requests

Include motivation, design/trade-offs, changed files, test results, supported devices/drivers, performance data and known limitations. Never describe cross-compilation as physical-device validation.