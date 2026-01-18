# TODO

## Failing tests (blocking)
- Please add the missing error log in the TemporarilyEnableHotKeys RegSetValueEx failure path (see `tests/test_hotkey_registry.cpp:80`; implementation in `tests/test_hotkey_registry_impl.cpp`).
- Please make the Windows config watcher tests actually trigger apply/sleep counts (see `tests/test_config_watcher.cpp:35` and `tests/test_config_watcher.cpp:48`; current `tests/test_config_watcher_impl.cpp` is a no-op).
- Please fix log rotation/backups in the unit-test path so `.1`/`.2` files are created (see `tests/test_log.cpp:69` and `tests/test_log.cpp:100`; logic in `source/log.cpp`).

## Flaky or intermittent
- Please stabilize the pipe listener test that sometimes fails to open the named pipe (see `tests/test_log.cpp:202`; failure in `last_run.txt`).

## Test coverage gaps (not wired into CMake)
- Please add these tests to `CMakeLists.txt` `TEST_SOURCES` so they run in CMake/CI:
  - `tests/test_tray_icon.cpp`
  - `tests/test_tray_icon_update.cpp`
  - `tests/test_tray_icon_integration.cpp`
  - `tests/test_unknown_option.cpp`
  - `tests/test_timer_guard.cpp`

## CI/build improvements called out in docs
- Please vendorize the Catch2 single-header for deterministic/offline CI builds (noted in `BUILD.md`).
- Please add an artifact upload step in CI for binary releases (noted in `BUILD.md`).

## Test invocation cleanup
- Please update any ad-hoc test filters/scripts so they match current test names (see `temp_test_output.txt` showing no matches).
