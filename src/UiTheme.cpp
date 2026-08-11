#include "UiTheme.h"

#include <commctrl.h>
#include <dwmapi.h>

#include <algorithm>

namespace {
constexpr wchar_t HotProperty[] = L"CommandPanel.OwnerDrawHot";

void DrawCheckMark(HDC dc, const RECT& box, UINT dpi)
{
    HPEN pen = CreatePen(PS_SOLID, std::max(1, Ui::Scale(2, dpi)), RGB(255, 255, 255));
    HGDIOBJ previous = SelectObject(dc, pen);
    MoveToEx(dc, box.left + Ui::Scale(4, dpi), box.top + Ui::Scale(9, dpi), nullptr);
    LineTo(dc, box.left + Ui::Scale(8, dpi), box.bottom - Ui::Scale(5, dpi));
    LineTo(dc, box.right - Ui::Scale(4, dpi), box.top + Ui::Scale(5, dpi));
    SelectObject(dc, previous);
    DeleteObject(pen);
}

void DrawFocusRing(HDC dc, RECT bounds, UINT dpi)
{
    InflateRect(&bounds, -Ui::Scale(2, dpi), -Ui::Scale(2, dpi));
    Ui::DrawRoundedRect(dc, bounds, Ui::Primary, Ui::Primary, Ui::Scale(7, dpi));
}

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

bool ToggleSelectable(bool& selected, bool enabled)
{
    if (!enabled) return false;
    selected = !selected;
    return true;
}

void DrawSelectableCheckBox(HDC dc, RECT bounds, std::wstring_view text,
                            const SelectableVisual& visual)
{
    HBRUSH background = CreateSolidBrush(Window);
    FillRect(dc, &bounds, background);
    DeleteObject(background);
    if (visual.focused && visual.enabled) DrawFocusRing(dc, bounds, visual.dpi);
    const int available = static_cast<int>(std::min(bounds.right - bounds.left, bounds.bottom - bounds.top));
    const int side = std::max(Scale(17, visual.dpi), available - Scale(4, visual.dpi));
    RECT box{bounds.left + Scale(1, visual.dpi), (bounds.top + bounds.bottom - side) / 2,
             bounds.left + Scale(1, visual.dpi) + side, (bounds.top + bounds.bottom + side) / 2};
    const COLORREF fill = !visual.enabled ? RGB(238, 241, 245) : (visual.selected ? Primary : Window);
    const COLORREF border = visual.selected ? Primary : (visual.hot || visual.focused ? Primary : Border);
    DrawRoundedRect(dc, box, fill, border, Scale(4, visual.dpi));
    if (visual.selected) DrawCheckMark(dc, box, visual.dpi);
    RECT label{box.right + Scale(8, visual.dpi), bounds.top, bounds.right, bounds.bottom};
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, visual.enabled ? Text : TextMuted);
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &label,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawSelectableOption(HDC dc, RECT bounds, std::wstring_view text,
                          const SelectableVisual& visual, COLORREF selectedColor)
{
    const COLORREF fill = visual.selected ? selectedColor :
        (!visual.enabled ? RGB(238, 241, 245) : (visual.pressed ? RGB(232, 237, 244) : (visual.hot ? SurfaceHover : Window)));
    const COLORREF border = visual.selected ? selectedColor : BorderStrong;
    DrawRoundedRect(dc, bounds, fill, border, Scale(7, visual.dpi));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, !visual.enabled ? TextMuted : (visual.selected ? RGB(255, 255, 255) : Text));
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &bounds,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
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
