#pragma once

#include "ConfigManager.h"

#include <windows.h>

class CommandDialog
{
public:
    static bool Show(HWND owner, const CommandButton& initial, CommandButton& result);
    static bool PromptName(HWND owner, const std::wstring& title, const std::wstring& initial, std::wstring& result);
    static bool Confirm(HWND owner, const std::wstring& title, const std::wstring& message,
                        const std::wstring& primaryText = L"确认");
    static bool ConfirmCommand(HWND owner, const std::wstring& buttonName, const std::wstring& command);
};
