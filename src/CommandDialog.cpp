#include "CommandDialog.h"

#include "UiTheme.h"

#include <windowsx.h>

#include <algorithm>
#include <cwctype>

namespace {
constexpr int IdName = 101;
constexpr int IdCommand = 102;
constexpr int IdConfirm = 103;
constexpr int IdPowerShell = 104;
constexpr int IdWsl = 105;
constexpr int IdClose = 106;

enum class DialogMode { Editor, Name, Confirm };

struct State
{
    HWND window = nullptr;
    HWND owner = nullptr;
    HWND name = nullptr;
    HWND command = nullptr;
    HWND confirm = nullptr;
    HWND terminalLabel = nullptr;
    HWND powerShell = nullptr;
    HWND wsl = nullptr;
    HWND nameLabel = nullptr;
    HWND commandLabel = nullptr;
    HWND message = nullptr;
    HWND error = nullptr;
    HWND ok = nullptr;
    HWND cancel = nullptr;
    HWND close = nullptr;
    HFONT bodyFont = nullptr;
    HFONT labelFont = nullptr;
    HFONT titleFont = nullptr;
    HFONT monoFont = nullptr;
    UINT dpi = 96;
    DialogMode mode = DialogMode::Editor;
    CommandButton initial;
    CommandButton result;
    TerminalKind selectedTerminal = TerminalKind::PowerShell;
    bool requireConfirmation{};
    bool commandPreview{};
    std::wstring title;
    std::wstring messageText;
    std::wstring buttonName;
    std::wstring commandText;
    std::wstring primaryText = L"保存";
    bool accepted = false;
};

int S(const State& state, int value) { return Ui::Scale(value, state.dpi); }

void SetFont(HWND control, HFONT font)
{
    if (control != nullptr) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void RecreateFonts(State& state)
{
    for (HFONT* font : {&state.bodyFont, &state.labelFont, &state.titleFont, &state.monoFont}) {
        if (*font != nullptr) { DeleteObject(*font); *font = nullptr; }
    }
    state.bodyFont = Ui::CreateFont(state.dpi, 10);
    state.labelFont = Ui::CreateFont(state.dpi, 10, FW_SEMIBOLD);
    state.titleFont = Ui::CreateFont(state.dpi, 11, FW_SEMIBOLD);
    state.monoFont = Ui::CreateFont(state.dpi, 10, FW_NORMAL, L"Consolas");
    SetFont(state.name, state.bodyFont);
    SetFont(state.command, state.monoFont);
    SetFont(state.confirm, state.bodyFont);
    SetFont(state.terminalLabel, state.labelFont);
    SetFont(state.powerShell, state.bodyFont);
    SetFont(state.wsl, state.bodyFont);
    SetFont(state.nameLabel, state.labelFont);
    SetFont(state.commandLabel, state.labelFont);
    SetFont(state.message, state.bodyFont);
    SetFont(state.error, state.bodyFont);
    SetFont(state.ok, state.bodyFont);
    SetFont(state.cancel, state.bodyFont);
}

SIZE DialogSize(const State& state)
{
    if (state.mode == DialogMode::Editor) return SIZE{S(state, 560), S(state, 490)};
    if (state.mode == DialogMode::Name) return SIZE{S(state, 520), S(state, 250)};
    if (state.commandPreview) return SIZE{S(state, 520), S(state, 240)};
    return SIZE{S(state, 520), S(state, 280)};
}

void Layout(State& state)
{
    RECT client{};
    GetClientRect(state.window, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int margin = S(state, 28);
    const int buttonWidth = S(state, 92);
    const int buttonHeight = S(state, 36);
    const int buttonGap = S(state, 10);
    const int footerTop = state.mode == DialogMode::Editor ? S(state, 416) :
                          state.mode == DialogMode::Name ? S(state, 176) :
                          state.commandPreview ? S(state, 170) : S(state, 194);
    const int buttonY = footerTop + (height - footerTop - buttonHeight) / 2;

    MoveWindow(state.close, width - S(state, 48), 0, S(state, 48), S(state, 56), TRUE);
    MoveWindow(state.ok, width - margin - buttonWidth, buttonY, buttonWidth, buttonHeight, TRUE);
    MoveWindow(state.cancel, width - margin - buttonWidth * 2 - buttonGap, buttonY,
               buttonWidth, buttonHeight, TRUE);

    if (state.mode == DialogMode::Editor) {
        MoveWindow(state.nameLabel, margin, S(state, 75), width - margin * 2, S(state, 22), TRUE);
        MoveWindow(state.name, margin + S(state, 12), S(state, 105), width - margin * 2 - S(state, 24), S(state, 24), TRUE);
        MoveWindow(state.commandLabel, margin, S(state, 154), width - margin * 2, S(state, 22), TRUE);
        MoveWindow(state.command, margin + S(state, 12), S(state, 184), width - margin * 2 - S(state, 24), S(state, 105), TRUE);
        MoveWindow(state.terminalLabel, margin, S(state, 306), S(state, 150), S(state, 22), TRUE);
        const int terminalWidth = S(state, 135);
        MoveWindow(state.powerShell, margin, S(state, 334), terminalWidth, S(state, 34), TRUE);
        MoveWindow(state.wsl, margin + terminalWidth, S(state, 334), terminalWidth, S(state, 34), TRUE);
        MoveWindow(state.confirm, margin, S(state, 376), S(state, 220), S(state, 28), TRUE);
        MoveWindow(state.error, margin, S(state, 398), width - margin * 2, S(state, 20), TRUE);
    } else if (state.mode == DialogMode::Name) {
        MoveWindow(state.nameLabel, margin, S(state, 76), width - margin * 2, S(state, 22), TRUE);
        MoveWindow(state.name, margin + S(state, 12), S(state, 107), width - margin * 2 - S(state, 24), S(state, 24), TRUE);
        MoveWindow(state.error, margin, S(state, 148), width - margin * 2, S(state, 20), TRUE);
    } else {
        MoveWindow(state.message, margin, S(state, 78), width - margin * 2, S(state, 96), TRUE);
    }
    Ui::ApplyRoundedRegion(state.window, width, height, S(state, 12));
}

void SetError(State& state, const wchar_t* text)
{
    SetWindowTextW(state.error, text);
    ShowWindow(state.error, text != nullptr && *text != L'\0' ? SW_SHOW : SW_HIDE);
}

std::wstring SingleLinePreview(std::wstring_view text)
{
    std::wstring preview;
    preview.reserve(text.size());
    bool previousSpace{};
    for (const wchar_t character : text) {
        const bool whitespace = character == L'\r' || character == L'\n' || character == L'\t';
        if (whitespace) {
            if (!previousSpace) preview.push_back(L' ');
            previousSpace = true;
        } else {
            preview.push_back(character);
            previousSpace = character == L' ';
        }
    }
    return preview;
}

void DrawCommandConfirmation(const State& state, HDC dc, const RECT& client)
{
    const int margin = S(state, 28);
    HGDIOBJ oldBody = SelectObject(dc, state.bodyFont);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, Ui::Text);
    RECT prompt{margin, S(state, 76), client.right - margin, S(state, 98)};
    DrawTextW(dc, L"确认执行？", -1, &prompt, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    RECT button{margin, S(state, 103), client.right - margin, S(state, 125)};
    const std::wstring buttonText = L"按钮：" + state.buttonName;
    DrawTextW(dc, buttonText.c_str(), -1, &button, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    RECT command{margin, S(state, 130), client.right - margin, S(state, 152)};
    const std::wstring commandText = L"命令：" + SingleLinePreview(state.commandText);
    DrawTextW(dc, commandText.c_str(), -1, &command,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, oldBody);
}

bool HasNonSpace(const std::wstring& value)
{
    return std::find_if(value.begin(), value.end(), [](wchar_t ch) { return !iswspace(ch); }) != value.end();
}

void Accept(State& state)
{
    if (state.mode == DialogMode::Confirm) {
        state.accepted = true;
        DestroyWindow(state.window);
        return;
    }
    wchar_t name[1024]{};
    wchar_t command[8192]{};
    GetWindowTextW(state.name, name, 1024);
    if (state.mode == DialogMode::Editor) GetWindowTextW(state.command, command, 8192);
    std::wstring nameText(name), commandText(command);
    if (!HasNonSpace(nameText)) {
        SetError(state, L"请输入名称。");
        SetFocus(state.name);
        return;
    }
    if (state.mode == DialogMode::Editor && !HasNonSpace(commandText)) {
        SetError(state, L"请输入要执行的命令。");
        SetFocus(state.command);
        return;
    }
    state.result = state.initial;
    state.result.name = std::move(nameText);
    if (state.mode == DialogMode::Editor) {
        state.result.command = std::move(commandText);
        state.result.confirm = state.requireConfirmation;
        state.result.terminal = state.selectedTerminal;
    }
    state.accepted = true;
    DestroyWindow(state.window);
}

void DrawButton(const State& state, const DRAWITEMSTRUCT& item)
{
    RECT rect{};
    GetClientRect(item.hwndItem, &rect);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool hot = Ui::IsControlHot(item.hwndItem);
    if (item.CtlID == IdClose) {
        const COLORREF fill = pressed ? RGB(218, 36, 49) : (hot ? RGB(232, 54, 67) : Ui::Window);
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(item.hDC, &rect, brush);
        DeleteObject(brush);
        const int cx = (rect.left + rect.right) / 2;
        const int cy = (rect.top + rect.bottom) / 2;
        const int arm = S(state, 5);
        HPEN pen = CreatePen(PS_SOLID, S(state, 1), hot || pressed ? RGB(255,255,255) : Ui::TextMuted);
        HGDIOBJ old = SelectObject(item.hDC, pen);
        MoveToEx(item.hDC, cx - arm, cy - arm, nullptr); LineTo(item.hDC, cx + arm + 1, cy + arm + 1);
        MoveToEx(item.hDC, cx + arm, cy - arm, nullptr); LineTo(item.hDC, cx - arm - 1, cy + arm + 1);
        SelectObject(item.hDC, old); DeleteObject(pen);
        return;
    }
    if (item.CtlID == IdPowerShell || item.CtlID == IdWsl) {
        const TerminalKind kind = item.CtlID == IdWsl ? TerminalKind::Wsl : TerminalKind::PowerShell;
        const Ui::SelectableVisual visual{!disabled, state.selectedTerminal == kind, pressed, hot,
                                          (item.itemState & ODS_FOCUS) != 0, state.dpi};
        const COLORREF selectedColor = kind == TerminalKind::Wsl ? Ui::Success : Ui::Primary;
        wchar_t text[64]{}; GetWindowTextW(item.hwndItem, text, 64);
        Ui::DrawSelectableOption(item.hDC, rect, text, visual, selectedColor);
        return;
    }
    if (item.CtlID == IdConfirm) {
        wchar_t text[128]{}; GetWindowTextW(item.hwndItem, text, 128);
        Ui::DrawSelectableCheckBox(item.hDC, rect, text,
            Ui::SelectableVisual{!disabled, state.requireConfirmation, pressed, hot,
                                 (item.itemState & ODS_FOCUS) != 0, state.dpi});
        return;
    }
    const bool primary = item.CtlID == IDOK;
    const bool dangerPrimary = primary && state.primaryText == L"删除";
    const COLORREF primaryColor = dangerPrimary ? Ui::Danger : Ui::Primary;
    COLORREF fill = primary ? primaryColor : Ui::Window;
    COLORREF border = primary ? primaryColor : Ui::Border;
    if (disabled) { fill = RGB(238,241,245); border = Ui::Border; }
    else if (pressed) fill = primary ? (dangerPrimary ? RGB(178, 30, 42) : Ui::PrimaryPressed) : RGB(232, 237, 244);
    else if (hot) { fill = primary ? (dangerPrimary ? RGB(224, 54, 67) : RGB(40, 125, 240)) : Ui::SurfaceHover; border = primary ? fill : Ui::BorderStrong; }
    Ui::DrawRoundedRect(item.hDC, rect, fill, border, S(state, 8));
    wchar_t text[128]{}; GetWindowTextW(item.hwndItem, text, 128);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, disabled ? RGB(150,157,168) : (primary ? RGB(255,255,255) : Ui::Text));
    DrawTextW(item.hDC, text, -1, &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

LRESULT CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = reinterpret_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        state->window = hwnd;
        state->dpi = GetDpiForWindow(hwnd);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (state == nullptr) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
    case WM_CREATE: {
        state->close = CreateWindowExW(0, L"BUTTON", nullptr, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                       0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdClose)), GetModuleHandleW(nullptr), nullptr);
        state->ok = CreateWindowExW(0, L"BUTTON", state->primaryText.c_str(),
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
                                    0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), GetModuleHandleW(nullptr), nullptr);
        state->cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), GetModuleHandleW(nullptr), nullptr);
        Ui::TrackOwnerDrawButton(state->close); Ui::TrackOwnerDrawButton(state->ok); Ui::TrackOwnerDrawButton(state->cancel);
        if (state->mode == DialogMode::Confirm && !state->commandPreview) {
            state->message = CreateWindowExW(0, L"STATIC", state->messageText.c_str(),
                                             WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        } else {
            state->nameLabel = CreateWindowExW(0, L"STATIC", L"名称", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                               0, 0, 0, 0, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            state->name = CreateWindowExW(0, L"EDIT", state->initial.name.c_str(),
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdName)), GetModuleHandleW(nullptr), nullptr);
            state->error = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | SS_LEFT,
                                           0, 0, 0, 0, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (state->mode == DialogMode::Editor) {
                state->commandLabel = CreateWindowExW(0, L"STATIC", L"命令", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                                      0, 0, 0, 0, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
                state->command = CreateWindowExW(0, L"EDIT", state->initial.command.c_str(),
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL,
                                                 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdCommand)), GetModuleHandleW(nullptr), nullptr);
                state->confirm = CreateWindowExW(0, L"BUTTON", L"执行前再次确认",
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdConfirm)), GetModuleHandleW(nullptr), nullptr);
                state->requireConfirmation = state->initial.confirm;
                Ui::TrackOwnerDrawButton(state->confirm);
                state->terminalLabel = CreateWindowExW(0, L"STATIC", L"执行终端", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                                        0, 0, 0, 0, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
                state->powerShell = CreateWindowExW(0, L"BUTTON", L"PowerShell",
                                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                     0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdPowerShell)), GetModuleHandleW(nullptr), nullptr);
                state->wsl = CreateWindowExW(0, L"BUTTON", L"WSL",
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                              0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdWsl)), GetModuleHandleW(nullptr), nullptr);
                state->selectedTerminal = state->initial.terminal;
                Ui::TrackOwnerDrawButton(state->powerShell); Ui::TrackOwnerDrawButton(state->wsl);
            }
            SendMessageW(state->name, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(*state, 2), S(*state, 2)));
        }
        RecreateFonts(*state);
        Layout(*state);
        Ui::EnableRoundedCorners(hwnd);
        SetFocus(state->mode == DialogMode::Confirm ? state->cancel : state->name);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT client{}; GetClientRect(hwnd, &client);
        HBRUSH white = CreateSolidBrush(Ui::Window); FillRect(dc, &client, white); DeleteObject(white);
        const int footerTop = state->mode == DialogMode::Editor ? S(*state, 416) :
                              state->mode == DialogMode::Name ? S(*state, 176) :
                              state->commandPreview ? S(*state, 170) : S(*state, 194);
        RECT footer{0, footerTop, client.right, client.bottom};
        HBRUSH footerBrush = CreateSolidBrush(Ui::Surface); FillRect(dc, &footer, footerBrush); DeleteObject(footerBrush);
        HPEN line = CreatePen(PS_SOLID, 1, Ui::Border); HGDIOBJ oldPen = SelectObject(dc, line);
        MoveToEx(dc, 0, S(*state, 56), nullptr); LineTo(dc, client.right, S(*state, 56));
        MoveToEx(dc, 0, footerTop, nullptr); LineTo(dc, client.right, footerTop);
        SelectObject(dc, oldPen); DeleteObject(line);
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, state->titleFont));
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, Ui::Text);
        RECT titleRect{S(*state, 24), 0, client.right - S(*state, 56), S(*state, 56)};
        DrawTextW(dc, state->title.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(dc, oldFont);
        if (state->mode == DialogMode::Confirm && state->commandPreview) {
            DrawCommandConfirmation(*state, dc, client);
        } else if (state->mode != DialogMode::Confirm) {
            RECT nameBox{S(*state, 28), S(*state, 99), client.right - S(*state, 28), S(*state, 139)};
            Ui::DrawRoundedRect(dc, nameBox, Ui::Window, GetFocus() == state->name ? Ui::Primary : Ui::BorderStrong, S(*state, 8));
            if (state->mode == DialogMode::Editor) {
                RECT commandBox{S(*state, 28), S(*state, 178), client.right - S(*state, 28), S(*state, 300)};
                Ui::DrawRoundedRect(dc, commandBox, Ui::Window, GetFocus() == state->command ? Ui::Primary : Ui::BorderStrong, S(*state, 8));
            }
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; ScreenToClient(hwnd, &point);
        if (point.y < S(*state, 56)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam); HWND control = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, OPAQUE); SetBkColor(dc, Ui::Window);
        SetTextColor(dc, control == state->error ? Ui::Danger : Ui::Text);
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam); SetBkMode(dc, OPAQUE);
        SetBkColor(dc, Ui::Window); SetTextColor(dc, Ui::Text);
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_DRAWITEM:
        if (auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam); item != nullptr && item->CtlType == ODT_BUTTON) {
            DrawButton(*state, *item); return TRUE;
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) { Accept(*state); return 0; }
        if (LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == IdClose) { DestroyWindow(hwnd); return 0; }
        if (LOWORD(wParam) == IdConfirm && HIWORD(wParam) == BN_CLICKED) {
            if (Ui::ToggleSelectable(state->requireConfirmation, IsWindowEnabled(state->confirm) != FALSE))
                InvalidateRect(state->confirm, nullptr, FALSE);
            return 0;
        }
        if ((LOWORD(wParam) == IdPowerShell || LOWORD(wParam) == IdWsl) && HIWORD(wParam) == BN_CLICKED) {
            state->selectedTerminal = LOWORD(wParam) == IdWsl ? TerminalKind::Wsl : TerminalKind::PowerShell;
            InvalidateRect(state->powerShell, nullptr, FALSE);
            InvalidateRect(state->wsl, nullptr, FALSE);
            return 0;
        }
        if ((LOWORD(wParam) == IdName || LOWORD(wParam) == IdCommand) && HIWORD(wParam) == EN_CHANGE) {
            SetError(*state, L""); InvalidateRect(hwnd, nullptr, FALSE);
        }
        if ((LOWORD(wParam) == IdName || LOWORD(wParam) == IdCommand) &&
            (HIWORD(wParam) == EN_SETFOCUS || HIWORD(wParam) == EN_KILLFOCUS))
            InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_DPICHANGED: {
        state->dpi = HIWORD(wParam);
        RecreateFonts(*state);
        const auto* suggested = reinterpret_cast<RECT*>(lParam);
        const SIZE size = DialogSize(*state);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, size.cx, size.cy,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        Layout(*state); InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd); return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ShowInternal(State& state)
{
    static bool registered = false;
    const wchar_t* className = L"CommandPanelModernDialog";
    if (!registered) {
        WNDCLASSW wc{};
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = DialogProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        registered = true;
    }
    state.dpi = GetDpiForWindow(state.owner);
    const SIZE size = DialogSize(state);
    RECT ownerRect{}; GetWindowRect(state.owner, &ownerRect);
    int x = ownerRect.left + (ownerRect.right - ownerRect.left - size.cx) / 2;
    int y = ownerRect.top + (ownerRect.bottom - ownerRect.top - size.cy) / 2;
    HMONITOR monitor = MonitorFromWindow(state.owner, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)}; GetMonitorInfoW(monitor, &info);
    x = std::clamp(x, static_cast<int>(info.rcWork.left),
                   static_cast<int>(info.rcWork.right) - static_cast<int>(size.cx));
    y = std::clamp(y, static_cast<int>(info.rcWork.top),
                   static_cast<int>(info.rcWork.bottom) - static_cast<int>(size.cy));
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, className, state.title.c_str(),
                                  WS_POPUP | WS_SYSMENU | WS_CLIPCHILDREN,
                                  x, y, size.cx, size.cy, state.owner, nullptr,
                                  GetModuleHandleW(nullptr), &state);
    if (window == nullptr) return false;
    EnableWindow(state.owner, FALSE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    MSG message{};
    while (IsWindow(window) && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    EnableWindow(state.owner, TRUE);
    SetActiveWindow(state.owner);
    for (HFONT font : {state.bodyFont, state.labelFont, state.titleFont, state.monoFont})
        if (font != nullptr) DeleteObject(font);
    return state.accepted;
}
}

bool CommandDialog::Show(HWND owner, const CommandButton& initial, CommandButton& result)
{
    State state;
    state.owner = owner;
    state.mode = DialogMode::Editor;
    state.initial = initial;
    state.title = initial.id.empty() ? L"添加命令" : L"编辑命令";
    state.primaryText = L"保存";
    if (!ShowInternal(state)) return false;
    result = std::move(state.result);
    return true;
}

bool CommandDialog::PromptName(HWND owner, const std::wstring& title, const std::wstring& initial, std::wstring& result)
{
    State state;
    state.owner = owner;
    state.mode = DialogMode::Name;
    state.title = title;
    state.initial.id = "tab-name";
    state.initial.name = initial;
    state.primaryText = L"保存";
    if (!ShowInternal(state)) return false;
    result = std::move(state.result.name);
    return true;
}

bool CommandDialog::Confirm(HWND owner, const std::wstring& title, const std::wstring& message,
                            const std::wstring& primaryText)
{
    State state;
    state.owner = owner;
    state.mode = DialogMode::Confirm;
    state.title = title;
    state.messageText = message;
    state.primaryText = primaryText;
    return ShowInternal(state);
}

bool CommandDialog::ConfirmCommand(HWND owner, const std::wstring& buttonName, const std::wstring& command)
{
    State state;
    state.owner = owner;
    state.mode = DialogMode::Confirm;
    state.commandPreview = true;
    state.title = L"确认执行";
    state.buttonName = buttonName;
    state.commandText = command;
    state.primaryText = L"执行";
    return ShowInternal(state);
}
