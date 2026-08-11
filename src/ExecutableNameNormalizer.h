#pragma once

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>

enum class ExecutableNameNormalizationResult {
    Unchanged,
    RelaunchStarted,
    Failed,
};

std::optional<std::filesystem::path> CanonicalExecutableTarget(
    const std::filesystem::path& executable, const std::wstring& canonicalFileName);
ExecutableNameNormalizationResult NormalizeExecutableName(
    HINSTANCE instance, const std::filesystem::path& executable,
    const std::wstring& canonicalFileName, std::wstring& error);
