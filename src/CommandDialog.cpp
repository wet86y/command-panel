#include "CommandDialog.h"

#include "ConfigManager.h"

#include <algorithm>
#include <cwctype>

namespace {
constexpr int IdName = 101;
constexpr int IdCommand = 102;
constexpr int IdConfirm = 103;
constexpr int IdOk = 104;
constexpr int IdCancel = 105;

struct State
{
    HWND window = nullptr;
    HWND owner = nullptr;
    HWND name = nullptr;
    HWND command = nullptr;
    HWND confirm = nullptr;
    CommandButton initial;
    CommandButton result;
    bool accepted = false;
    bool nameOnly = false;
};

void SetControlFont(HWND control)
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

LRESULT CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = reinterpret_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        state->window = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (state == nullptr) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
    case WM_CREATE: {
        state->name = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->initial.name.c_str(),
                                      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 28, 390, 26,
                                      hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdName)), GetModuleHandleW(nullptr), nullptr);
        if (!state->nameOnly) {
            state->command = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->initial.command.c_str(),
                                             WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                                             100, 88, 390, 120, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdCommand)), GetModuleHandleW(nullptr), nullptr);
            state->confirm = CreateWindowExW(0, L"BUTTON", L"执行前再次确认", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                             100, 220, 180, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdConfirm)), GetModuleHandleW(nullptr), nullptr);
            CreateWindowExW(0, L"STATIC", L"命令", WS_CHILD | WS_VISIBLE, 22, 92, 70, 22, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            CheckDlgButton(hwnd, IdConfirm, state->initial.confirm ? BST_CHECKED : BST_UNCHECKED);
        }
        CreateWindowExW(0, L"STATIC", L"名称", WS_CHILD | WS_VISIBLE, 22, state->nameOnly ? 74 : 32, 70, 22, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND ok = CreateWindowExW(0, L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                  state->nameOnly ? 330 : 330, state->nameOnly ? 150 : 250, 76, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdOk)), GetModuleHandleW(nullptr), nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE,
                                      414, state->nameOnly ? 150 : 250, 76, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdCancel)), GetModuleHandleW(nullptr), nullptr);
        SetControlFont(state->name); SetControlFont(state->command); SetControlFont(state->confirm); SetControlFont(ok); SetControlFont(cancel);
        SetFocus(state->name);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IdCancel) { DestroyWindow(hwnd); return 0; }
        if (LOWORD(wParam) == IdOk) {
            wchar_t name[1024]{};
            wchar_t command[8192]{};
            GetWindowTextW(state->name, name, 1024);
            if (!state->nameOnly) GetWindowTextW(state->command, command, 8192);
            std::wstring nameText(name), commandText(command);
            auto notSpace = [](wchar_t ch) { return !iswspace(ch); };
            if (std::find_if(nameText.begin(), nameText.end(), notSpace) == nameText.end() ||
                (!state->nameOnly && std::find_if(commandText.begin(), commandText.end(), notSpace) == commandText.end())) {
                MessageBoxW(hwnd, L"名称和命令不能为空。", L"无法保存", MB_OK | MB_ICONWARNING);
                return 0;
            }
            state->result = state->initial;
            state->result.name = std::move(nameText);
            if (!state->nameOnly) {
                state->result.command = std::move(commandText);
                state->result.confirm = IsDlgButtonChecked(hwnd, IdConfirm) == BST_CHECKED;
            }
            state->accepted = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
}

static bool ShowInternal(HWND owner, const CommandButton& initial, CommandButton& result, bool nameOnly,
                         const wchar_t* titleOverride = nullptr)
{
    static bool registered = false;
    const wchar_t* className = L"CommandPanelCommandDialog";
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = DialogProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        RegisterClassW(&wc);
        registered = true;
    }
    State state;
    state.owner = owner;
    state.initial = initial;
    state.nameOnly = nameOnly;
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, className,
                                  titleOverride != nullptr ? titleOverride : (nameOnly ? L"重命名标签页" : (initial.id.empty() ? L"添加命令" : L"编辑命令")),
                                  WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 520, 320, owner, nullptr,
                                  GetModuleHandleW(nullptr), &state);
    if (window == nullptr) return false;
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    SetWindowPos(window, HWND_TOP, ownerRect.left + (ownerRect.right - ownerRect.left - 520) / 2,
                 ownerRect.top + (ownerRect.bottom - ownerRect.top - 320) / 2, 520, 320, SWP_SHOWWINDOW);
    EnableWindow(owner, FALSE);
    MSG message{};
    while (IsWindow(window) && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    if (state.accepted) result = std::move(state.result);
    return state.accepted;
}

bool CommandDialog::Show(HWND owner, const CommandButton& initial, CommandButton& result)
{
    return ShowInternal(owner, initial, result, false);
}

bool CommandDialog::PromptName(HWND owner, const std::wstring& title, const std::wstring& initial, std::wstring& result)
{
    CommandButton value;
    value.id = "tab-name";
    value.name = initial;
    if (!ShowInternal(owner, value, value, true, title.c_str())) return false;
    result = value.name;
    return true;
}
