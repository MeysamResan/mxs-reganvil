# RegAnvil changelog

## Unreleased

- Add a generated documentation website, practical MMIO/W1C/read-clear guides, crawlable metadata, sitemap, and Markdown/llms documentation exports.
- Configure verified main-push GitHub Pages deployment, Context7 registration/refresh, and optional repository metadata synchronization; document the required one-time owner setup.
- Add current Context7 indexing configuration and a tested vcpkg overlay pinned to the immutable 0.1.0 source release.
- Check generated links, metadata, tutorial execution, and automation error handling without making external requests during tests.

## 0.1.0 - 2026-09-10

First release of the RegAnvil native C register model.

- Portable C99 library with one public header and one implementation file, caller-owned storage, explicit status codes, readable status strings, and no heap allocation or mutable global state.
- 32-bit RW, RO, write-one-to-clear, reserved-bit, and read-clear behavior; validated initialization; reset; side-effect-free peek; and validated simulated hardware set/clear operations.
- Optional bounded software-access trace with before/after values, record copying, trace inspection and clearing, and a saturating dropped-event counter that never prevents register accesses.
- Always-active behavioral tests, storage sentinels, failure preservation, independent instances, and deterministic randomized comparison with a separate per-bit reference model.
- Callback-based interrupt demonstration that detects a W1C read/modify/write bug and verifies selective acknowledgement.
- Original RP2040 GPIO0–7 driver fixture with datasheet-sourced register definitions and checks for enable masks, selective edge acknowledgement, read-only levels, callback failures, and independent instances.
- CMake/CTest integration, private optional ASan/UBSan instrumentation, static target `mxs::reganvil`, install/export rules, relocatable package configuration, and independent source and installed consumers.
- Native Linux GCC/Clang, Windows MSVC, macOS Clang, and Linux sanitizer CI configurations.
- Repeatable PowerShell verification with fresh builds, retained logs and JSON summaries, and explicit sanitizer-skip reporting.
- Complete README with architecture and register-behavior diagrams, a runnable example, driver integration, API/status reference, ownership rules, and build/install guidance.
- MIT license, a dated verification record, and comments documenting the core invariants and deliberate complexity/storage tradeoffs.
