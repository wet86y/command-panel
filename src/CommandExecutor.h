#pragma once

#include "TerminalSession.h"

#include <atomic>
#include <optional>
#include <string>
#include <string_view>

struct CommandOutputResult
{
    std::wstring display;
    std::optional<int> exitCode;
};

class CommandExecutor
{
public:
    explicit CommandExecutor(TerminalSession& session) : session_(session) {}

    bool ExecuteManaged(const std::wstring& command);
    bool SendInteractiveInput(const std::wstring& input);
    CommandOutputResult ConsumeOutput(std::wstring_view text);
    void Reset();
    bool IsBusy() const { return busy_.load(); }

private:
    TerminalSession& session_;
    std::string currentExecutionId_;
    std::atomic_bool busy_ = false;
    std::wstring markerBuffer_;
};
