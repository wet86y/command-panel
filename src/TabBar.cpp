#include "TabBar.h"
#include "CommandMenu.h"
#include "UiTheme.h"

#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cwctype>

namespace {
constexpr UINT MsgCommitEdit = WM_APP + 20;
constexpr UINT MsgCancelEdit = WM_APP + 21;
}

TabBar::~TabBar()
{
    if (font_ != nullptr) DeleteObject(font_);
}

bool TabBar::Create(HWND parent)
{
    static bool registered = false;
    const wchar_t* className = L"CommandPanelTabBar";
    if (!registered) {
        WNDCLASSW wc{};
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = className;
        RegisterClassW(&wc);
        registered = true;
    }
    hwnd_ = CreateWindowExW(0, className, nullptr, WS_CHILD | WS_VISIBLE,
                            0, 0, 0, 0, parent, nullptr, GetModuleHandleW(nullptr), this);
    fontDpi_ = GetDpiForWindow(hwnd_);
    font_ = Ui::CreateFont(fontDpi_, 10);
    return hwnd_ != nullptr;
}

void TabBar::SetCallbacks(SelectCallback select, AddCallback add, RenameCallback rename, DeleteCallback remove)
{
    selectCallback_ = std::move(select);
    addCallback_ = std::move(add);
    renameCallback_ = std::move(rename);
    deleteCallback_ = std::move(remove);
}

void TabBar::SetTabs(const std::vector<std::wstring>& names, int active)
{
    CancelEdit();
    names_ = names;
    active_ = names_.empty() ? -1 : std::clamp(active, 0, static_cast<int>(names_.size()) - 1);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void TabBar::BeginEdit(int index)
{
    if (index < 0 || index >= static_cast<int>(names_.size()) ||
        index >= static_cast<int>(tabRects_.size())) return;
    CancelEdit();
    RECT rect = tabRects_[index];
    const UINT dpi = GetDpiForWindow(hwnd_);
    InflateRect(&rect, -Ui::Scale(8, dpi), -Ui::Scale(9, dpi));
    edit_ = CreateWindowExW(0, L"EDIT", names_[index].c_str(),
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                            hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (edit_ == nullptr) return;
    editIndex_ = index;
    SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    SetWindowLongPtrW(edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    editOriginalProc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc)));
    SendMessageW(edit_, EM_SETSEL, 0, -1);
    SetFocus(edit_);
}

bool TabBar::CommitEdit()
{
    if (edit_ == nullptr || editIndex_ < 0) return true;
    const int length = GetWindowTextLengthW(edit_);
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(edit_, value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    const auto first = std::find_if_not(value.begin(), value.end(), [](wchar_t ch) { return iswspace(ch); });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](wchar_t ch) { return iswspace(ch); }).base();
    if (first == value.end() || first >= last) { CancelEdit(); return true; }
    value = std::wstring(first, last);
    if (renameCallback_ && !renameCallback_(editIndex_, value)) {
        SetFocus(edit_);
        SendMessageW(edit_, EM_SETSEL, 0, -1);
        return false;
    }
    names_[editIndex_] = std::move(value);
    CancelEdit();
    InvalidateRect(hwnd_, nullptr, TRUE);
    return true;
}

void TabBar::CancelEdit()
{
    if (edit_ == nullptr) { editIndex_ = -1; return; }
    endingEdit_ = true;
    HWND edit = edit_;
    edit_ = nullptr;
    editIndex_ = -1;
    DestroyWindow(edit);
    endingEdit_ = false;
}

RECT TabBar::AddRect() const
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    const UINT dpi = GetDpiForWindow(hwnd_);
    return RECT{client.right - Ui::Scale(68, dpi), Ui::Scale(9, dpi),
                client.right - Ui::Scale(16, dpi), client.bottom - Ui::Scale(9, dpi)};
}

int TabBar::HitTest(POINT point) const
{
    for (size_t i = 0; i < tabRects_.size(); ++i) {
        if (PtInRect(&tabRects_[i], point)) return static_cast<int>(i);
    }
    return -1;
}

LRESULT CALLBACK TabBar::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<TabBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = reinterpret_cast<TabBar*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK TabBar::EditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<TabBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->HandleEditMessage(hwnd, message, wParam, lParam) :
                  DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT TabBar::HandleEditMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN) {
        if (wParam == VK_RETURN) { PostMessageW(hwnd_, MsgCommitEdit, 0, 0); return 0; }
        if (wParam == VK_ESCAPE) { PostMessageW(hwnd_, MsgCancelEdit, 0, 0); return 0; }
    }
    if (message == WM_KILLFOCUS && !endingEdit_)
        PostMessageW(hwnd_, MsgCommitEdit, 0, 0);
    return CallWindowProcW(editOriginalProc_, hwnd, message, wParam, lParam);
}

LRESULT TabBar::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        CancelEdit();
        InvalidateRect(hwnd_, nullptr, TRUE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC targetDc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        HDC dc = nullptr;
        HPAINTBUFFER buffer = BeginBufferedPaint(targetDc, &client, BPBF_COMPATIBLEBITMAP, nullptr, &dc);
        if (buffer == nullptr) dc = targetDc;
        const UINT dpi = GetDpiForWindow(hwnd_);
        if (fontDpi_ != dpi) {
            if (font_ != nullptr) DeleteObject(font_);
            fontDpi_ = dpi;
            font_ = Ui::CreateFont(dpi, 10);
        }
        HBRUSH background = CreateSolidBrush(Ui::Window);
        FillRect(dc, &client, background);
        DeleteObject(background);
        tabRects_.clear();
        int x = Ui::Scale(18, dpi);
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font_));
        SetBkMode(dc, TRANSPARENT);
        for (size_t i = 0; i < names_.size(); ++i) {
            SIZE size{};
            GetTextExtentPoint32W(dc, names_[i].c_str(), static_cast<int>(names_[i].size()), &size);
            const int width = std::clamp(static_cast<int>(size.cx) + Ui::Scale(48, dpi),
                                         Ui::Scale(104, dpi), Ui::Scale(180, dpi));
            RECT rect{x, 0, x + width, client.bottom};
            tabRects_.push_back(rect);
            RECT backgroundRect = rect;
            InflateRect(&backgroundRect, -Ui::Scale(7, dpi), -Ui::Scale(8, dpi));
            if (static_cast<int>(i) == active_)
                Ui::DrawRoundedRect(dc, backgroundRect, RGB(230, 236, 244), RGB(218, 225, 234),
                                    Ui::Scale(8, dpi));
            if (static_cast<int>(i) == hot_ && static_cast<int>(i) != active_) {
                Ui::DrawRoundedRect(dc, backgroundRect, Ui::SurfaceHover, Ui::SurfaceHover, Ui::Scale(8, dpi));
            }
            SetTextColor(dc, i == static_cast<size_t>(active_) ? Ui::Text : RGB(76, 84, 95));
            DrawTextW(dc, names_[i].c_str(), -1, &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            x += width;
        }
        const RECT add = AddRect();
        Ui::DrawRoundedRect(dc, add, hot_ == -1 ? Ui::PrimarySoft : Ui::Surface,
                            hot_ == -1 ? RGB(170, 198, 238) : Ui::Border, Ui::Scale(8, dpi));
        SetTextColor(dc, hot_ == -1 ? Ui::Primary : RGB(75, 82, 92));
        RECT plus = add;
        DrawTextW(dc, L"+", -1, &plus, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        SelectObject(dc, oldFont);
        if (buffer != nullptr) EndBufferedPaint(buffer, TRUE);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const RECT add = AddRect();
        if (PtInRect(&add, point)) {
            if (CommitEdit() && addCallback_) addCallback_();
        } else {
            const int index = HitTest(point);
            if (index >= 0 && CommitEdit() && selectCallback_) selectCallback_(index);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!trackingMouse_) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd_, 0};
            TrackMouseEvent(&tracking);
            trackingMouse_ = true;
        }
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const RECT add = AddRect();
        const int next = PtInRect(&add, point) ? -1 : HitTest(point);
        if (next != hot_) { hot_ = next; InvalidateRect(hwnd_, nullptr, FALSE); }
        return 0;
    }
    case WM_MOUSELEAVE:
        trackingMouse_ = false;
        hot_ = -2;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDBLCLK: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int index = HitTest(point);
        if (index >= 0) BeginEdit(index);
        return 0;
    }
    case WM_CONTEXTMENU: {
        POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        POINT clientPoint = screenPoint;
        if (screenPoint.x == -1 && screenPoint.y == -1) {
            GetCursorPos(&screenPoint);
            clientPoint = screenPoint;
        }
        ScreenToClient(hwnd_, &clientPoint);
        const int index = HitTest(clientPoint);
        if (index < 0) return 0;
        switch (CommandMenu::Show(hwnd_, screenPoint)) {
        case CommandMenuAction::Edit: BeginEdit(index); break;
        case CommandMenuAction::Delete:
            if (deleteCallback_) deleteCallback_(index);
            break;
        case CommandMenuAction::None: break;
        }
        return 0;
    }
    case MsgCommitEdit:
        CommitEdit();
        return 0;
    case MsgCancelEdit:
        CancelEdit();
        return 0;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
