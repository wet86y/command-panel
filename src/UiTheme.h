#pragma once

#include <windows.h>

#include <string_view>

namespace Ui {

constexpr COLORREF Window = RGB(255, 255, 255);
constexpr COLORREF Surface = RGB(248, 250, 252);
constexpr COLORREF SurfaceHover = RGB(243, 247, 252);
constexpr COLORREF Border = RGB(218, 224, 232);
constexpr COLORREF BorderStrong = RGB(199, 208, 219);
constexpr COLORREF Text = RGB(27, 34, 44);
constexpr COLORREF TextMuted = RGB(103, 112, 124);
constexpr COLORREF Primary = RGB(24, 111, 235);
constexpr COLORREF PrimaryPressed = RGB(17, 88, 205);
constexpr COLORREF PrimarySoft = RGB(235, 243, 255);
constexpr COLORREF Success = RGB(20, 153, 82);
constexpr COLORREF Danger = RGB(210, 42, 55);
constexpr COLORREF Terminal = RGB(10, 16, 22);
constexpr COLORREF TerminalText = RGB(226, 232, 240);

// Owner-drawn BUTTON controls do not support the native checkbox/radio state
// protocol.  Selected state therefore always belongs to the caller.
struct SelectableVisual {
    bool enabled{true};
    bool selected{};
    bool pressed{};
    bool hot{};
    bool focused{};
    UINT dpi{96};
};

int Scale(int value, UINT dpi);
int CompactScale(int value, UINT dpi);
HFONT CreateFont(UINT dpi, int pointSize, int weight = FW_NORMAL,
                 const wchar_t* family = L"Segoe UI");
void DrawRoundedRect(HDC dc, RECT rect, COLORREF fill, COLORREF border, int radius,
                     int penStyle = PS_SOLID);
void ApplyRoundedRegion(HWND window, int width, int height, int radius);
void EnableRoundedCorners(HWND window);

bool ToggleSelectable(bool& selected, bool enabled = true);
void DrawSelectableCheckBox(HDC dc, RECT bounds, std::wstring_view text,
                            const SelectableVisual& visual);
void DrawSelectableOption(HDC dc, RECT bounds, std::wstring_view text,
                          const SelectableVisual& visual, COLORREF selectedColor);

void TrackOwnerDrawButton(HWND button);
bool IsControlHot(HWND control);

} // namespace Ui
