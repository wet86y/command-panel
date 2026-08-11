#pragma once

#include <windows.h>

enum class AboutPresentation {
    Idle,
    Checking,
    Available,
    Downloading,
    Paused,
    Completed,
    Failed,
    Cancelled,
    Launching,
};

struct AboutLayout {
    RECT name{};
    RECT version{};
    RECT developer{};
    RECT repository{};
    RECT status{};
    RECT progress{};
    RECT acceleration{};
    RECT notes{};
    RECT check{};
    RECT download{};
    RECT pauseResume{};
    RECT background{};
    RECT cancel{};
    RECT nextNode{};
    RECT install{};
    int minimumWidth{};
    int minimumHeight{};
};

AboutLayout CalculateAboutLayout(int width, int height, UINT dpi,
                                 AboutPresentation presentation) noexcept;
