#include "App.h"

#include "Utf.h"

#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <optional>

namespace {
bool IsElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool result = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) &&
                        elevation.TokenIsElevated != 0;
    CloseHandle(token);
    return result;
}

struct StartupRequest
{
    std::optional<TerminalKind> terminal;
    std::wstring command;
};

StartupRequest ParseStartupRequest()
{
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments == nullptr) return {};
    StartupRequest request;
    for (int index = 1; index + 1 < argumentCount; ++index) {
        if (std::wstring_view(arguments[index]) == L"--elevated-command") {
            request.command = Utf8ToWide(Base64Decode(WideToUtf8(arguments[index + 1])));
            ++index;
        } else if (std::wstring_view(arguments[index]) == L"--elevated-terminal") {
            TerminalKind kind = TerminalKind::PowerShell;
            if (ParseTerminalKind(WideToUtf8(arguments[index + 1]), kind)) request.terminal = kind;
            ++index;
        }
    }
    LocalFree(arguments);
    return request;
}
}

int App::Run(HINSTANCE instance, int showCommand)
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    const bool bufferedPaintInitialized = SUCCEEDED(BufferedPaintInit());
    StartupRequest startup = ParseStartupRequest();
    MainWindow window{IsElevated(), startup.terminal, std::move(startup.command)};
    if (!window.Create(instance) || !window.Initialize()) {
        MessageBoxW(nullptr, L"快捷控制台主窗口初始化失败。", L"快捷控制台", MB_OK | MB_ICONERROR);
        if (bufferedPaintInitialized) BufferedPaintUnInit();
        if (SUCCEEDED(comResult)) CoUninitialize();
        return 1;
    }
    ShowWindow(window.Hwnd(), showCommand);
    UpdateWindow(window.Hwnd());
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    const int result = static_cast<int>(message.wParam);
    if (bufferedPaintInitialized) BufferedPaintUnInit();
    if (SUCCEEDED(comResult)) CoUninitialize();
    return result;
}
