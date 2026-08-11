#pragma once

#include <windows.h>

#include <string_view>

enum class AboutButtonKind {
    Primary,
    Secondary,
    Link,
    CheckBox,
};

struct AboutButtonVisual {
    AboutButtonKind kind{AboutButtonKind::Secondary};
    bool enabled{true};
    bool pressed{};
    bool hot{};
    bool focused{};
    bool checked{};
    UINT dpi{96};
};

// This renderer deliberately owns every visible button pixel.  It is shared by
// the window and the DIB regression tests so system button chrome cannot return.
// The explicit background fill is required before drawing rounded geometry into
// an uninitialised BufferedPaint surface.
void FillAboutButtonBackground(HDC dc, const RECT& bounds);
void DrawAboutButton(HDC dc, RECT bounds, std::wstring_view text,
                     const AboutButtonVisual& visual);
