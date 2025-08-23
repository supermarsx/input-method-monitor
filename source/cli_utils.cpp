#include "cli_utils.h"
#include "log.h"
#include <sstream>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif

extern bool g_cliMode;

std::wstring GetUsageString() {
    return
        L"Usage: kbdlayoutmon [options]\n\n"
        L"Options:\n"
        L"  --config <path>  Load configuration from the specified file\n"
        L"  --no-tray    Run without the system tray icon\n"
        L"  --debug      Enable debug logging\n"
        L"  --verbose    Increase logging verbosity\n"
        L"  --cli        Run in CLI mode without GUI/tray icon\n"
        L"  --tray-icon <0|1>        Override tray icon setting\n"
        L"  --temp-hotkey-timeout <ms>  Override temporary hotkey timeout\n"
        L"  --log-path <path>          Override log file location\n"
        L"  --icon-path <path>         Override tray icon image\n"
        L"  --tray-tooltip <text>      Override tray icon tooltip\n"
        L"  --max-log-size-mb <num>    Override max log size\n"
        L"  --max-queue-size <num>     Override log queue length\n"
        L"  --enable-startup           Add application to user startup\n"
        L"  --disable-startup          Remove application from user startup\n"
        L"  --enable-language-hotkey   Enable the Windows \"Language\" hotkey\n"
        L"  --disable-language-hotkey  Disable the Windows \"Language\" hotkey\n"
        L"  --enable-layout-hotkey     Enable the Windows \"Layout\" hotkey\n"
        L"  --disable-layout-hotkey    Disable the Windows \"Layout\" hotkey\n"
        L"  --version    Print the application version and exit\n"
        L"  --status     Print startup and hotkey states and exit\n"
        L"  --help       Show this help message and exit";
}

void WarnUnrecognizedOption(const wchar_t* option) {
    std::wstringstream ss;
    ss << L"Unrecognized option: " << option;
    WriteLog(LogLevel::Error, ss.str());
    ss << L"\n\n" << GetUsageString();
#ifdef _WIN32
    if (g_cliMode || AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* fp = _wfopen(L"CONOUT$", L"w");
        if (fp) {
            fwprintf(fp, L"%s\n", ss.str().c_str());
            fclose(fp);
        }
        if (!g_cliMode)
            FreeConsole();
    } else {
        MessageBox(NULL, ss.str().c_str(), L"Input Method Monitor", MB_OK | MB_ICONEXCLAMATION);
    }
#else
    if (g_cliMode) {
        fwprintf(stdout, L"%s\n", ss.str().c_str());
    }
#endif
}
