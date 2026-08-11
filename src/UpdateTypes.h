#pragma once

#include "AboutLayout.h"

#include <cstdint>
#include <functional>
#include <string>

struct UpdateViewState {
    AboutPresentation presentation = AboutPresentation::Idle;
    std::wstring status = L"点击“检查更新”获取最新版本。";
    std::wstring version;
    std::wstring releaseNotes;
    std::wstring node;
    std::uint64_t received{};
    std::uint64_t total{};
    double bytesPerSecond{};
    int connections{1};
    bool parallelFallback{};
    bool background{};
    bool acceleration{true};
};

using UpdateObserver = std::function<void(const UpdateViewState&)>;
