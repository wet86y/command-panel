#include "App.h"

#include "ExecutableNameNormalizer.h"
#include "LocalUpdateDiagnostics.h"
#include "Utf.h"
#include "Version.h"

#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <thread>

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
    std::optional<std::wstring> updateHealthMarker;
    bool verifyRelease{};
};

bool WriteUpdateHealthMarker(const std::wstring& marker)
{
    try {
        const std::filesystem::path path(marker);
        if (!path.is_absolute() || path.parent_path().empty()) return false;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
        const std::wstring temporary = path.wstring() + L".tmp." + std::to_wstring(GetCurrentProcessId());
        HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        const std::string text = WideToUtf8(kCommandPanelVersion);
        DWORD written{};
        const bool wrote = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
                           written == text.size() && FlushFileBuffers(file);
        CloseHandle(file);
        if (!wrote || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(temporary.c_str());
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

// The update Stub owns its transaction directory.  A process started with an
// update-health marker must acknowledge the outer update before it starts the
// independent name-normalisation Stub, otherwise both helpers can race on the
// same executable path.
bool WaitForOuterUpdateTransaction(const std::wstring& marker, std::stop_token stopToken)
{
    const std::filesystem::path path(marker);
    const ULONGLONG deadline = GetTickCount64() + 35000;
    for (;;) {
        if (stopToken.stop_requested()) return false;
        std::error_code error;
        const bool present = std::filesystem::exists(path, error);
        if (!present && !error) return true;
        if (GetTickCount64() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}

bool VerifyReleaseBundle(HINSTANCE instance)
{
    HRSRC version = FindResourceW(instance, MAKEINTRESOURCEW(1), RT_VERSION);
    HRSRC stub = FindResourceW(instance, MAKEINTRESOURCEW(201), RT_RCDATA);
    if (LocalUpdateDiagnosticsEnabled() || version == nullptr || stub == nullptr || SizeofResource(instance, stub) < 2) return false;
    HGLOBAL loaded = LoadResource(instance, stub);
    const auto* bytes = loaded != nullptr ? static_cast<const unsigned char*>(LockResource(loaded)) : nullptr;
    return bytes != nullptr && bytes[0] == 'M' && bytes[1] == 'Z';
}

std::filesystem::path CurrentExecutablePath()
{
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return {};
        if (length + 1 < path.size()) {
            path.resize(length);
            return std::filesystem::path(path);
        }
        path.resize(path.size() * 2);
    }
}

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
        } else if (std::wstring_view(arguments[index]) == L"--update-health") {
            request.updateHealthMarker = arguments[index + 1];
            ++index;
        }
    }
    for (int index = 1; index < argumentCount; ++index) {
        if (std::wstring_view(arguments[index]) == L"--verify-release") request.verifyRelease = true;
    }
    LocalFree(arguments);
    return request;
}
}

int App::Run(HINSTANCE instance, int showCommand)
{
    const StartupRequest startup = ParseStartupRequest();
    if (startup.verifyRelease) return VerifyReleaseBundle(instance) ? 0 : 10;
    RecordLocalUpdateDiagnostic(L"startup path=" + CurrentExecutablePath().wstring() +
                                L" update-health=" + (startup.updateHealthMarker ? L"present" : L"absent"));
    const auto normalizeExecutableName = [instance] {
        std::wstring error;
        const auto result = NormalizeExecutableName(instance, CurrentExecutablePath(), L"快捷控制台.exe", error);
        if (result == ExecutableNameNormalizationResult::Failed)
            RecordLocalUpdateDiagnostic(L"rename hand-off failed error=" + error);
        return result;
    };
    // A release asset launched normally can rename before window creation.  An
    // update health launch must first acknowledge the freshly installed binary;
    // otherwise the outer updater would mistake this hand-off for a failed update.
    if (!startup.updateHealthMarker && normalizeExecutableName() == ExecutableNameNormalizationResult::RelaunchStarted) return 0;
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    const bool bufferedPaintInitialized = SUCCEEDED(BufferedPaintInit());
    MainWindow window{IsElevated(), startup.terminal, std::move(startup.command)};
    if (!window.Create(instance) || !window.Initialize()) {
        MessageBoxW(nullptr, L"快捷控制台主窗口初始化失败。", L"快捷控制台", MB_OK | MB_ICONERROR);
        if (bufferedPaintInitialized) BufferedPaintUnInit();
        if (SUCCEEDED(comResult)) CoUninitialize();
        return 1;
    }
    if (startup.updateHealthMarker && !WriteUpdateHealthMarker(*startup.updateHealthMarker)) {
        RecordLocalUpdateDiagnostic(L"update health marker failed path=" + *startup.updateHealthMarker);
        MessageBoxW(window.Hwnd(), L"更新后的启动健康标记写入失败，更新助手将回滚到上一版本。",
                    L"快捷控制台", MB_OK | MB_ICONERROR);
        if (bufferedPaintInitialized) BufferedPaintUnInit();
        if (SUCCEEDED(comResult)) CoUninitialize();
        return 2;
    }
    ShowWindow(window.Hwnd(), showCommand);
    UpdateWindow(window.Hwnd());
    std::jthread updateRenameThread;
    if (startup.updateHealthMarker) {
        const std::wstring marker = *startup.updateHealthMarker;
        const HWND updateWindow = window.Hwnd();
        RecordLocalUpdateDiagnostic(L"update health marker written path=" + marker);
        updateRenameThread = std::jthread([instance, marker, updateWindow](std::stop_token stopToken) {
            if (!WaitForOuterUpdateTransaction(marker, stopToken)) {
                if (!stopToken.stop_requested())
                    RecordLocalUpdateDiagnostic(L"update hand-off wait timed out; deferring rename to the next launch");
                return;
            }
            RecordLocalUpdateDiagnostic(L"outer update transaction completed; starting rename hand-off");
            std::wstring error;
            const auto result = NormalizeExecutableName(instance, CurrentExecutablePath(), L"快捷控制台.exe", error);
            if (result == ExecutableNameNormalizationResult::RelaunchStarted) {
                RecordLocalUpdateDiagnostic(L"rename hand-off started after update transaction completed");
                if (IsWindow(updateWindow)) PostMessageW(updateWindow, WM_CLOSE, 0, 0);
            } else if (result == ExecutableNameNormalizationResult::Failed) {
                RecordLocalUpdateDiagnostic(L"rename hand-off failed after update transaction error=" + error);
            }
        });
    }
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    const int result = static_cast<int>(message.wParam);
    if (updateRenameThread.joinable()) {
        updateRenameThread.request_stop();
        updateRenameThread.join();
    }
    if (bufferedPaintInitialized) BufferedPaintUnInit();
    if (SUCCEEDED(comResult)) CoUninitialize();
    return result;
}
