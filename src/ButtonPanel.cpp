#include "ButtonPanel.h"
#include "UiTheme.h"

#include <windowsx.h>

#include <algorithm>

namespace {
constexpr int FirstButtonId = 1000;
constexpr int AddButtonId = 0x7FFF;

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
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = className;
        RegisterClassW(&wc);
        registered = true;
    }
    hwnd_ = CreateWindowExW(0, className, nullptr,
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
                            0, 0, 0, 0, parent, nullptr, GetModuleHandleW(nullptr), this);
    fontDpi_ = GetDpiForWindow(hwnd_);
    font_ = Ui::CreateFont(fontDpi_, 9);
    return hwnd_ != nullptr;
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
    for (HWND control : controls_) DestroyWindow(control);
    controls_.clear();
    enabled_.clear();
    for (size_t i = 0; i < buttons.size(); ++i) {
        HWND control = CreateWindowExW(0, L"BUTTON", buttons[i].name.c_str(),
                                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_TEXT | BS_OWNERDRAW,
                                       0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(FirstButtonId + static_cast<int>(i))),
                                       GetModuleHandleW(nullptr), nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        Ui::TrackOwnerDrawButton(control);
        controls_.push_back(control);
        enabled_.push_back(buttons[i].enabled);
    }
    HWND add = CreateWindowExW(0, L"BUTTON", L"+ 添加按钮", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                               0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(AddButtonId)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(add, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    Ui::TrackOwnerDrawButton(add);
    controls_.push_back(add);
    enabled_.push_back(true);
    scrollPosition_ = 0;
    SetBusy(busy_);
    Layout();
}

void ButtonPanel::SetBusy(bool busy)
{
    busy_ = busy;
    for (size_t i = 0; i < controls_.size(); ++i) {
        const bool isAdd = i + 1 == controls_.size();
        EnableWindow(controls_[i], isAdd ? TRUE : (enabled_[i] && !busy_));
    }
}

void ButtonPanel::SetResizeSuspended(bool suspended)
{
    resizeSuspended_ = suspended;
    if (!resizeSuspended_) Layout();
}

int ButtonPanel::ButtonIndex(HWND button) const
{
    for (size_t i = 0; i < controls_.size(); ++i) if (controls_[i] == button) return static_cast<int>(i);
    return -1;
}

void ButtonPanel::ScrollTo(int position)
{
    SCROLLINFO info{sizeof(info), SIF_ALL};
    GetScrollInfo(hwnd_, SB_VERT, &info);
    const int maxPosition = std::max(0, info.nMax - static_cast<int>(info.nPage) + 1);
    scrollPosition_ = std::clamp(position, 0, maxPosition);
    SetScrollPos(hwnd_, SB_VERT, scrollPosition_, TRUE);
    Layout();
}

void ButtonPanel::Layout()
{
    if (hwnd_ == nullptr || resizeSuspended_) return;
    RECT client{};
    GetClientRect(hwnd_, &client);
    const UINT dpi = GetDpiForWindow(hwnd_);
    if (fontDpi_ != dpi) {
        if (font_ != nullptr) DeleteObject(font_);
        fontDpi_ = dpi;
        font_ = Ui::CreateFont(dpi, 9);
        for (HWND control : controls_)
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
    const int margin = Ui::Scale(12, dpi);
    const int gap = Ui::Scale(14, dpi);
    const int height = Ui::CompactScale(64, dpi);
    const int widthLimit = std::max(100, static_cast<int>(client.right) - margin * 2);
    const int columns = widthLimit >= Ui::Scale(760, dpi)
        ? 4 : std::max(1, (widthLimit + gap) / (Ui::Scale(180, dpi) + gap));
    const int slotWidth = std::max(100, (widthLimit - gap * (columns - 1)) / columns);
    const int cardInset = std::min(Ui::Scale(10, dpi), std::max(0, (slotWidth - 100) / 2));
    const int cardWidth = std::max(100, slotWidth - cardInset * 2);
    HDC dc = GetDC(hwnd_);
    HFONT old = static_cast<HFONT>(SelectObject(dc, font_));
    int x = margin;
    int y = margin - scrollPosition_;
    for (size_t index = 0; index < controls_.size(); ++index) {
        const int column = static_cast<int>(index % columns);
        const int row = static_cast<int>(index / columns);
        x = margin + column * (slotWidth + gap) + (slotWidth - cardWidth) / 2;
        y = margin + row * (height + gap) - scrollPosition_;
        MoveWindow(controls_[index], x, y, cardWidth, height, TRUE);
    }
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);
    int contentHeight = y + height + margin + scrollPosition_;
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    info.nMin = 0;
    info.nMax = std::max(static_cast<int>(client.bottom), contentHeight) - 1;
    info.nPage = static_cast<UINT>(std::max(0, static_cast<int>(client.bottom)));
    info.nPos = scrollPosition_;
    SetScrollInfo(hwnd_, SB_VERT, &info, TRUE);
}

LRESULT CALLBACK ButtonPanel::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<ButtonPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = reinterpret_cast<ButtonPanel*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT ButtonPanel::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        HBRUSH background = CreateSolidBrush(Ui::Window);
        FillRect(dc, &client, background);
        DeleteObject(background);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    case WM_SIZE:
        if (!resizeSuspended_) Layout();
        return 0;
    case WM_VSCROLL: {
        SCROLLINFO info{sizeof(info), SIF_ALL};
        GetScrollInfo(hwnd_, SB_VERT, &info);
        int position = info.nPos;
        switch (LOWORD(wParam)) {
        case SB_LINEUP: --position; break;
        case SB_LINEDOWN: ++position; break;
        case SB_PAGEUP: position -= static_cast<int>(info.nPage); break;
        case SB_PAGEDOWN: position += static_cast<int>(info.nPage); break;
        case SB_THUMBTRACK: position = info.nTrackPos; break;
        default: break;
        }
        ScrollTo(position);
        return 0;
    }
    case WM_MOUSEWHEEL:
        ScrollTo(scrollPosition_ - GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 30);
        return 0;
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED) {
            const int id = LOWORD(wParam);
            if (clickCallback_) clickCallback_(id == AddButtonId ? -1 : id - FirstButtonId);
            return 0;
        }
        break;
    case WM_DRAWITEM: {
        auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item == nullptr || item->CtlType != ODT_BUTTON) break;
        RECT rect = item->rcItem;
        const bool selected = (item->itemState & ODS_SELECTED) != 0;
        const bool disabled = (item->itemState & ODS_DISABLED) != 0;
        const bool hot = Ui::IsControlHot(item->hwndItem);
        const bool focused = (item->itemState & ODS_FOCUS) != 0;
        const bool isAdd = item->CtlID == AddButtonId;
        const COLORREF fill = disabled ? RGB(239, 242, 246) :
            (selected ? RGB(224, 234, 248) : (hot ? RGB(235, 240, 246) : Ui::Surface));
        const COLORREF border = isAdd ? RGB(137, 177, 237) :
            (focused || hot ? Ui::BorderStrong : Ui::Border);
        Ui::DrawRoundedRect(item->hDC, rect, fill, border, 12,
                            isAdd ? PS_DASH : PS_SOLID);
        wchar_t text[512]{};
        GetWindowTextW(item->hwndItem, text, 512);
        SetBkMode(item->hDC, TRANSPARENT);
        SetTextColor(item->hDC, disabled ? RGB(155, 160, 170) : (isAdd ? Ui::Primary : Ui::Text));
        DrawTextW(item->hDC, text, -1, &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        return TRUE;
    }
    case WM_CONTEXTMENU: {
        HWND control = reinterpret_cast<HWND>(wParam);
        const int index = ButtonIndex(control);
        if (index >= 0 && index + 1 < static_cast<int>(controls_.size()) && contextCallback_) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (point.x == -1 && point.y == -1) { GetCursorPos(&point); }
            contextCallback_(index, point);
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
