#include "UiTheme.h"

#include <commctrl.h>
#include <dwmapi.h>

#include <algorithm>

namespace {
constexpr wchar_t HotProperty[] = L"CommandPanel.OwnerDrawHot";

LRESULT CALLBACK ButtonSubclass(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                UINT_PTR, DWORD_PTR)
{
    switch (message) {
    case WM_MOUSEMOVE:
        if (GetPropW(hwnd, HotProperty) == nullptr) {
            SetPropW(hwnd, HotProperty, reinterpret_cast<HANDLE>(1));
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tracking);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        RemovePropW(hwnd, HotProperty);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_ENABLE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_NCDESTROY:
        RemovePropW(hwnd, HotProperty);
        RemoveWindowSubclass(hwnd, ButtonSubclass, 1);
        break;
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}
}

namespace Ui {

int Scale(int value, UINT dpi)
{
    return MulDiv(MulDiv(value, 3, 4), static_cast<int>(dpi), 96);
}

int CompactScale(int value, UINT dpi)
{
    return MulDiv(Scale(value, dpi), 3, 4);
}

HFONT CreateFont(UINT dpi, int pointSize, int weight, const wchar_t* family)
{
    const int compactPointSize = std::max(6, MulDiv(pointSize, 3, 4));
    return ::CreateFontW(-MulDiv(compactPointSize, static_cast<int>(dpi), 72), 0, 0, 0, weight,
                         FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, family);
}

void DrawRoundedRect(HDC dc, RECT rect, COLORREF fill, COLORREF border, int radius, int penStyle)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(penStyle, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void ApplyRoundedRegion(HWND window, int width, int height, int radius)
{
    if (window == nullptr || width <= 0 || height <= 0) return;
    SetWindowRgn(window, CreateRoundRectRgn(0, 0, width + 1, height + 1, radius, radius), TRUE);
}

void EnableRoundedCorners(HWND window)
{
    if (window == nullptr) return;
    const int preference = 2; // DWMWCP_ROUND, kept numeric for older SDK compatibility.
    DwmSetWindowAttribute(window, 33, &preference, sizeof(preference));
}

void TrackOwnerDrawButton(HWND button)
{
    if (button != nullptr) SetWindowSubclass(button, ButtonSubclass, 1, 0);
}

bool IsControlHot(HWND control)
{
    return control != nullptr && GetPropW(control, HotProperty) != nullptr;
}

} // namespace Ui
