# RegAnvil verification

This file separates the repeatable checks shipped with RegAnvil from a dated record of checks actually executed. A configured CI job is not evidence that it passed. Physical-hardware behavior is outside the native register model's verification scope.

## Repeatable checks

A checkout needs a C compiler, CMake 3.20 or later, and a native build tool on `PATH`. The build performs no downloads. From the project root, the PowerShell helper runs fresh Debug, Release, ASan/UBSan, and library-only builds:

```powershell
./scripts/verify.ps1
```

It creates `.build/verification/<run>/`, retains command logs and `summary.json`, and writes the latest result to `.build/verification/latest.json`. Requested failures stop the run. `-Compiler` and `-Generator` select tools; `-SkipSanitizers` explicitly omits unsupported sanitizer checks and records that omission. For example:

```powershell
./scripts/verify.ps1 -Generator 'Visual Studio 17 2022' -SkipSanitizers
```

The helper supports PowerShell 7 and Windows PowerShell 5.1. An ignored `.build/env.ps1` may supply a local toolchain when CMake is absent; it is optional and is not distributed. The helper performs no downloads or system installation.

The equivalent ordinary commands work without PowerShell:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --config Debug --parallel
ctest --test-dir build/debug -C Debug --output-on-failure --no-tests=error

cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --config Release --parallel
ctest --test-dir build/release -C Release --output-on-failure --no-tests=error

cmake -S . -B build/sanitized -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DMXS_REGANVIL_ENABLE_SANITIZERS=ON
cmake --build build/sanitized --config Debug --parallel
ctest --test-dir build/sanitized -C Debug --output-on-failure --no-tests=error

cmake -S . -B build/library -DCMAKE_BUILD_TYPE=Release -DMXS_REGANVIL_BUILD_TESTS=OFF -DMXS_REGANVIL_BUILD_EXAMPLES=OFF
cmake --build build/library --config Release --parallel
```

For a manual sanitizer run, set `ASAN_OPTIONS=halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` in the shell. The verification helper sets and restores them for its instrumented run. Sanitizer configuration requires a supported GCC/Clang driver and matching runtimes; do not treat a skipped configuration as passed.

## What the default suite checks

| CTest entry | Coverage |
| --- | --- |
| `reganvil.behavior` | 166,355 always-active checks, including 6,144 deterministic randomized operations against an independent per-bit model. |
| `reganvil.demo` | Detect the deliberate W1C acknowledgement bug and verify its correction. Detecting that bug is a successful result. |
| `reganvil.driver_integration` | 2,349 checks of an original callback driver using documented RP2040 GPIO0–7 register definitions. [Fixture documentation](tests/driver/README.md) gives sources and modeled limits. |
| `reganvil.public_consumer` | Compile and run a separate public-header consumer. |
| `reganvil.source_consumer` | Configure, build, and run a standalone consumer using `add_subdirectory`; verify subproject defaults and clean target interfaces. |
| `reganvil.installed_consumer` | Install into a unique local prefix, relocate it, inspect exported metadata for original paths, then build and run a `find_package` consumer. |

Release checks use explicit conditions rather than `assert`, so `NDEBUG` does not disable them. Randomized seeds are `0x5eed1234`, `0x91c4a7d3`, and `0x743eb821`, with 2,048 operations each and trace capacities four, one, and zero. Failure diagnostics report the seed and operation index.

Consumer tests run beneath the build directory and do not install to system locations. Each build retains `Testing/Temporary/LastTest.log` and its `consumer-checks/` work.

## Local release verification: 2026-09-10

These results were refreshed against the final 0.1.0 source and build files after the documentation and build-hardening pass. The core retained its behavior and generated code; comments now explain the important invariants. Coverage, deliberate-fault, portability, minimum-CMake, and build-option checks were rerun after those edits.

### Environment

| Component | Version used |
| --- | --- |
| Host | Windows x64, Windows build 26200 |
| Compiler | Clang 23.1.1, UCRT; x86_64 and i686 Windows targets |
| Distribution | [LLVM-MinGW 20260908](https://github.com/mstorsjo/llvm-mingw/releases/tag/20260908) |
| CMake | [4.4.3](https://github.com/Kitware/CMake/releases/tag/v4.4.3) and minimum supported [3.20.0](https://github.com/Kitware/CMake/releases/tag/v3.20.0) |
| Runner | [Ninja 1.13.2](https://github.com/ninja-build/ninja/releases/tag/v1.13.2) |
| Verification shells | PowerShell 7.6 and Windows PowerShell 5.1.26100.9444 |

Portable tools were obtained from official upstream releases, checked against published SHA-256 values, and extracted to ignored `.build/tools/` directories. No system installation was performed. These local tool directories are not part of the source package.

### Results actually executed

| Check | Result |
| --- | --- |
| Fresh Debug configure/build/CTest | 6/6 passed |
| Fresh Release configure/build/CTest | 6/6 passed |
| Fresh ASan + UBSan configure/build/CTest | 6/6 passed; no sanitizer findings |
| Library-only Release | Passed with tests and examples disabled |
| Full verifier in PowerShell 7.6 and Windows PowerShell 5.1 | All four requested configurations passed in each shell |
| Direct-source demonstration and final README example | Strict C99 with `-O2 -DNDEBUG`; both ran successfully |
| Windows x86_64 and i686 executables | Both compiled and executed the behavior suite, RP2040 fixture, demonstration, C consumer, and C++11 header consumer |
| Minimum CMake 3.20.0 | Release 6/6 passed |
| Ninja Multi-Config and a build path with spaces | Debug 6/6 and Release 6/6 passed |
| Tests enabled, examples disabled | 5/5 passed |
| `BUILD_TESTING=OFF` | Zero tests registered; library and demonstration built; demonstration ran |
| Unforced CMake subproject defaults | Tests/examples stayed off; consumer passed; warning and sanitizer interface options did not leak |
| Clang static analyzer | No core diagnostics |
| Freestanding core compile | `-ffreestanding -fno-builtin` succeeded with no unresolved symbols |
| Core LLVM coverage | 195/195 executable lines, 100/100 branch outcomes, 156/156 regions, and 13/13 functions exercised |
| Deliberately faulty core variants | All 12 compiled cleanly and failed test conditions; the original passed |

All three README Mermaid diagrams were parsed and rendered with Mermaid 12.0.0 in headless Chrome, then visually inspected. The exact final README C example compiled under strict C99 and printed `Pending flags: 0x02`.

Release behavior-test flags included `-O3 -DNDEBUG -std=c99`. Direct x86_64/i686 checks used `-pedantic-errors -Wall -Wextra -Wconversion -Wshadow -Werror -O2 -DNDEBUG`; the C++ consumer used C++11. Compiler targets and PE headers verified the two Windows architectures, and both were executed. Those local warning-as-error flags are not exported to consumers.

The PowerShell command wrapper was also exercised with a command writing stderr while exiting zero and a command exiting seven. It accepted the successful command, reported the nonzero exit code, and restored its error preference. This checks the Windows PowerShell 5.1 native-stderr handling in the helper.

### Supplemental evidence and limits

Coverage and deliberate-fault experiments were supplemental local checks. Their original scripts and logs are ignored workspace products, not dependencies shipped with the library. The default CMake/CTest and PowerShell commands above are the repeatable checks supplied to every checkout.

Coverage measured `src/mxs_reganvil.c`; it did not measure all helper or test code. The 12 faulty variants changed individual core rules and failed through test conditions, rather than compilation errors or process crashes. These finite experiments show that the tests caught those mistakes; they do not establish that every possible defect is detectable. No required core change or library defect was found during the review. A benchmarked read-path candidate did not consistently improve performance and was rejected; no runtime speedup is claimed.

Retained local evidence, when working in the original verification workspace:

| Evidence | Ignored workspace location |
| --- | --- |
| Verifier result, logs, and JSON summary | `.build/verification/latest.json` and the named run directory |
| Windows PowerShell 5.1 verifier | `.build/logs/verifier-powershell51.log` |
| CMake 3.20.0 | `.build/logs/cmake-3.20.0.log` |
| Both Windows architectures, analyzer, and freestanding object | `.build/logs/portability.log`, `.build/portability/` |
| Multi-config, option, and subproject matrix | `.build/logs/matrix.log`, `.build/matrix/` |
| LLVM coverage | `.build/quality/coverage-summary.txt`, `coverage.txt`, and `coverage.json` |
| Compiled faulty-variant results | `.build/quality/mutation-results.json` and adjacent logs |
| Final minimum-CMake and build-option runs | `.build/logs/final-cmake-3.20.0.log`, `.build/logs/final-matrix.log` |
| README diagram rendering | `.build/docs/mermaid-results.json` and three adjacent PNGs |
| Direct RP2040 driver fixture | `.build/logs/driver-integration-direct.log` |

These ignored paths are a record of local evidence locations; they are not links to files distributed in a checkout or release archive.

## Hosted and hardware verification

The [GitHub Actions workflow](.github/workflows/ci.yml) defines Linux GCC/Clang, macOS Apple Clang, and Windows MSVC jobs in Debug and Release, plus Linux Clang ASan/UBSan. Consult the workflow results for the exact commit being evaluated. The local results above do not claim a hosted run, native Linux/macOS/MSVC execution, or LeakSanitizer coverage.

ARM targets and physical hardware are outside the recorded local execution evidence. The RP2040 fixture checks a datasheet-defined register subset; it does not simulate pad signals, processor interrupt routing, simultaneous hardware events, or an entire chip. The model also does not intercept arbitrary volatile accesses or simulate a bus/CPU or electrical timing. Calls on a single model remain single-threaded and non-reentrant.
