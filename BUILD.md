BUILD
=====

This document explains how to build and test this project on Windows and POSIX systems, plus CI notes.

Prerequisites
-------------
- Windows
  - Visual Studio 2022 (recommended) or 2019 with C++ Desktop workload (MSVC + CMake tools)
  - CMake 3.20+ (installed and on PATH)
  - Optional: build2 (if you prefer the build2 workflow in some scripts)
  - Optional: `vswhere` (installed with Visual Studio or available at `C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe`)
- POSIX (Linux/macOS)
  - CMake 3.20+
  - A C++ toolchain (gcc/clang)
  - make or ninja

Build (Windows - recommended using CMake)
----------------------------------------
1. Open a Developer Command Prompt or use `vswhere`/vcvars to set environment variables.
2. From the repo root:
   - `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release`
   - `cmake --build build --config Release -- /m`
3. To run tests:
   - `cmake --build build --config Release --target RUN_TESTS`

Alternative: helper batch script
--------------------------------
- `scripts\sc-compile.bat` attempts to locate Visual Studio and invoke `vcvars64.bat` then run a build. It uses `vswhere` if available. If you have issues, run the CMake commands above manually.

Build (POSIX)
--------------
1. From the repo root:
   - `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
   - `cmake --build build`
2. Run tests:
   - `ctest --test-dir build --output-on-failure`

Testing
-------
- The project uses Catch2 for unit tests. The tests live in `tests/` and are driven by CMake targets.
- If your environment is offline, consider vendorizing the single-header Catch2 in `tests/vendor/catch2/`.

CI Notes
--------
- The GitHub Actions workflows in `.github/workflows/` provide Linux and Windows build/test jobs. The Windows job uses CMake with MSVC to avoid relying on `build2` on runners.
- Ensure runners have CMake and a matching MSVC toolchain; GitHub-hosted runners with `windows-2022` include these.

Troubleshooting
---------------
- "vcvars64.bat not found": ensure Visual Studio with C++ workload is installed. `vswhere.exe` can locate installations.
- "Catch2 not found": either allow CMake to fetch Catch2 or copy `catch.hpp` into `tests/vendor/catch2/`.
- If the build fails due to missing components, install required workloads via Visual Studio Installer.

Further improvements
--------------------
- Vendorize Catch2 single-header to make tests deterministic in CI.
- Add an artifact upload step in CI to produce binary releases.

Contact
-------
If you want me to vendorize Catch2 or modify the CI workflow further, tell me and I'll proceed.
