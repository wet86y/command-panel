#include "CommandMenu.h"

#include "UiTheme.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>

namespace {
struct MenuState
{
    HWND window = nullptr;
    HWND owner = nullptr;
    HFONT font = nullptr;
    UINT dpi = 96;
    int hot = -1;
    bool tracking = false;
    CommandMenuAction result = CommandMenuAction::None;
};

int RowAt(const MenuState& state, POINT point)
{
    const int padding = Ui::Scale(6, state.dpi);
    const int rowHeight = Ui::Scale(40, state.dpi);
    if (point.x < padding || point.y < padding) return -1;
    RECT client{};
    GetClientRect(state.window, &client);
    if (point.x >= client.right - padding || point.y >= client.bottom - padding) return -1;
    const int row = (point.y - padding) / rowHeight;
    return row >= 0 && row < 2 ? row : -1;
}

void CloseWith(MenuState& state, CommandMenuAction result)
{
    state.result = result;
    if (GetCapture() == state.window) ReleaseCapture();
    DestroyWindow(state.window);
}

LRESULT CALLBACK MenuProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<MenuState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = reinterpret_cast<MenuState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        state->window = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (state == nullptr) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT client{};
        GetClientRect(hwnd, &client);
        RECT frame = client;
        frame.right -= 1; frame.bottom -= 1;
        Ui::DrawRoundedRect(dc, frame, Ui::Surface, Ui::BorderStrong,
                            Ui::Scale(10, state->dpi));
        const int padding = Ui::Scale(6, state->dpi);
        const int rowHeight = Ui::Scale(40, state->dpi);
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, state->font));
        SetBkMode(dc, TRANSPARENT);
        const wchar_t* labels[] = {L"编辑", L"删除"};
        for (int row = 0; row < 2; ++row) {
            RECT rect{padding, padding + row * rowHeight, client.right - padding,
                      padding + (row + 1) * rowHeight};
            if (state->hot == row)
                Ui::DrawRoundedRect(dc, rect, row == 1 ? RGB(248, 226, 229) : RGB(226, 234, 244),
                                    row == 1 ? RGB(248, 226, 229) : RGB(226, 234, 244),
                                    Ui::Scale(7, state->dpi));
            RECT textRect = rect;
            textRect.left += Ui::Scale(14, state->dpi);
            SetTextColor(dc, row == 1 ? Ui::Danger : Ui::Text);
            DrawTextW(dc, labels[row], -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        }
        SelectObject(dc, oldFont);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!state->tracking) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tracking);
            state->tracking = true;
        }
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int row = RowAt(*state, point);
        if (row != state->hot) { state->hot = row; InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;
    }
    case WM_MOUSELEAVE:
        state->tracking = false;
        state->hot = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int row = RowAt(*state, point);
        if (row >= 0) CloseWith(*state, row == 0 ? CommandMenuAction::Edit : CommandMenuAction::Delete);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (RowAt(*state, point) < 0) CloseWith(*state, CommandMenuAction::None);
        return 0;
    }
    case WM_CANCELMODE:
        CloseWith(*state, CommandMenuAction::None);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && IsWindowVisible(hwnd))
            CloseWith(*state, CommandMenuAction::None);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) CloseWith(*state, CommandMenuAction::None);
        else if (wParam == VK_UP || wParam == VK_DOWN) {
            state->hot = state->hot < 0 ? (wParam == VK_UP ? 1 : 0) : 1 - state->hot;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wParam == VK_RETURN && state->hot >= 0) {
            CloseWith(*state, state->hot == 0 ? CommandMenuAction::Edit : CommandMenuAction::Delete);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
}

CommandMenuAction CommandMenu::Show(HWND owner, POINT screenPoint)
{
    static bool registered = false;
    const wchar_t* className = L"CommandPanelContextMenu";
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = MenuProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return CommandMenuAction::None;
        registered = true;
    }

    MenuState state;
    state.owner = owner;
    state.dpi = GetDpiForWindow(owner);
    state.font = Ui::CreateFont(state.dpi, 10);
    const int width = Ui::Scale(156, state.dpi);
    const int height = Ui::Scale(92, state.dpi);
    HMONITOR monitor = MonitorFromPoint(screenPoint, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    screenPoint.x = std::clamp(screenPoint.x, info.rcWork.left, info.rcWork.right - width);
    screenPoint.y = std::clamp(screenPoint.y, info.rcWork.top, info.rcWork.bottom - height);
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, className, nullptr,
                                  WS_POPUP, screenPoint.x, screenPoint.y, width, height,
                                  owner, nullptr, GetModuleHandleW(nullptr), &state);
    if (window == nullptr) { DeleteObject(state.font); return CommandMenuAction::None; }
    const DWMNCRENDERINGPOLICY nonClientPolicy = DWMNCRP_DISABLED;
    DwmSetWindowAttribute(window, DWMWA_NCRENDERING_POLICY, &nonClientPolicy, sizeof(nonClientPolicy));
    Ui::EnableRoundedCorners(window);
    const COLORREF borderColor = 0xFFFFFFFE;
    DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
    Ui::ApplyRoundedRegion(window, width, height, Ui::Scale(10, state.dpi));
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
    SetFocus(window);
    SetCapture(window);

    MSG message{};
    while (IsWindow(window) && GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    DeleteObject(state.font);
    SetForegroundWindow(owner);
    return state.result;
}
