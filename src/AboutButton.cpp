#include "AboutButton.h"

#include "UiTheme.h"

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
    if (visual.kind == AboutButtonKind::Link) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, visual.enabled ? Ui::Primary : Ui::TextMuted);
        DrawTextW(dc, text.data(), static_cast<int>(text.size()), &bounds,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (visual.focused && visual.enabled) Ui::DrawFocusOutline(dc, bounds, visual.dpi);
        return;
    }

    if (visual.kind == AboutButtonKind::CheckBox) {
        Ui::DrawSelectableCheckBox(dc, bounds, text,
            Ui::SelectableVisual{visual.enabled, visual.checked, visual.pressed, visual.hot, visual.focused, visual.dpi});
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
    if (visual.focused && visual.enabled) Ui::DrawFocusOutline(dc, bounds, visual.dpi);
}
