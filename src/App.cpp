#include "App.h"

#include <commctrl.h>

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
}

int App::Run(HINSTANCE instance, int showCommand)
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!IsElevated()) {
        MessageBoxW(nullptr, L"当前进程未取得管理员权限，快捷控制台无法继续。", L"快捷控制台", MB_OK | MB_ICONERROR);
        if (SUCCEEDED(comResult)) CoUninitialize();
        return 1;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    MainWindow window;
    if (!window.Create(instance) || !window.Initialize()) {
        MessageBoxW(nullptr, L"快捷控制台主窗口初始化失败。", L"快捷控制台", MB_OK | MB_ICONERROR);
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
    if (SUCCEEDED(comResult)) CoUninitialize();
    return result;
}
