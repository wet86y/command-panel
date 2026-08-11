#include <DesktopUpdateKit/UpdateKit.h>

#include <windows.h>

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
std::vector<std::byte> LoadUpdaterStub(HINSTANCE instance)
{
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(201), RT_RCDATA);
    if (resource == nullptr) return {};
    HGLOBAL loaded = LoadResource(instance, resource);
    const DWORD size = SizeofResource(instance, resource);
    const auto* bytes = loaded != nullptr ? static_cast<const std::byte*>(LockResource(loaded)) : nullptr;
    return bytes != nullptr && size != 0 ? std::vector<std::byte>(bytes, bytes + size) : std::vector<std::byte>{};
}

std::string NarrowAscii(const wchar_t* text)
{
    std::string result;
    while (*text != L'\0') result.push_back(static_cast<char>(*text++));
    return result;
}
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 4) {
        std::wcerr << L"Usage: CommandPanelUpdateHandoffTest <downloaded-exe> <target-exe> <sha256>\n";
        return 10;
    }
    const std::filesystem::path downloaded(argv[1]);
    const std::filesystem::path target(argv[2]);
    if (!downloaded.is_absolute() || !target.is_absolute() || !std::filesystem::exists(downloaded)) return 11;
    const auto stub = LoadUpdaterStub(GetModuleHandleW(nullptr));
    if (stub.empty()) return 12;
    const auto result = desktop_update_kit::launch_update(stub, downloaded, target, NarrowAscii(argv[3]),
                                                           static_cast<int>(GetCurrentProcessId()));
    if (!result.started) {
        std::cerr << result.error << '\n';
        return 13;
    }
    return 0;
}
