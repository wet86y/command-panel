#include "LocalUpdateDiagnostics.h"

#include <windows.h>

#include <filesystem>
#include <string>

namespace {
#if COMMAND_PANEL_LOCAL_UPDATE_DIAGNOSTICS
std::filesystem::path DiagnosticLogPath()
{
    wchar_t localAppData[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, static_cast<DWORD>(std::size(localAppData)));
    if (length == 0 || length >= std::size(localAppData)) return {};
    return std::filesystem::path(localAppData) / L"快捷控制台" / L"logs" / L"update-handoff.log";
}
#endif
}

void RecordLocalUpdateDiagnostic(std::wstring_view message) noexcept
{
#if COMMAND_PANEL_LOCAL_UPDATE_DIAGNOSTICS
    try {
        const auto path = DiagnosticLogPath();
        if (path.empty()) return;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return;
        const std::wstring record = L"[" + std::to_wstring(GetTickCount64()) + L"] " + std::wstring(message) + L"\r\n";
        HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        DWORD written{};
        WriteFile(file, record.data(), static_cast<DWORD>(record.size() * sizeof(wchar_t)), &written, nullptr);
        FlushFileBuffers(file);
        CloseHandle(file);
        OutputDebugStringW(record.c_str());
    } catch (...) {
    }
#else
    (void)message;
#endif
}

bool LocalUpdateDiagnosticsEnabled() noexcept
{
#if COMMAND_PANEL_LOCAL_UPDATE_DIAGNOSTICS
    return true;
#else
    return false;
#endif
}
