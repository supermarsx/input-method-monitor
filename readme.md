# Input Method Monitor

## Overview
Input Method Monitor is a small Windows utility that keeps your default input method in sync with the currently active keyboard layout. It installs a shell hook through a companion DLL and can display a tray icon for quick actions.

## Features
- Monitors keyboard layout changes and updates registry settings for the default input method. Registry writes are handled on a background thread.
- Optional system tray icon with options to:
  - Launch the application at system startup.
  - Toggle Windows "Language" and "Layout" hotkeys.
  - Temporarily enable both hotkeys for a short period.
  - Open the debug log file when logging is enabled.
  - Open the configuration file for editing.
  - Toggle debug logging on or off at runtime.
  - Restart or exit the application.
- Optional debug logging to `kbdlayoutmon.log`.

## Configuration
Configuration is read from `kbdlayoutmon.config` located next to the executable. The DLL loads this file during `DLL_PROCESS_ATTACH` so both modules share the same settings. By default the file should sit in the same folder as `kbdlayoutmon.exe` (for example `dist\kbdlayoutmon.config` when built with the provided scripts). Supported options are:

```
DEBUG=1       # Enable debug logging (0 to disable)
TRAY_ICON=1   # Show the tray icon (0 to run without it)
STARTUP=0     # Launch the application at system startup
LANGUAGE_HOTKEY=1   # Enable the Windows "Language" hotkey
LAYOUT_HOTKEY=1     # Enable the Windows "Layout" hotkey
TEMP_HOTKEY_TIMEOUT=10000 # Milliseconds for temporary hotkeys to remain enabled
LOG_PATH=path\to\logfile # Optional custom log file location
MAX_LOG_SIZE_MB=10 # Rotate log when it exceeds this size in megabytes
MAX_QUEUE_SIZE=1000 # Maximum number of log messages buffered before dropping oldest
```

Lines that begin with `#` or `;` (after trimming whitespace) are treated as comments and ignored.

Changes to `kbdlayoutmon.config` are picked up automatically while the program is running.
Debug logging can also be toggled on or off at runtime from the tray icon menu.
You can specify an alternate configuration file on startup using `--config <path>`.

## Command Line Options
The executable also accepts a few optional flags which override settings in the
configuration file. Use `--help` to display a summary at runtime:

```
--config <path>           Load configuration from the specified file
--no-tray                 Run without the system tray icon
--debug                   Enable debug logging
--verbose                 Increase logging verbosity
--cli                     Run in CLI mode without GUI/tray icon
--tray-icon <0|1>         Override tray icon setting
--temp-hotkey-timeout <ms>  Override temporary hotkey timeout
--log-path <path>         Override log file location
--max-log-size-mb <num>   Override maximum log size
--max-queue-size <num>    Override maximum queued log messages
--version                 Print the application version and exit
--help                    Show this help text
```

## Build

### Installing Catch2
Unit tests depend on [Catch2](https://github.com/catchorg/Catch2). Install it before attempting to build or run tests.

On Debian/Ubuntu the packaged version can be installed with:

```bash
sudo apt install catch2
```

If your distribution does not provide it, build Catch2 from source following the instructions in its repository.

After installation the tests can be executed with:

```bash
scripts/run_tests.sh
```

The script checks for the presence of Catch2 headers and will abort with a helpful error message if they are missing.
### Using CMake
The project can be built with CMake 3.15 or newer. From a *Developer Command Prompt* run:

```batch
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
cmake --build . --target run_tests
# or simply
# ctest
```

`kbdlayoutmon.exe` and `kbdlayoutmonhook.dll` will be produced in `build/Release` with resources and the application manifest embedded.

### Using build2
The repository also ships with a basic [build2](https://build2.org/) setup. After installing
the build2 toolchain run:

```bash
b configure
b
b test
```

Executables will appear in the current directory when the build completes.

### Batch Script
Visual Studio with C++ tools is also sufficient. You can run:

```batch
scripts\sc-compile.bat
```

This script drops the resulting binaries into the `dist` folder.

## Batch Scripts
Several helper scripts are provided in the `scripts` directory:

- `sc-compile.bat` – build the project.
- `sc-killapp.bat` – terminate a running instance of `kbdlayoutmon.exe`.
- `sc-del-dist.bat` – remove the `dist` directory.
- `sc-del-etc.bat` – clean up temporary build files.
- `sc-taildebug.bat` – tail the log file inside `dist`.
- `sc-taildebug-cur.bat` – tail a log file in the current directory.

Run these scripts from a Windows command prompt.

## Runtime
The application targets Windows XP and later (manifest includes modern versions) and depends on the standard Visual C++ runtime libraries. PowerShell is required for the log tail scripts.

## License
This project is released under the MIT License. See `license.md` for details.
