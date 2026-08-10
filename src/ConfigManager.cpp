#include "ConfigManager.h"

#include "Utf.h"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string_view>

namespace {
class JsonParser
{
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    bool Parse(std::vector<CommandButton>& buttons, std::vector<CommandTab>& tabs,
               UiState& ui, bool& parsedTabsRoot, std::wstring& error)
    {
        Skip();
        if (!Consume('{')) return Fail(L"根节点必须是对象", error);
        int version = 1;
        bool hasButtons = false;
        bool hasTabs = false;
        Skip();
        if (Peek('}')) { Consume('}'); return Fail(L"配置缺少 buttons", error); }
        while (true) {
            std::string key;
            if (!String(key)) return Fail(L"对象键名无效", error);
            if (!Consume(':')) return Fail(L"对象键后缺少冒号", error);
            if (key == "version") {
                if (!Integer(version)) return Fail(L"version 必须是整数", error);
            } else if (key == "buttons") {
                if (!Buttons(buttons, error)) return false;
                hasButtons = true;
            } else if (key == "tabs") {
                if (!ParseTabs(tabs, error)) return false;
                hasTabs = true;
            } else if (key == "ui") {
                if (!ParseUi(ui, error)) return false;
            } else {
                if (!SkipValue()) return Fail(L"无法跳过未知字段", error);
            }
            Skip();
            if (Consume('}')) break;
            if (!Consume(',')) return Fail(L"对象字段之间缺少逗号", error);
        }
        Skip();
        if (position_ != text_.size()) return Fail(L"配置末尾存在多余内容", error);
        if (version != 1 && version != 2) return Fail(L"不支持的配置版本", error);
        if (!hasButtons && !hasTabs) return Fail(L"配置缺少 buttons 或 tabs", error);
        parsedTabsRoot = hasTabs;
        return true;
    }

    bool ParseTabs(std::vector<CommandTab>& tabs, std::wstring& error)
    {
        if (!Consume('[')) return Fail(L"tabs 必须是数组", error);
        Skip();
        if (Consume(']')) return true;
        while (true) {
            if (!Consume('{')) return Fail(L"标签页必须是对象", error);
            CommandTab tab;
            bool hasId = false, hasName = false, hasButtons = false;
            while (true) {
                std::string key;
                if (!String(key) || !Consume(':')) return Fail(L"标签页字段无效", error);
                if (key == "id" || key == "name") {
                    std::string value;
                    if (!String(value)) return Fail(L"标签页文本字段无效", error);
                    if (key == "id") { tab.id = value; hasId = true; }
                    else { tab.name = Utf8ToWide(value); hasName = true; }
                } else if (key == "buttons") {
                    if (!Buttons(tab.buttons, error)) return false;
                    hasButtons = true;
                } else if (!SkipValue()) {
                    return Fail(L"无法跳过标签页未知字段", error);
                }
                Skip();
                if (Consume('}')) break;
                if (!Consume(',')) return Fail(L"标签页字段之间缺少逗号", error);
            }
            if (!hasId || !hasName || !hasButtons || tab.id.empty() || tab.name.empty()) {
                return Fail(L"标签页必须包含非空 id、name 和 buttons", error);
            }
            tabs.push_back(std::move(tab));
            Skip();
            if (Consume(']')) break;
            if (!Consume(',')) return Fail(L"标签页之间缺少逗号", error);
        }
        return true;
    }

    bool ParseUi(UiState& ui, std::wstring& error)
    {
        if (!Consume('{')) return Fail(L"ui 必须是对象", error);
        Skip();
        if (Consume('}')) return true;
        while (true) {
            std::string key;
            if (!String(key) || !Consume(':')) return Fail(L"ui 字段无效", error);
            int value = 0;
            if (key == "window_width" || key == "window_height" ||
                key == "button_section_height" || key == "input_section_height") {
                if (!Integer(value) || value < 0) return Fail(L"ui 尺寸必须是非负整数", error);
                if (key == "window_width") ui.windowWidth = value;
                else if (key == "window_height") ui.windowHeight = value;
                else if (key == "button_section_height") ui.buttonSectionHeight = value;
                else ui.inputSectionHeight = value;
            } else if (!SkipValue()) {
                return Fail(L"无法跳过 ui 未知字段", error);
            }
            Skip();
            if (Consume('}')) break;
            if (!Consume(',')) return Fail(L"ui 字段之间缺少逗号", error);
        }
        return true;
    }

private:
    bool Fail(const std::wstring& message, std::wstring& error)
    {
        error = message + L"（位置 " + std::to_wstring(position_) + L"）";
        return false;
    }

    void Skip()
    {
        while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\t' ||
                                             text_[position_] == '\r' || text_[position_] == '\n')) ++position_;
    }

    bool Consume(char value)
    {
        Skip();
        if (position_ < text_.size() && text_[position_] == value) { ++position_; return true; }
        return false;
    }

    bool Peek(char value)
    {
        Skip();
        return position_ < text_.size() && text_[position_] == value;
    }

    bool String(std::string& output)
    {
        Skip();
        if (position_ >= text_.size() || text_[position_] != '"') return false;
        ++position_;
        output.clear();
        while (position_ < text_.size()) {
            const char ch = text_[position_++];
            if (ch == '"') return true;
            if (ch == '\\') {
                if (position_ >= text_.size()) return false;
                const char escaped = text_[position_++];
                switch (escaped) {
                case '"': output.push_back('"'); break;
                case '\\': output.push_back('\\'); break;
                case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                case 'u': {
                    if (position_ + 4 > text_.size()) return false;
                    for (int i = 0; i < 4; ++i) {
                        const char digit = text_[position_++];
                        if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') ||
                              (digit >= 'A' && digit <= 'F'))) return false;
                    }
                    // Config files are written as UTF-8, so retain escaped Unicode as UTF-8.
                    const std::string hex = text_.substr(position_ - 4, 4);
                    unsigned long code = std::stoul(hex, nullptr, 16);
                    if (code <= 0x7F) output.push_back(static_cast<char>(code));
                    else if (code <= 0x7FF) {
                        output.push_back(static_cast<char>(0xC0 | (code >> 6)));
                        output.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    } else {
                        output.push_back(static_cast<char>(0xE0 | (code >> 12)));
                        output.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                        output.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    }
                    break;
                }
                default: return false;
                }
            } else {
                if (static_cast<unsigned char>(ch) < 0x20) return false;
                output.push_back(ch);
            }
        }
        return false;
    }

    bool Literal(std::string_view literal)
    {
        Skip();
        if (text_.compare(position_, literal.size(), literal) != 0) return false;
        position_ += literal.size();
        return true;
    }

    bool Boolean(bool& value)
    {
        if (Literal("true")) { value = true; return true; }
        if (Literal("false")) { value = false; return true; }
        return false;
    }

    bool Integer(int& value)
    {
        Skip();
        const size_t start = position_;
        if (position_ < text_.size() && text_[position_] == '-') ++position_;
        const size_t digits = position_;
        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') ++position_;
        if (position_ == digits) { position_ = start; return false; }
        try { value = std::stoi(text_.substr(start, position_ - start)); return true; }
        catch (...) { position_ = start; return false; }
    }

    bool Buttons(std::vector<CommandButton>& buttons, std::wstring& error)
    {
        if (!Consume('[')) return Fail(L"buttons 必须是数组", error);
        Skip();
        if (Consume(']')) return true;
        while (true) {
            if (!Consume('{')) return Fail(L"按钮必须是对象", error);
            CommandButton button;
            bool hasId = false, hasName = false, hasCommand = false;
            while (true) {
                std::string key;
                if (!String(key) || !Consume(':')) return Fail(L"按钮字段无效", error);
                std::string value;
                if (key == "id" || key == "name" || key == "command") {
                    if (!String(value)) return Fail(L"按钮文本字段无效", error);
                    if (key == "id") { button.id = value; hasId = true; }
                    else if (key == "name") { button.name = Utf8ToWide(value); hasName = true; }
                    else { button.command = Utf8ToWide(value); hasCommand = true; }
                } else if (key == "confirm" || key == "enabled") {
                    bool boolean = false;
                    if (!Boolean(boolean)) return Fail(L"按钮布尔字段无效", error);
                    if (key == "confirm") button.confirm = boolean;
                    else button.enabled = boolean;
                } else if (!SkipValue()) {
                    return Fail(L"无法跳过按钮未知字段", error);
                }
                Skip();
                if (Consume('}')) break;
                if (!Consume(',')) return Fail(L"按钮字段之间缺少逗号", error);
            }
            if (!hasId || !hasName || !hasCommand || button.id.empty() || button.name.empty() || button.command.empty()) {
                return Fail(L"按钮必须包含非空 id、name 和 command", error);
            }
            buttons.push_back(std::move(button));
            Skip();
            if (Consume(']')) break;
            if (!Consume(',')) return Fail(L"按钮之间缺少逗号", error);
        }
        return true;
    }

    bool SkipValue()
    {
        Skip();
        if (position_ >= text_.size()) return false;
        if (text_[position_] == '"') { std::string ignored; return String(ignored); }
        if (text_[position_] == '{') {
            ++position_;
            int depth = 1;
            bool quoted = false, escaped = false;
            while (position_ < text_.size() && depth > 0) {
                const char ch = text_[position_++];
                if (quoted) { if (escaped) escaped = false; else if (ch == '\\') escaped = true; else if (ch == '"') quoted = false; }
                else if (ch == '"') quoted = true;
                else if (ch == '{') ++depth;
                else if (ch == '}') --depth;
            }
            return depth == 0;
        }
        if (text_[position_] == '[') {
            ++position_;
            int depth = 1;
            while (position_ < text_.size() && depth > 0) { if (text_[position_] == '[') ++depth; else if (text_[position_] == ']') --depth; ++position_; }
            return depth == 0;
        }
        bool ignoredBool = false;
        if (Boolean(ignoredBool)) return true;
        int ignoredInt = 0;
        if (Integer(ignoredInt)) return true;
        return false;
    }

    std::string text_;
    size_t position_ = 0;
};

std::string EscapeJson(std::wstring_view value)
{
    const std::string utf8 = WideToUtf8(value);
    std::string result = "\"";
    for (unsigned char ch : utf8) {
        switch (ch) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result.push_back(static_cast<char>(ch)); break;
        }
    }
    result += '"';
    return result;
}

bool ReadFileUtf8(const std::filesystem::path& path, std::string& content, std::wstring& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { error = L"无法打开配置文件"; return false; }
    content.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF) {
        error = L"配置文件不应包含 BOM";
        return false;
    }
    return true;
}
}

ConfigManager::ConfigManager()
{
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    legacyPath_ = std::filesystem::path(modulePath, modulePath + length).parent_path() / L"config.json";
    PWSTR localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData))) {
        path_ = std::filesystem::path(localAppData) / L"快捷控制台" / L"config.json";
        CoTaskMemFree(localAppData);
    } else {
        wchar_t fallback[MAX_PATH]{};
        const DWORD fallbackLength = GetEnvironmentVariableW(L"LOCALAPPDATA", fallback, MAX_PATH);
        path_ = fallbackLength > 0 && fallbackLength < MAX_PATH
            ? std::filesystem::path(fallback, fallback + fallbackLength) / L"快捷控制台" / L"config.json"
            : legacyPath_;
    }
}

bool ConfigManager::Load()
{
    lastError_.clear();
    wasMissing_ = false;
    tabs_.clear();
    ui_ = {};
    if (!std::filesystem::exists(path_)) {
        std::error_code directoryError;
        std::filesystem::create_directories(path_.parent_path(), directoryError);
        if (directoryError) {
            lastError_ = L"无法创建配置目录：" + path_.parent_path().wstring();
            return false;
        }
        if (path_ != legacyPath_ && std::filesystem::exists(legacyPath_)) {
            if (!CopyFileW(legacyPath_.c_str(), path_.c_str(), TRUE)) {
                lastError_ = L"无法迁移旧配置：" + Win32ErrorMessage(GetLastError());
                return false;
            }
        } else {
            wasMissing_ = true;
            tabs_ = DefaultTabs();
            if (!Save()) return false;
            return true;
        }
    }

    std::string content;
    if (!ReadFileUtf8(path_, content, lastError_)) {
        return false;
    }
    JsonParser parser(std::move(content));
    std::vector<CommandButton> legacyButtons;
    std::vector<CommandTab> parsedTabs;
    UiState parsedUi;
    bool parsedTabsRoot = false;
    auto parseRoot = [&]() -> bool {
        // Parse the document once as a legacy root. The parser's root parser is
        // extended below through a small compatibility conversion path.
        return parser.Parse(legacyButtons, parsedTabs, parsedUi, parsedTabsRoot, lastError_);
    };
    if (!parseRoot()) {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        std::wstring broken = path_.wstring() + L".broken." +
                              std::to_wstring(time.wYear) + std::to_wstring(time.wMonth) +
                              std::to_wstring(time.wDay) + L"-" + std::to_wstring(time.wHour) +
                              std::to_wstring(time.wMinute) + std::to_wstring(time.wSecond);
        CopyFileW(path_.c_str(), broken.c_str(), TRUE);
        tabs_.clear();
        lastError_ = L"配置文件解析失败：" + lastError_ + L"。原文件已保留为：" + broken;
        return false;
    }
    if (parsedTabsRoot) {
        tabs_ = std::move(parsedTabs);
    } else if (!legacyButtons.empty()) {
        tabs_ = DefaultTabs();
        for (auto& button : legacyButtons) {
            size_t target = 0;
            if (button.id == "openclaw-start" || button.id == "openclaw-restart" ||
                button.id == "openclaw-stop" || button.id == "openclaw-doctor") target = 1;
            else if (button.id == "wsl-list" || button.id.rfind("system-", 0) == 0) target = 2;
            auto& destination = tabs_[target].buttons;
            auto existing = std::find_if(destination.begin(), destination.end(), [&](const auto& value) {
                return value.id == button.id;
            });
            if (existing != destination.end()) *existing = std::move(button);
            else destination.push_back(std::move(button));
        }
        // Upgrade version 1 files immediately so the next launch preserves tabs.
        Save();
    }
    ui_ = parsedUi;
    return true;
}

bool ConfigManager::Save()
{
    lastError_.clear();
    std::error_code directoryError;
    std::filesystem::create_directories(path_.parent_path(), directoryError);
    if (directoryError) {
        lastError_ = L"无法创建配置目录：" + path_.parent_path().wstring();
        return false;
    }
    std::string content = "{\n  \"version\": 2,\n  \"ui\": {\n";
    content += "    \"window_width\": " + std::to_string(ui_.windowWidth) + ",\n";
    content += "    \"window_height\": " + std::to_string(ui_.windowHeight) + ",\n";
    content += "    \"button_section_height\": " + std::to_string(ui_.buttonSectionHeight) + ",\n";
    content += "    \"input_section_height\": " + std::to_string(ui_.inputSectionHeight) + "\n";
    content += "  },\n  \"tabs\": [\n";
    for (size_t tabIndex = 0; tabIndex < tabs_.size(); ++tabIndex) {
        const auto& tab = tabs_[tabIndex];
        content += "    {\n      \"id\": " + EscapeJson(Utf8ToWide(tab.id)) + ",\n";
        content += "      \"name\": " + EscapeJson(tab.name) + ",\n      \"buttons\": [\n";
        for (size_t i = 0; i < tab.buttons.size(); ++i) {
            const auto& button = tab.buttons[i];
            content += "        {\n          \"id\": " + EscapeJson(Utf8ToWide(button.id)) + ",\n";
            content += "          \"name\": " + EscapeJson(button.name) + ",\n";
            content += "          \"command\": " + EscapeJson(button.command) + ",\n";
            content += "          \"confirm\": " + std::string(button.confirm ? "true" : "false") + ",\n";
            content += "          \"enabled\": " + std::string(button.enabled ? "true" : "false") + "\n        }";
            if (i + 1 != tab.buttons.size()) content += ',';
            content += '\n';
        }
        content += "      ]\n    }";
        if (tabIndex + 1 != tabs_.size()) content += ',';
        content += '\n';
    }
    content += "  ]\n}\n";
    const auto temporary = path_.wstring() + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        lastError_ = L"无法创建临时配置文件：" + Win32ErrorMessage(GetLastError());
        return false;
    }
    DWORD written = 0;
    const BOOL wrote = WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
                       written == content.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!wrote || !MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        DeleteFileW(temporary.c_str());
        lastError_ = L"替换配置文件失败：" + Win32ErrorMessage(error);
        return false;
    }
    return true;
}

std::string ConfigManager::NewId()
{
    GUID guid{};
    CoCreateGuid(&guid);
    wchar_t text[64]{};
    StringFromGUID2(guid, text, 64);
    std::wstring value(text);
    value.erase(std::remove(value.begin(), value.end(), L'{'), value.end());
    value.erase(std::remove(value.begin(), value.end(), L'}'), value.end());
    return WideToUtf8(value);
}

std::vector<CommandTab> ConfigManager::DefaultTabs()
{
    return {
        {"common", L"常用", {
            {"openclaw-status", L"OpenClaw 状态", L"wsl.exe -- bash -lic \"openclaw gateway status\"", false, true},
            {"wsl-status", L"WSL 状态", L"wsl.exe --status", false, true},
            {"wsl-shutdown", L"WSL Shutdown", L"wsl.exe --shutdown", true, true},
        }},
        {"openclaw", L"OpenClaw", {
            {"openclaw-start", L"启动", L"wsl.exe -- bash -lic \"openclaw gateway start\"", false, true},
            {"openclaw-restart", L"重启", L"wsl.exe -- bash -lic \"openclaw gateway restart\"", true, true},
            {"openclaw-stop", L"停止", L"wsl.exe -- bash -lic \"openclaw gateway stop\"", true, true},
            {"openclaw-doctor", L"Doctor", L"wsl.exe -- bash -lic \"openclaw doctor\"", false, true},
        }},
        {"system", L"系统", {
            {"wsl-list", L"WSL 列表", L"wsl.exe -l -v", false, true},
            {"system-processes", L"进程列表", L"Get-Process", false, true},
            {"system-services", L"服务列表", L"Get-Service", false, true},
            {"system-ipconfig", L"网络配置", L"ipconfig", false, true},
        }},
    };
}
