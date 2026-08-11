#include "CommandExecutor.h"

#include "ConfigManager.h"
#include "Utf.h"

#include <windows.h>

#include <algorithm>

namespace {
std::wstring NewExecutionId()
{
    return Utf8ToWide(ConfigManager::NewId());
}
}

bool CommandExecutor::ExecuteManaged(const std::wstring& command, bool elevateWsl)
{
    if (busy_ || command.empty() || !session_.IsRunning()) return false;
    const std::string utf8 = WideToUtf8(command);
    const std::string encoded = Base64Encode(utf8);
    if (encoded.empty() && !utf8.empty()) return false;
    currentExecutionId_ = ConfigManager::NewId();
    const std::wstring id = Utf8ToWide(currentExecutionId_);
    std::wstring wrapper;
    if (kind_ == TerminalKind::PowerShell) {
        wrapper =
            L"$__cp_command = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('" + Utf8ToWide(encoded) + L"')); "
            L"$__cp_exit = 0; try { Invoke-Expression $__cp_command; "
            L"$__cp_ok = $?; if ($null -ne $LASTEXITCODE) { $__cp_exit = [int]$LASTEXITCODE } "
            L"elseif (-not $__cp_ok) { $__cp_exit = 1 } } catch { Write-Error $_; $__cp_exit = 1 }; "
            L"Write-Output ('__COMMAND_PANEL_DONE__:' + '" + id + L"' + ':' + $__cp_exit); "
            L"Remove-Variable __cp_command, __cp_ok, __cp_exit -ErrorAction SilentlyContinue\r\n";
    } else {
        const std::wstring invocation = elevateWsl
            ? L"sudo bash -lc \"$__cp_command\""
            : L"eval \"$__cp_command\"";
        wrapper =
            L"__cp_command=\"$(printf '%s' '" + Utf8ToWide(encoded) +
            L"' | base64 -d)\"; " + invocation + L"; __cp_exit=$?; "
            L"printf '\\n__COMMAND_PANEL_DONE__:" + id + L":%d\\n' \"$__cp_exit\"; "
            L"unset __cp_command __cp_exit\n";
    }
    const std::string wire = WideToUtf8(wrapper);
    if (!session_.SendRaw(wire)) {
        currentExecutionId_.clear();
        return false;
    }
    busy_ = true;
    markerBuffer_.clear();
    return true;
}

bool CommandExecutor::SendInteractiveInput(const std::wstring& input)
{
    std::wstring value = input;
    value += L"\r\n";
    return session_.SendRaw(WideToUtf8(value));
}

CommandOutputResult CommandExecutor::ConsumeOutput(std::wstring_view text)
{
    CommandOutputResult result;
    const std::wstring prefix = L"__COMMAND_PANEL_DONE__:";
    std::wstring combined = markerBuffer_;
    combined.append(text);
    markerBuffer_.clear();

    size_t search = 0;
    while (search < combined.size()) {
        const size_t marker = combined.find(prefix, search);
        if (marker == std::wstring::npos) {
            size_t partial = std::wstring::npos;
            const size_t first = combined.size() >= prefix.size() ? combined.size() - prefix.size() + 1 : 0;
            for (size_t start = first; start < combined.size(); ++start) {
                const size_t length = combined.size() - start;
                if (length < prefix.size() && prefix.compare(0, length, combined, start, length) == 0) {
                    partial = start;
                    break;
                }
            }
            if (partial != std::wstring::npos && partial >= search) {
                result.display.append(combined.substr(search, partial - search));
                markerBuffer_ = combined.substr(partial);
            } else {
                result.display.append(combined.substr(search));
            }
            return result;
        }
        result.display.append(combined.substr(search, marker - search));
        const size_t lineEnd = combined.find_first_of(L"\r\n", marker);
        if (lineEnd == std::wstring::npos) {
            markerBuffer_ = combined.substr(marker);
            return result;
        }
        const std::wstring line = combined.substr(marker, lineEnd - marker);
        const std::wstring expected = prefix + Utf8ToWide(currentExecutionId_) + L":";
        if (busy_ && line.rfind(expected, 0) == 0) {
            try {
                result.exitCode = std::stoi(line.substr(expected.size()));
                busy_ = false;
                currentExecutionId_.clear();
            } catch (...) {
                result.display.append(line);
            }
        } else {
            result.display.append(line);
        }
        search = lineEnd;
        while (search < combined.size() && (combined[search] == L'\r' || combined[search] == L'\n')) ++search;
    }
    return result;
}

void CommandExecutor::Reset()
{
    busy_ = false;
    currentExecutionId_.clear();
    markerBuffer_.clear();
}
