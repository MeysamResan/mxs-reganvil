# Package-manager integration

RegAnvil includes a **local vcpkg overlay** for the published **0.1.0** source release. The port builds the static library, installs the public header and MIT notice, and preserves the CMake target `mxs::reganvil`.

This is a repository-local overlay. It has not been submitted to or accepted into vcpkg's curated registry; provide the overlay path explicitly when installing it. Registry inclusion requires a separate manual submission and upstream review. See Microsoft's [overlay-port documentation](https://learn.microsoft.com/en-us/vcpkg/concepts/overlay-ports) and [port contribution process](https://learn.microsoft.com/en-us/vcpkg/contributing/maintainer-guide).

The curated-registry guide also calls for demonstrated project maturity (normally a release or public development history of at least six months, or equivalent evidence) and CI coverage on an official triplet. This new overlay and its community-triplet smoke test do not establish either criterion. The overlay is immediately usable without waiting for registry acceptance. [Current maintainer criteria](https://learn.microsoft.com/en-us/vcpkg/contributing/maintainer-guide#packaged-projects-should-be-mature).

## Files

| Path | Purpose |
| --- | --- |
| [`vcpkg/mxs-reganvil/vcpkg.json`](vcpkg/mxs-reganvil/vcpkg.json) | Port name, release version, MIT metadata, and host-only CMake helper dependencies. |
| [`vcpkg/mxs-reganvil/portfile.cmake`](vcpkg/mxs-reganvil/portfile.cmake) | Verified release download, static build, installation, and CMake-package cleanup. |
| [`vcpkg/mxs-reganvil/usage`](vcpkg/mxs-reganvil/usage) | Consumer instructions displayed after installation. |
| [`vcpkg/smoke/`](vcpkg/smoke/) | Manifest consumer using the repository's existing public-API behavioral check. |

## Use from a consuming project

Add `mxs-reganvil` to the consuming project's `vcpkg.json`:

```json
{
  "dependencies": [
    "mxs-reganvil"
  ]
}
```

Configure that project with the vcpkg CMake toolchain and an absolute overlay path. This example uses PowerShell and an existing vcpkg checkout; replace the two paths with your own:

```powershell
$reganvilVcpkg = 'C:/tools/vcpkg'
$reganvilOverlay = 'C:/source/mxs-reganvil/packaging/vcpkg/mxs-reganvil'
$env:VCPKG_DISABLE_METRICS = '1'
$env:VCPKG_BINARY_SOURCES = 'clear'

cmake -S . -B build/vcpkg `
  "-DCMAKE_TOOLCHAIN_FILE=$reganvilVcpkg/scripts/buildsystems/vcpkg.cmake" `
  "-DVCPKG_OVERLAY_PORTS=$reganvilOverlay" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build/vcpkg --config Release --parallel
```

Choose a triplet matching the consuming compiler and platform. Examples are `x64-windows` for Windows/MSVC, `x64-linux` for Linux, and `arm64-osx` for Apple Silicon. These are usage choices, not a claim that this overlay has been executed on every triplet. For MinGW, use `x64-mingw-static`, put that toolchain on `PATH`, and set `VCPKG_HOST_TRIPLET=x64-mingw-static` when no MSVC host toolchain is available.

The library itself is static even when the selected triplet normally uses dynamic libraries. The port declares this with [`vcpkg_check_linkage(ONLY_STATIC_LIBRARY)`](https://learn.microsoft.com/en-us/vcpkg/maintainers/functions/vcpkg_check_linkage); it does not add a DLL build.

Link the installed package in your CMake project:

```cmake
find_package(mxs_reganvil CONFIG REQUIRED)
target_link_libraries(your_driver_test PRIVATE mxs::reganvil)
```

Include `<mxs_reganvil.h>`. vcpkg merges the Debug and Release imports under `share/mxs_reganvil`; the port's MIT notice and usage file are under `share/mxs-reganvil`. The two CMake helper dependencies run during packaging and add no library runtime dependencies. [CMake config fixup](https://learn.microsoft.com/en-us/vcpkg/maintainers/functions/vcpkg_cmake_config_fixup) preserves the exported target while relocating its metadata.

## Repeat the overlay smoke test

From the RegAnvil repository root, using the variables above:

```powershell
cmake -S packaging/vcpkg/smoke -B .build/vcpkg-smoke `
  "-DCMAKE_TOOLCHAIN_FILE=$reganvilVcpkg/scripts/buildsystems/vcpkg.cmake" `
  "-DVCPKG_OVERLAY_PORTS=$reganvilOverlay" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build .build/vcpkg-smoke --config Debug --parallel
ctest --test-dir .build/vcpkg-smoke -C Debug --output-on-failure --no-tests=error
cmake --build .build/vcpkg-smoke --config Release --parallel
ctest --test-dir .build/vcpkg-smoke -C Release --output-on-failure --no-tests=error
```

These commands use a multi-configuration generator, such as Visual Studio, which is the Windows default when available. With Ninja or Makefiles, use a separate build directory and `-DCMAKE_BUILD_TYPE=Debug` or `Release` for each configuration; `--config` alone does not switch a single-configuration build.

The test resolves the installed package through `find_package`, links `mxs::reganvil`, and executes checks of W1C state preservation and trace metadata through the public API. Port builds disable RegAnvil's tests, examples, and sanitizers; the separate consumer verifies the resulting package. The library's full test matrix remains documented in [VERIFYING.md](../VERIFYING.md).

## Source pin and updates

The overlay always downloads the published [0.1.0 source ZIP](https://github.com/MeysamResan/mxs-reganvil/releases/download/v0.1.0/mxs-reganvil-0.1.0-source.zip). It does not package edits in the local working tree.

```text
SHA-256
ce396fe71755cf6da7da91765a42530c7505384d21a7a29df488a9057cb20437

SHA-512 used by the port
2760b0e76fb69b3d0a45e4e44097e1d3bd912da3a8392bd4df18e3ad495853f07826c4b8bbfdf11ea83ded473f4bec15330c67f82c1276f833cfd945f057f337
```

The release asset URL and SHA-512 pin the exact source bytes. vcpkg's [download helper](https://learn.microsoft.com/en-us/vcpkg/maintainers/functions/vcpkg_download_distfile) rejects a mismatched archive. CMake helper versions follow the consumer's vcpkg registry or checkout; pin that separately when reproducing a complete dependency environment.

After manually publishing the next source release:

1. Download its asset and verify the release checksum. Compute the SHA-512 with `Get-FileHash -Algorithm SHA512` or an equivalent hashing tool.
2. Update the version in the port manifest and the corresponding SHA-512 in `portfile.cmake`. The asset URL and filename derive from the manifest version. Update the smoke test's requested version when appropriate.
3. Update this source-pin record and rerun the package installation and consumer in fresh directories. Keep the published 0.1.0 asset unchanged.
4. Review and manually commit or submit the overlay changes. A recipe-only change for the same source version should increment the manifest's `port-version` according to [vcpkg's versioning rules](https://learn.microsoft.com/en-us/vcpkg/users/versioning).

## Executed verification

The local overlay was tested with vcpkg checkout `b773faa425fbb6804af8dcdaa6bd725409fc4c80`, tool version `2026-07-27`, CMake 4.4.3, Ninja 1.13.2, and LLVM-MinGW Clang 23.1.1 on Windows x64. The target and host triplets were both `x64-mingw-static`, using Ninja Multi-Config.

- The public release ZIP matched the recorded SHA-256 and SHA-512.
- `vcpkg format-manifest` accepted both manifests.
- The overlay built Debug and Release libraries and passed vcpkg's post-build validation.
- The installed-package consumer passed CTest in both Debug and Release.
- A copied installation passed the same tests in both configurations with only CMAKE_PREFIX_PATH, without the vcpkg toolchain.
- Installed metadata provided the expected static target and MIT copyright notice without retaining the source or build paths.

The vcpkg checkout, downloaded tools, installed package, and logs stayed under ignored `.build/packaging/`. Telemetry and binary-cache uploads were disabled. Native Linux, macOS, and MSVC overlay execution remain unverified; the successful Windows MinGW check does not establish curated-registry acceptance or support for every triplet.
