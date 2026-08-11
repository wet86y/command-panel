#pragma once

#include <string>
#include <string_view>

enum class TerminalKind
{
    PowerShell,
    Wsl,
};

inline constexpr std::string_view TerminalKindName(TerminalKind kind)
{
    return kind == TerminalKind::Wsl ? "wsl" : "powershell";
}

inline bool ParseTerminalKind(std::string_view value, TerminalKind& kind)
{
    if (value == "powershell") { kind = TerminalKind::PowerShell; return true; }
    if (value == "wsl") { kind = TerminalKind::Wsl; return true; }
    return false;
}

inline constexpr int TerminalIndex(TerminalKind kind)
{
    return kind == TerminalKind::Wsl ? 1 : 0;
}
