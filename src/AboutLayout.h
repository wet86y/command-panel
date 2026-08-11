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
    RECT productCard{};
    RECT updateCard{};
    RECT icon{};
    RECT name{};
    RECT version{};
    RECT developer{};
    RECT currentVersion{};
    RECT status{};
    RECT progress{};
    RECT notes{};
    RECT check{};
    RECT download{};
    RECT pauseResume{};
    RECT background{};
    RECT cancel{};
    RECT acceleration{};
    RECT nextNode{};
    RECT install{};
    RECT repository{};
    int minimumWidth{};
    int minimumHeight{};
};

AboutLayout CalculateAboutLayout(int width, int height, UINT dpi,
                                 AboutPresentation presentation) noexcept;
