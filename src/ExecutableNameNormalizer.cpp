#include "ExecutableNameNormalizer.h"

#include "LocalUpdateDiagnostics.h"
#include "UpdateCoordinator.h"

#include <DesktopUpdateKit/UpdateKit.h>

namespace {
std::wstring Widen(const std::string& value)
{
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (length > 0) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::vector<std::byte> LoadUpdaterStub(HINSTANCE instance)
{
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(kUpdaterStubResourceId), RT_RCDATA);
    if (resource == nullptr) return {};
    HGLOBAL loaded = LoadResource(instance, resource);
    const DWORD size = SizeofResource(instance, resource);
    const auto* bytes = loaded != nullptr ? static_cast<const std::byte*>(LockResource(loaded)) : nullptr;
    return bytes != nullptr && size != 0 ? std::vector<std::byte>(bytes, bytes + size) : std::vector<std::byte>{};
}
}

std::optional<std::filesystem::path> CanonicalExecutableTarget(
    const std::filesystem::path& executable, const std::wstring& canonicalFileName)
{
    if (!executable.is_absolute() || canonicalFileName.empty()) return {};
    const std::filesystem::path canonical(canonicalFileName);
    if (canonical.has_parent_path() || canonical.filename() != canonical ||
        _wcsicmp(canonical.extension().c_str(), L".exe") != 0 ||
        _wcsicmp(executable.filename().c_str(), canonicalFileName.c_str()) == 0) return {};
    return executable.parent_path() / canonical;
}

ExecutableNameNormalizationResult NormalizeExecutableName(
    HINSTANCE instance, const std::filesystem::path& executable,
    const std::wstring& canonicalFileName, std::wstring& error)
{
    try {
        const auto target = CanonicalExecutableTarget(executable, canonicalFileName);
        if (!target) {
            RecordLocalUpdateDiagnostic(L"rename unchanged source=" + executable.wstring());
            return ExecutableNameNormalizationResult::Unchanged;
        }
        RecordLocalUpdateDiagnostic(L"rename requested source=" + executable.wstring() + L" target=" + target->wstring());
        const auto stub = LoadUpdaterStub(instance);
        if (stub.empty()) {
            error = L"内嵌更新助手不可用。";
            RecordLocalUpdateDiagnostic(L"rename failed stage=load-stub");
            return ExecutableNameNormalizationResult::Failed;
        }
        const auto result = desktop_update_kit::launch_rename(
            stub, executable, *target, static_cast<int>(GetCurrentProcessId()));
        if (!result.started) {
            error = Widen(result.error);
            RecordLocalUpdateDiagnostic(L"rename failed stage=launch-stub error=" + error);
            return ExecutableNameNormalizationResult::Failed;
        }
        RecordLocalUpdateDiagnostic(L"rename started stub-bytes=" + std::to_wstring(stub.size()));
        return ExecutableNameNormalizationResult::RelaunchStarted;
    } catch (const std::exception& exception) {
        error = Widen(exception.what());
        RecordLocalUpdateDiagnostic(L"rename failed stage=exception error=" + error);
        return ExecutableNameNormalizationResult::Failed;
    } catch (...) {
        error = L"无法规范化程序文件名。";
        RecordLocalUpdateDiagnostic(L"rename failed stage=unknown-exception");
        return ExecutableNameNormalizationResult::Failed;
    }
}
