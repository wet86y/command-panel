#include "TabBar.h"

#include <windowsx.h>

#include <algorithm>

namespace {
HFONT CreateTabFont()
{
    const int dpi = GetDpiForSystem();
    return CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}
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
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        RegisterClassW(&wc);
        registered = true;
    }
    hwnd_ = CreateWindowExW(0, className, nullptr, WS_CHILD | WS_VISIBLE,
                            0, 0, 0, 0, parent, nullptr, GetModuleHandleW(nullptr), this);
    font_ = CreateTabFont();
    return hwnd_ != nullptr;
}

void TabBar::SetCallbacks(SelectCallback select, AddCallback add, RenameCallback rename)
{
    selectCallback_ = std::move(select);
    addCallback_ = std::move(add);
    renameCallback_ = std::move(rename);
}

void TabBar::SetTabs(const std::vector<std::wstring>& names, int active)
{
    names_ = names;
    active_ = names_.empty() ? -1 : std::clamp(active, 0, static_cast<int>(names_.size()) - 1);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

RECT TabBar::AddRect() const
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    return RECT{client.right - 68, 9, client.right - 16, client.bottom - 9};
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

LRESULT TabBar::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        tabRects_.clear();
        int x = 18;
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font_));
        SetBkMode(dc, TRANSPARENT);
        for (size_t i = 0; i < names_.size(); ++i) {
            SIZE size{};
            GetTextExtentPoint32W(dc, names_[i].c_str(), static_cast<int>(names_[i].size()), &size);
            const int width = std::clamp(static_cast<int>(size.cx) + 48, 104, 180);
            RECT rect{x, 0, x + width, client.bottom};
            tabRects_.push_back(rect);
            SetTextColor(dc, i == static_cast<size_t>(active_) ? RGB(25, 31, 40) : RGB(85, 91, 100));
            DrawTextW(dc, names_[i].c_str(), -1, &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            if (i == static_cast<size_t>(active_)) {
                RECT underline{rect.left + 10, client.bottom - 3, rect.right - 10, client.bottom};
                HBRUSH blue = CreateSolidBrush(RGB(20, 110, 235));
                FillRect(dc, &underline, blue);
                DeleteObject(blue);
            }
            x += width;
        }
        const RECT add = AddRect();
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(220, 225, 232));
        HBRUSH brush = CreateSolidBrush(RGB(250, 252, 255));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        RoundRect(dc, add.left, add.top, add.right, add.bottom, 8, 8);
        SelectObject(dc, oldBrush); SelectObject(dc, oldPen);
        DeleteObject(brush); DeleteObject(pen);
        SetTextColor(dc, RGB(75, 82, 92));
        RECT plus = add;
        DrawTextW(dc, L"+", -1, &plus, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        SelectObject(dc, oldFont);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const RECT add = AddRect();
        if (PtInRect(&add, point)) {
            if (addCallback_) addCallback_();
        } else {
            const int index = HitTest(point);
            if (index >= 0 && selectCallback_) selectCallback_(index);
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int index = HitTest(point);
        if (index >= 0 && renameCallback_) renameCallback_(index);
        return 0;
    }
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
