#!/bin/sh
set -e

# Try to compile a tiny include to detect Catch2. If missing, attempt to download
# the single-header Catch2 v3.4.0 into tests/vendor/catch2/catch.hpp and create
# a small catch_main.cpp to provide the test runner.
VENDOR_DIR="tests/vendor"
CATCH_HEADER="$VENDOR_DIR/catch2/catch_amalgamated.hpp"

check_catch() {
    echo '#include <catch2/catch_test_macros.hpp>' | g++ -std=c++17 -x c++ - -fsyntax-only >/dev/null 2>&1
}

if ! check_catch; then
    echo "Catch2 not found. Attempting to download Catch2 v3.4.0 amalgamated header to $CATCH_HEADER..." >&2
    mkdir -p "$VENDOR_DIR/catch2"
    URL="https://raw.githubusercontent.com/catchorg/Catch2/v3.4.0/extras/catch_amalgamated.hpp"
    if command -v curl >/dev/null 2>&1; then
        curl -L -s -o "$CATCH_HEADER" "$URL" || { echo "curl failed" >&2; rm -f "$CATCH_HEADER"; }
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$CATCH_HEADER" "$URL" || { echo "wget failed" >&2; rm -f "$CATCH_HEADER"; }
    elif command -v git >/dev/null 2>&1; then
        tmpdir=$(mktemp -d)
        git clone --depth 1 --branch v3.4.0 https://github.com/catchorg/Catch2.git "$tmpdir" >/dev/null 2>&1 || true
        if [ -f "$tmpdir/single_include/catch2/catch.hpp" ]; then
            cp "$tmpdir/single_include/catch2/catch.hpp" "$CATCH_HEADER"
        fi
        rm -rf "$tmpdir"
    else
        echo "No download tool (curl/wget/git) available to fetch Catch2." >&2
    fi

    if ! check_catch; then
        echo "Failed to make Catch2 available. Install Catch2 or ensure network access." >&2
        exit 1
    fi
fi

# Ensure a catch main exists when using the amalgamated header
if [ ! -f tests/catch_main.cpp ]; then
    echo "Creating tests/catch_main.cpp..."
    cat > tests/catch_main.cpp <<'EOF'
#include "catch2/catch_amalgamated.hpp"

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}
EOF
fi

# Compile tests using either system Catch2 (linked) or the downloaded single-header
# If system Catch2 headers/libs are present, the original link flags will work. The
# single-header path provides tests/catch_main.cpp which defines main, so we don't
# need to link Catch2 libraries.

# Prefer using installed Catch2 (link) if available, otherwise build against the
# single-header in tests/vendor.
if echo '#include <catch2/catch_test_macros.hpp>' | g++ -std=c++17 -x c++ - -fsyntax-only >/dev/null 2>&1; then
    # System Catch2 present: mirror original build (linking to Catch2 libs)
    g++ -std=c++17 -DUNIT_TEST -I tests -I resources \
        tests/test_configuration.cpp tests/test_log.cpp tests/test_utils.cpp tests/test_timer_guard.cpp \
        tests/test_tray_icon.cpp tests/test_tray_icon_integration.cpp tests/test_tray_icon_update.cpp \
        tests/test_hotkey_registry.cpp tests/test_unknown_option.cpp tests/test_config_watcher_posix.cpp tests/stubs.cpp \
        source/configuration.cpp source/log.cpp source/config_parser.cpp source/tray_icon.cpp \
        source/hotkey_registry.cpp source/hotkey_cli.cpp source/config_watcher.cpp source/config_watcher_posix.cpp source/cli_utils.cpp \
        source/app_state.cpp \
        -o tests/run_tests \
        -lCatch2Main -lCatch2 -pthread
else
    # Use the downloaded single-header and catch_main.cpp
    g++ -std=c++17 -DUNIT_TEST -I tests -I resources -I "$VENDOR_DIR" \
        tests/catch_main.cpp \
        tests/vendor/catch2/catch_amalgamated.cpp \
        tests/test_configuration.cpp tests/test_log.cpp tests/test_utils.cpp tests/test_timer_guard.cpp \
        tests/test_tray_icon.cpp tests/test_tray_icon_integration.cpp tests/test_tray_icon_update.cpp \
        tests/test_hotkey_registry.cpp tests/test_unknown_option.cpp tests/test_config_watcher_posix.cpp tests/stubs.cpp \
        source/configuration.cpp source/log.cpp source/config_parser.cpp source/tray_icon.cpp \
        source/hotkey_registry.cpp source/hotkey_cli.cpp source/config_watcher.cpp source/config_watcher_posix.cpp source/cli_utils.cpp \
        source/app_state.cpp \
        -o tests/run_tests -pthread
fi

./tests/run_tests "$@"
