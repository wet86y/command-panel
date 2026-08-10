#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct CommandButton
{
    std::string id;
    std::wstring name;
    std::wstring command;
    bool confirm = false;
    bool enabled = true;
};

struct CommandTab
{
    std::string id;
    std::wstring name;
    std::vector<CommandButton> buttons;
};

class ConfigManager
{
public:
    ConfigManager();

    bool Load();
    bool Save();
    bool WasMissing() const { return wasMissing_; }
    const std::wstring& LastError() const { return lastError_; }
    const std::filesystem::path& ConfigPath() const { return path_; }

    std::vector<CommandTab>& Tabs() { return tabs_; }
    const std::vector<CommandTab>& Tabs() const { return tabs_; }

    static std::string NewId();
    static std::vector<CommandTab> DefaultTabs();

private:
    std::filesystem::path path_;
    std::vector<CommandTab> tabs_;
    std::wstring lastError_;
    bool wasMissing_ = false;
};
