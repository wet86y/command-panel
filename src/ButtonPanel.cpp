#include "ButtonPanel.h"

#include "UiScroll.h"
#include "UiTheme.h"

#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdlib>

namespace {
constexpr UINT_PTR ScrollHideTimer = 1;
constexpr UINT ScrollHideDelayMs = 1200;
}

ButtonPanel::~ButtonPanel()
{
    if (font_ != nullptr) DeleteObject(font_);
}

bool ButtonPanel::Create(HWND parent)
{
    static bool registered = false;
    const wchar_t* className = L"CommandPanelButtonPanel";
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ButtonPanel::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = className;
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        registered = true;
    }
    hwnd_ = CreateWindowExW(0, className, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            0, 0, 0, 0, parent, nullptr, GetModuleHandleW(nullptr), this);
    if (hwnd_ == nullptr) return false;
    fontDpi_ = GetDpiForWindow(hwnd_);
    font_ = Ui::CreateFont(fontDpi_, 9);
    return true;
}

void ButtonPanel::SetCallbacks(ClickCallback click, ContextCallback context)
{
    clickCallback_ = std::move(click);
    contextCallback_ = std::move(context);
}

void ButtonPanel::SetButtons(const std::vector<CommandButton>& buttons)
{
    Rebuild(buttons);
}

void ButtonPanel::Rebuild(const std::vector<CommandButton>& buttons)
{
    buttons_ = buttons;
    availability_.assign(buttons_.size(), true);
    scrollPosition_ = 0;
    wheelRemainder_ = 0;
    hotIndex_ = -1;
    pressedIndex_ = -1;
    focusedIndex_ = buttons_.empty() ? 0 : std::min(focusedIndex_, static_cast<int>(buttons_.size()));
    Layout();
}

void ButtonPanel::SetAvailability(std::vector<bool> availability)
{
    availability_ = std::move(availability);
    if (availability_.size() != buttons_.size()) availability_.assign(buttons_.size(), true);
    if (!IsCardEnabled(pressedIndex_)) pressedIndex_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ButtonPanel::SetBusy(bool busy)
{
    if (busy_ == busy) return;
    busy_ = busy;
    if (!IsCardEnabled(pressedIndex_)) pressedIndex_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool ButtonPanel::IsCardEnabled(int index) const
{
    if (index < 0 || index >= CardCount()) return false;
    if (index == static_cast<int>(buttons_.size())) return true;
    const std::size_t value = static_cast<std::size_t>(index);
    return buttons_[value].enabled && !busy_ &&
           (value >= availability_.size() || availability_[value]);
}

int ButtonPanel::HitTest(POINT point) const
{
    for (std::size_t index = 0; index < layout_.cards.size(); ++index) {
        if (PtInRect(&layout_.cards[index], point)) return static_cast<int>(index);
    }
    return -1;
}

void ButtonPanel::ShowScrollIndicator()
{
    if (layout_.maximumScroll <= 0) {
        HideScrollIndicator();
        return;
    }
    scrollIndicatorVisible_ = true;
    SetTimer(hwnd_, ScrollHideTimer, ScrollHideDelayMs, nullptr);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ButtonPanel::HideScrollIndicator()
{
    KillTimer(hwnd_, ScrollHideTimer);
    if (!scrollIndicatorVisible_) return;
    scrollIndicatorVisible_ = false;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ButtonPanel::ScrollTo(int position, bool revealIndicator)
{
    const int next = std::clamp(position, 0, layout_.maximumScroll);
    const bool changed = next != scrollPosition_;
    scrollPosition_ = next;
    if (changed) {
        hotIndex_ = -1;
        Layout();
    }
    if (revealIndicator && (changed || layout_.maximumScroll > 0)) ShowScrollIndicator();
}

void ButtonPanel::Layout()
{
    if (hwnd_ == nullptr) return;
    RECT client{};
    GetClientRect(hwnd_, &client);
    const UINT dpi = GetDpiForWindow(hwnd_);
    if (fontDpi_ != dpi) {
        if (font_ != nullptr) DeleteObject(font_);
        fontDpi_ = dpi;
        font_ = Ui::CreateFont(dpi, 9);
    }
    layout_ = CalculateButtonLayout(client.right, client.bottom, dpi,
                                    static_cast<std::size_t>(CardCount()), scrollPosition_);
    const int clamped = std::clamp(scrollPosition_, 0, layout_.maximumScroll);
    if (clamped != scrollPosition_) {
        scrollPosition_ = clamped;
        layout_ = CalculateButtonLayout(client.right, client.bottom, dpi,
                                        static_cast<std::size_t>(CardCount()), scrollPosition_);
    }
    if (layout_.maximumScroll == 0) HideScrollIndicator();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK ButtonPanel::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<ButtonPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = reinterpret_cast<ButtonPanel*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(message, wParam, lParam) :
                  DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT ButtonPanel::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC target = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        HDC dc = nullptr;
        HPAINTBUFFER buffer = BeginBufferedPaint(target, &client, BPBF_COMPATIBLEBITMAP, nullptr, &dc);
        if (buffer == nullptr) dc = target;

        HBRUSH background = CreateSolidBrush(Ui::Window);
        FillRect(dc, &client, background);
        DeleteObject(background);
        HFONT previousFont = static_cast<HFONT>(SelectObject(dc, font_));
        SetBkMode(dc, TRANSPARENT);
        const bool panelFocused = GetFocus() == hwnd_;
        for (std::size_t index = 0; index < layout_.cards.size(); ++index) {
            RECT rect = layout_.cards[index];
            RECT visible{};
            if (!IntersectRect(&visible, &rect, &client)) continue;
            const int cardIndex = static_cast<int>(index);
            const bool isAdd = cardIndex == static_cast<int>(buttons_.size());
            const bool enabled = IsCardEnabled(cardIndex);
            const bool selected = pressedIndex_ == cardIndex && hotIndex_ == cardIndex;
            const bool hot = hotIndex_ == cardIndex;
            const bool focused = panelFocused && focusedIndex_ == cardIndex;
            const COLORREF fill = !enabled ? RGB(239, 242, 246) :
                (selected ? RGB(224, 234, 248) : (hot ? RGB(235, 240, 246) : Ui::Surface));
            const COLORREF border = isAdd ? RGB(137, 177, 237) :
                (focused || hot ? Ui::BorderStrong : Ui::Border);
            Ui::DrawRoundedRect(dc, rect, fill, border, Ui::Scale(12, fontDpi_),
                                isAdd ? PS_DASH : PS_SOLID);
            SetTextColor(dc, !enabled ? RGB(155, 160, 170) : (isAdd ? Ui::Primary : Ui::Text));
            const wchar_t* text = isAdd ? L"+ 添加按钮" : buttons_[index].name.c_str();
            DrawTextW(dc, text, -1, &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        SelectObject(dc, previousFont);

        if (scrollIndicatorVisible_ && layout_.maximumScroll > 0) {
            const int inset = Ui::Scale(6, fontDpi_);
            const int trackHeight = std::max(1, static_cast<int>(client.bottom) - inset * 2);
            const UiScroll::Metrics metrics{0, std::max(0, layout_.contentHeight - 1),
                                            static_cast<UINT>(std::max(0L, client.bottom)), scrollPosition_};
            const UiScroll::Thumb thumb = UiScroll::CalculateThumb(
                metrics, trackHeight, Ui::Scale(18, fontDpi_));
            if (thumb.visible) {
                const int width = std::max(2, Ui::Scale(4, fontDpi_));
                RECT thumbRect{client.right - inset - width, inset + thumb.top,
                               client.right - inset, inset + thumb.top + thumb.height};
                Ui::DrawRoundedRect(dc, thumbRect, RGB(100, 116, 139), RGB(100, 116, 139), width);
            }
        }

        if (buffer != nullptr) EndBufferedPaint(buffer, TRUE);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    case WM_SIZE:
        Layout();
        return 0;
    case WM_MOUSEWHEEL: {
        const UiScroll::WheelAction action = UiScroll::AccumulateWheel(
            GET_WHEEL_DELTA_WPARAM(wParam), UiScroll::SystemWheelScrollLines(), wheelRemainder_);
        int delta = 0;
        RECT client{};
        GetClientRect(hwnd_, &client);
        if (action.pages != 0) delta = -action.pages * std::max(1, static_cast<int>(client.bottom));
        else delta = -action.lines * Ui::Scale(10, GetDpiForWindow(hwnd_));
        if (delta != 0) ScrollTo(scrollPosition_ + delta, true);
        else if (layout_.maximumScroll > 0) ShowScrollIndicator();
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!trackingMouse_) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd_, 0};
            TrackMouseEvent(&tracking);
            trackingMouse_ = true;
        }
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int next = HitTest(point);
        if (next != hotIndex_) {
            hotIndex_ = next;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        trackingMouse_ = false;
        hotIndex_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int index = HitTest(point);
        if (IsCardEnabled(index)) {
            SetFocus(hwnd_);
            SetCapture(hwnd_);
            pressedIndex_ = index;
            focusedIndex_ = index;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        const int pressed = pressedIndex_;
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int released = HitTest(point);
        pressedIndex_ = -1;
        if (GetCapture() == hwnd_) ReleaseCapture();
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (pressed >= 0 && pressed == released && IsCardEnabled(pressed) && clickCallback_)
            clickCallback_(pressed == static_cast<int>(buttons_.size()) ? -1 : pressed);
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (pressedIndex_ != -1) {
            pressedIndex_ = -1;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_CONTEXTMENU: {
        POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        int index = -1;
        if (screenPoint.x == -1 && screenPoint.y == -1) {
            index = focusedIndex_;
            if (index >= 0 && index < static_cast<int>(layout_.cards.size())) {
                const RECT& rect = layout_.cards[static_cast<std::size_t>(index)];
                screenPoint = POINT{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
                ClientToScreen(hwnd_, &screenPoint);
            }
        } else {
            POINT clientPoint = screenPoint;
            ScreenToClient(hwnd_, &clientPoint);
            index = HitTest(clientPoint);
        }
        if (index >= 0 && index < static_cast<int>(buttons_.size()) && contextCallback_)
            contextCallback_(index, screenPoint);
        return 0;
    }
    case WM_KEYDOWN: {
        if (focusedIndex_ < 0) focusedIndex_ = 0;
        int next = focusedIndex_;
        if (wParam == VK_LEFT) --next;
        else if (wParam == VK_RIGHT) ++next;
        else if (wParam == VK_UP) next -= layout_.columns;
        else if (wParam == VK_DOWN) next += layout_.columns;
        else if ((wParam == VK_RETURN || wParam == VK_SPACE) && IsCardEnabled(focusedIndex_)) {
            if (clickCallback_)
                clickCallback_(focusedIndex_ == static_cast<int>(buttons_.size()) ? -1 : focusedIndex_);
            return 0;
        } else {
            break;
        }
        focusedIndex_ = std::clamp(next, 0, std::max(0, CardCount() - 1));
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_TIMER:
        if (wParam == ScrollHideTimer) {
            HideScrollIndicator();
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
