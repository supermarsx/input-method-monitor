# Input Method Monitor

## Overview
Input Method Monitor is a small Windows utility that keeps your default input method in sync with the currently active keyboard layout. It installs a shell hook through a companion DLL and can display a tray icon for quick actions.

## Features
- Monitors keyboard layout changes and updates registry settings for the default input method. Registry writes are handled on a background thread.
- Optional system tray icon with options to:
  - Launch the application at system startup.
  - Toggle Windows "Language" and "Layout" hotkeys.
  - Temporarily enable both hotkeys for a short period.
  - Restart or exit the application.
- Optional debug logging to `kbdlayoutmon.log`.

## Configuration
Configuration is read from `kbdlayoutmon.config` located next to the executable. Supported options are:

```
DEBUG=1       # Enable debug logging (0 to disable)
TRAY_ICON=1   # Show the tray icon (0 to run without it)
TEMP_HOTKEY_TIMEOUT=10000 # Milliseconds for temporary hotkeys to remain enabled
LOG_PATH=path\to\logfile # Optional custom log file location
```

## Command Line Options
The executable also accepts a few optional flags which override settings in the
configuration file:

```
--no-tray    Run without the system tray icon
--debug      Enable debug logging
```

## Build
Visual Studio with C++ tools is required. Open a **Developer Command Prompt** for Visual Studio 2022 (or adjust the path in the script) and run:

```
scripts\sc-compile.bat
```

The script compiles `kbdlayoutmon.exe` and `kbdlayoutmonhook.dll` and places the results in the `dist` folder.

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
