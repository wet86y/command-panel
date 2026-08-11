#include "AboutButton.h"

#include "UiTheme.h"

#include <algorithm>

namespace {
void DrawFocusRing(HDC dc, RECT bounds, UINT dpi)
{
    InflateRect(&bounds, -Ui::Scale(2, dpi), -Ui::Scale(2, dpi));
    Ui::DrawRoundedRect(dc, bounds, Ui::Primary, Ui::Primary, Ui::Scale(7, dpi));
}

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
}

void FillAboutButtonBackground(HDC dc, const RECT& bounds)
{
    HBRUSH background = CreateSolidBrush(Ui::Window);
    FillRect(dc, &bounds, background);
    DeleteObject(background);
}

void DrawAboutButton(HDC dc, RECT bounds, std::wstring_view text,
                     const AboutButtonVisual& visual)
{
    FillAboutButtonBackground(dc, bounds);
    const int radius = Ui::Scale(7, visual.dpi);
    const bool interactive = visual.enabled && visual.kind != AboutButtonKind::Link;
    if (visual.focused && visual.enabled) DrawFocusRing(dc, bounds, visual.dpi);

    if (visual.kind == AboutButtonKind::Link) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, visual.enabled ? Ui::Primary : Ui::TextMuted);
        DrawTextW(dc, text.data(), static_cast<int>(text.size()), &bounds,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return;
    }

    if (visual.kind == AboutButtonKind::CheckBox) {
        const int available = static_cast<int>(std::min(bounds.right - bounds.left, bounds.bottom - bounds.top));
        const int side = std::max(Ui::Scale(17, visual.dpi), available - Ui::Scale(4, visual.dpi));
        RECT box{bounds.left + Ui::Scale(1, visual.dpi), (bounds.top + bounds.bottom - side) / 2,
                 bounds.left + Ui::Scale(1, visual.dpi) + side, (bounds.top + bounds.bottom + side) / 2};
        const COLORREF fill = !visual.enabled ? RGB(238, 241, 245) : (visual.checked ? Ui::Primary : Ui::Window);
        const COLORREF border = visual.checked ? Ui::Primary : (visual.hot || visual.focused ? Ui::Primary : Ui::Border);
        Ui::DrawRoundedRect(dc, box, fill, border, Ui::Scale(4, visual.dpi));
        if (visual.checked) DrawCheckMark(dc, box, visual.dpi);
        RECT label{box.right + Ui::Scale(8, visual.dpi), bounds.top, bounds.right, bounds.bottom};
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, visual.enabled ? Ui::Text : Ui::TextMuted);
        DrawTextW(dc, text.data(), static_cast<int>(text.size()), &label,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return;
    }

    const bool primary = visual.kind == AboutButtonKind::Primary;
    COLORREF fill = primary ? Ui::Primary : Ui::Window;
    COLORREF border = primary ? Ui::Primary : Ui::Border;
    if (!visual.enabled) { fill = RGB(238, 241, 245); border = Ui::Border; }
    else if (visual.pressed) fill = primary ? Ui::PrimaryPressed : RGB(232, 237, 244);
    else if (visual.hot) { fill = primary ? RGB(40, 125, 240) : Ui::SurfaceHover; border = primary ? fill : Ui::BorderStrong; }
    if (!interactive) { fill = Ui::Window; border = Ui::Border; }
    Ui::DrawRoundedRect(dc, bounds, fill, border, radius);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, !visual.enabled ? Ui::TextMuted : (primary ? RGB(255, 255, 255) : Ui::Text));
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &bounds,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}
