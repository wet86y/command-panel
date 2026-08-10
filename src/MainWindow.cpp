#include "MainWindow.h"

#include "CommandDialog.h"
#include "Utf.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <richedit.h>
#include <windowsx.h>

#include <algorithm>

namespace {
constexpr int IdOutput = 2001;
constexpr int IdInput = 2002;
constexpr int IdStatus = 2003;
constexpr int IdTitle = 2004;
constexpr int IdAdmin = 2005;
constexpr int IdConnection = 2006;
constexpr int IdHint = 2007;
constexpr int IdTerminalTitle = 2008;
constexpr int IdClear = 2009;
constexpr int IdExit = 2010;
constexpr int IdReset = 2011;
constexpr int IdInputPrefix = 2012;
constexpr int IdExecute = 2013;
constexpr int IdMinimize = 2014;
constexpr int IdMaximize = 2015;
constexpr int IdClose = 2016;
constexpr int MenuEdit = 3001;
constexpr int MenuDuplicate = 3002;
constexpr int MenuLeft = 3003;
constexpr int MenuRight = 3004;
constexpr int MenuDelete = 3005;
constexpr COLORREF SurfaceColor = RGB(248, 250, 252);
constexpr COLORREF LineColor = RGB(226, 231, 238);

HFONT CreateFontForDpi(int pointSize, int weight, const wchar_t* family)
{
    const int dpi = GetDpiForSystem();
    return CreateFontW(-MulDiv(pointSize, dpi, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, family);
}

void SetFont(HWND control, HGDIOBJ font)
{
    if (control != nullptr) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void DrawActionButton(const DRAWITEMSTRUCT& item, bool primary)
{
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool closeButton = item.CtlID == IdClose;
    const bool captionButton = item.CtlID == IdMinimize || item.CtlID == IdMaximize || closeButton;
    if (captionButton) {
        const COLORREF fill = closeButton && selected ? RGB(232, 17, 35) : RGB(255, 255, 255);
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(item.hDC, &item.rcItem, brush);
        DeleteObject(brush);
        wchar_t caption[8]{};
        GetWindowTextW(item.hwndItem, caption, 8);
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, closeButton && selected ? RGB(255, 255, 255) : RGB(30, 36, 44));
        DrawTextW(item.hDC, caption, -1, const_cast<RECT*>(&item.rcItem), DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        return;
    }
    COLORREF fill = RGB(255, 255, 255);
    if (disabled) fill = RGB(235, 238, 243);
    else if (closeButton) fill = selected ? RGB(232, 17, 35) : RGB(255, 255, 255);
    else if (primary) fill = selected ? RGB(17, 88, 205) : RGB(25, 111, 235);
    else if (selected) fill = RGB(238, 244, 252);
    const COLORREF borderColor = closeButton ? fill : (primary ? fill : (disabled ? RGB(218, 223, 231) : RGB(215, 221, 230)));
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    HGDIOBJ oldBrush = SelectObject(item.hDC, brush);
    HGDIOBJ oldPen = SelectObject(item.hDC, pen);
    RoundRect(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom, 12, 12);
    SelectObject(item.hDC, oldPen);
    SelectObject(item.hDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    wchar_t text[256]{};
    GetWindowTextW(item.hwndItem, text, 256);
    RECT textRect = item.rcItem;
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, disabled ? RGB(150, 157, 168) :
        (closeButton && selected ? RGB(255, 255, 255) : (primary ? RGB(255, 255, 255) : RGB(28, 36, 48))));
    DrawTextW(item.hDC, text, -1, &textRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

std::wstring NormalizeRichEditText(std::wstring_view text)
{
    std::wstring normalized;
    normalized.reserve(text.size() + 8);
    for (size_t i = 0; i < text.size(); ++i) {
        const wchar_t ch = text[i];
        if (ch == L'\r') {
            normalized += L'\r';
            if (i + 1 >= text.size() || text[i + 1] != L'\n') normalized += L'\n';
        } else if (ch == L'\n') {
            if (i == 0 || text[i - 1] != L'\r') normalized += L'\r';
            normalized += L'\n';
        } else {
            normalized += ch;
        }
    }
    return normalized;
}
}

MainWindow::MainWindow() : executor_(session_)
{
    session_.SetCallbacks(
        [this](uint64_t generation, std::string bytes) { OnTerminalBytes(generation, std::move(bytes)); },
        [this](uint64_t generation) { OnTerminalExit(generation); });
}

MainWindow::~MainWindow()
{
    shuttingDown_ = true;
    session_.Stop();
    if (outputFont_ != nullptr) DeleteObject(outputFont_);
    if (headerFont_ != nullptr) DeleteObject(headerFont_);
    if (sectionFont_ != nullptr) DeleteObject(sectionFont_);
    if (bodyFont_ != nullptr) DeleteObject(bodyFont_);
}

bool MainWindow::Create(HINSTANCE instance)
{
    instance_ = instance;
    static bool registered = false;
    const wchar_t* className = L"CommandPanelMainWindow";
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        RegisterClassExW(&wc);
        registered = true;
    }
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int windowWidth = std::min(1100, static_cast<int>(workArea.right - workArea.left - 40));
    const int windowHeight = std::min(780, static_cast<int>(workArea.bottom - workArea.top - 40));
    const int windowLeft = workArea.left + (workArea.right - workArea.left - windowWidth) / 2;
    const int windowTop = workArea.top + (workArea.bottom - workArea.top - windowHeight) / 2;
    hwnd_ = CreateWindowExW(0, className, L"CommandPanel",
                            WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN,
                            windowLeft, windowTop, windowWidth, windowHeight, nullptr, nullptr, instance_, this);
    if (hwnd_ != nullptr) {
        const BOOL darkMode = FALSE;
        const COLORREF captionColor = RGB(255, 255, 255);
        const COLORREF textColor = RGB(30, 36, 44);
        DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
        DwmSetWindowAttribute(hwnd_, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
        DwmSetWindowAttribute(hwnd_, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));
        const COLORREF borderColor = RGB(255, 255, 255);
        DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
    }
    return hwnd_ != nullptr;
}

bool MainWindow::Initialize()
{
    LoadLibraryW(L"Msftedit.dll");

    title_ = CreateWindowExW(0, L"STATIC", L"▣  CommandPanel", WS_CHILD | WS_VISIBLE | SS_LEFT,
                             20, 0, 240, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdTitle)), instance_, nullptr);
    adminMode_ = CreateWindowExW(0, L"STATIC", L"♢  管理员模式", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 260, 0, 150, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdAdmin)), instance_, nullptr);
    connection_ = CreateWindowExW(0, L"STATIC", L"●  PowerShell 未连接", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   420, 0, 220, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdConnection)), instance_, nullptr);
    tabBar_.Create(hwnd_);
    tabBar_.SetCallbacks([this](int index) { OnTabSelected(index); },
                         [this] { AddTab(); },
                         [this](int index) { RenameTab(index); });
    buttonPanel_.Create(hwnd_);
    buttonPanel_.SetCallbacks([this](int index) { OnButton(index); },
                              [this](int index, POINT point) { OnContext(index, point); });
    hint_ = CreateWindowExW(0, L"STATIC", L"ⓘ  右键按钮可编辑命令", WS_CHILD | WS_VISIBLE | SS_LEFT,
                            20, 0, 260, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdHint)), instance_, nullptr);

    terminalTitle_ = CreateWindowExW(0, L"STATIC", L"PowerShell", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                     20, 0, 220, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdTerminalTitle)), instance_, nullptr);
    clear_ = CreateWindowExW(0, L"BUTTON", L"清空", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             0, 0, 80, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdClear)), instance_, nullptr);
    ctrlC_ = CreateWindowExW(0, L"BUTTON", L"退出", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             0, 0, 80, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdExit)), instance_, nullptr);
    reset_ = CreateWindowExW(0, L"BUTTON", L"重置", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             0, 0, 80, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdReset)), instance_, nullptr);

    output_ = CreateWindowExW(0, MSFTEDIT_CLASS, nullptr,
                              WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                              0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdOutput)), instance_, nullptr);
    inputPrefix_ = CreateWindowExW(0, L"STATIC", L"PS  >", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                   0, 0, 84, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdInputPrefix)), instance_, nullptr);
    input_ = CreateWindowExW(0, L"EDIT", nullptr,
                             WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                             0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdInput)), instance_, nullptr);
    execute_ = CreateWindowExW(0, L"BUTTON", L"执行", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                0, 0, 110, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdExecute)), instance_, nullptr);
    status_ = CreateWindowExW(0, L"STATIC", L"●  已就绪", WS_CHILD | WS_VISIBLE | SS_LEFT,
                              20, 0, 300, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdStatus)), instance_, nullptr);
    if (!title_ || !adminMode_ || !connection_ || !tabBar_.Hwnd() || !buttonPanel_.Hwnd() ||
        !output_ || !input_ || !execute_ || !status_) return false;

    SetWindowTextW(title_, L"CommandPanel");
    SetWindowTextW(adminMode_, L"◈  管理员模式");
    SetWindowTextW(connection_, L"●  PowerShell 已连接");
    SetWindowTextW(hint_, L"ⓘ  右键按钮可编辑命令");
    SetWindowTextW(clear_, L"清空");
    SetWindowTextW(ctrlC_, L"Ctrl+C");
    SetWindowTextW(reset_, L"重置终端");
    SetWindowTextW(inputPrefix_, L"PS  ›");
    SetWindowTextW(execute_, L"执行");
    SetWindowTextW(status_, L"●  已就绪");
    minimize_ = CreateWindowExW(0, L"BUTTON", L"—", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                0, 0, 46, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdMinimize)), instance_, nullptr);
    maximize_ = CreateWindowExW(0, L"BUTTON", L"□", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                0, 0, 46, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdMaximize)), instance_, nullptr);
    close_ = CreateWindowExW(0, L"BUTTON", L"×", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                             0, 0, 46, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdClose)), instance_, nullptr);
    if (!minimize_ || !maximize_ || !close_) return false;
    for (HWND button : {clear_, ctrlC_, reset_, execute_}) {
        SetWindowLongPtrW(button, GWL_STYLE, GetWindowLongPtrW(button, GWL_STYLE) | BS_OWNERDRAW);
    }

    outputFont_ = CreateFontForDpi(10, FW_NORMAL, L"Consolas");
    headerFont_ = CreateFontForDpi(15, FW_BOLD, L"Segoe UI");
    sectionFont_ = CreateFontForDpi(11, FW_BOLD, L"Segoe UI");
    bodyFont_ = CreateFontForDpi(10, FW_NORMAL, L"Segoe UI");
    SetFont(title_, headerFont_);
    SetFont(adminMode_, bodyFont_);
    SetFont(connection_, bodyFont_);
    SetFont(terminalTitle_, sectionFont_);
    SetFont(output_, outputFont_);
    SetFont(input_, bodyFont_);
    SetFont(inputPrefix_, bodyFont_);
    SetFont(execute_, bodyFont_);
    SetFont(status_, bodyFont_);
    SetFont(minimize_, bodyFont_);
    SetFont(maximize_, bodyFont_);
    SetFont(close_, bodyFont_);
    SetFont(clear_, bodyFont_);
    SetFont(ctrlC_, bodyFont_);
    SetFont(reset_, bodyFont_);
    SetFont(hint_, bodyFont_);
    SendMessageW(output_, EM_SETBKGNDCOLOR, 0, RGB(10, 16, 21));
    CHARFORMAT2W format{sizeof(format)};
    format.dwMask = CFM_COLOR;
    format.crTextColor = RGB(226, 232, 240);
    SendMessageW(output_, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
    SendMessageW(output_, EM_SETLIMITTEXT, 4 * 1024 * 1024, 0);

    inputOriginalProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(input_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(InputProc)));
    SetWindowLongPtrW(input_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    if (!config_.Load()) AddDiagnostic(L"[CommandPanel] " + config_.LastError() + L"\r\n");
    RefreshTabs();
    RefreshButtons();
    Layout();
    if (!StartTerminal()) {
        AddDiagnostic(L"[CommandPanel] " + session_.LastError() + L"\r\n");
        SetStatus(L"● 终端启动失败");
    }
    wchar_t systemRoot[MAX_PATH]{};
    const DWORD rootLength = GetEnvironmentVariableW(L"SystemRoot", systemRoot, MAX_PATH);
    std::wstring wslPath(systemRoot, rootLength);
    wslPath += L"\\System32\\wsl.exe";
    if (rootLength == 0 || GetFileAttributesW(wslPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        AddDiagnostic(L"[CommandPanel] 未检测到 wsl.exe，WSL 相关按钮可能无法执行。\r\n");
    }
    SetFocus(input_);
    return true;
}

bool MainWindow::StartTerminal()
{
    parser_.Reset();
    executor_.Reset();
    if (!session_.Start()) {
        SetWindowTextW(connection_, L"●  PowerShell 未连接");
        terminalReady_ = false;
        return false;
    }
    currentGeneration_ = session_.Generation();
    terminalReady_ = true;
    restartAttempts_ = 0;
    SetWindowTextW(connection_, L"●  PowerShell 已连接");
    UpdateBusyState();
    return true;
}

void MainWindow::Layout()
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = static_cast<int>(client.right);
    const int height = static_cast<int>(client.bottom);
    const int dpi = GetDpiForWindow(hwnd_);
    const int headerHeight = MulDiv(56, dpi, 96);
    const int tabsHeight = MulDiv(48, dpi, 96);
    const int inputHeight = MulDiv(62, dpi, 96);
    const int footerHeight = MulDiv(34, dpi, 96);
    const int buttonTop = headerHeight + tabsHeight;
    const int available = std::max(260, height - buttonTop - inputHeight - footerHeight);
    const int buttonHeight = std::clamp(available * 20 / 100, MulDiv(168, dpi, 96), MulDiv(194, dpi, 96));
    const int terminalTop = buttonTop + buttonHeight;
    const int inputTop = height - footerHeight - inputHeight;
    const int terminalHeaderHeight = MulDiv(44, dpi, 96);

    MoveWindow(title_, 52, 8, 190, 40, TRUE);
    MoveWindow(adminMode_, 252, 10, 154, 36, TRUE);
    MoveWindow(connection_, 415, 10, 230, 36, TRUE);
    MoveWindow(minimize_, width - 138, 0, 46, headerHeight, TRUE);
    MoveWindow(maximize_, width - 92, 0, 46, headerHeight, TRUE);
    MoveWindow(close_, width - 46, 0, 46, headerHeight, TRUE);
    MoveWindow(tabBar_.Hwnd(), 0, headerHeight, width, tabsHeight, TRUE);
    MoveWindow(buttonPanel_.Hwnd(), 14, buttonTop + 8, std::max(0, width - 28), buttonHeight - 50, TRUE);
    MoveWindow(hint_, 22, buttonTop + buttonHeight - 34, 300, 24, TRUE);

    MoveWindow(terminalTitle_, 22, terminalTop + 10, 220, 30, TRUE);
    MoveWindow(clear_, width - 300, terminalTop + 9, 82, 30, TRUE);
    MoveWindow(ctrlC_, width - 210, terminalTop + 9, 82, 30, TRUE);
    MoveWindow(reset_, width - 118, terminalTop + 9, 100, 30, TRUE);
    MoveWindow(output_, 14, terminalTop + terminalHeaderHeight, std::max(0, width - 28),
               std::max(80, inputTop - terminalTop - terminalHeaderHeight - 12), TRUE);
    const int outputWidth = std::max(0, width - 28);
    const int outputHeight = std::max(80, inputTop - terminalTop - terminalHeaderHeight - 12);
    SetWindowRgn(output_, CreateRoundRectRgn(0, 0, outputWidth, outputHeight, 16, 16), TRUE);

    const int inputControlHeight = inputHeight - 16;
    MoveWindow(inputPrefix_, 14, inputTop + 8, 84, inputControlHeight, TRUE);
    MoveWindow(input_, 98, inputTop + 8, std::max(120, width - 236), inputControlHeight, TRUE);
    MoveWindow(execute_, width - 128, inputTop + 8, 114, inputControlHeight, TRUE);
    SetWindowRgn(inputPrefix_, CreateRoundRectRgn(0, 0, 84, inputControlHeight, 10, 10), TRUE);
    SetWindowRgn(input_, CreateRoundRectRgn(0, 0, std::max(120, width - 236), inputControlHeight, 10, 10), TRUE);
    MoveWindow(status_, 20, height - footerHeight + 3, 300, footerHeight - 5, TRUE);
    buttonPanel_.Layout();
    if (terminalReady_) {
        const int columns = std::max(40, (width - 34) / std::max(7, MulDiv(8, dpi, 96)));
        const int rows = std::max(10, (inputTop - terminalTop - terminalHeaderHeight) / std::max(14, MulDiv(16, dpi, 96)));
        session_.Resize(static_cast<short>(std::min(columns, 300)), static_cast<short>(std::min(rows, 200)));
    }
}

void MainWindow::RefreshTabs()
{
    if (config_.Tabs().empty()) config_.Tabs() = ConfigManager::DefaultTabs();
    activeTab_ = std::clamp(activeTab_, 0, static_cast<int>(config_.Tabs().size()) - 1);
    std::vector<std::wstring> names;
    for (const auto& tab : config_.Tabs()) names.push_back(tab.name);
    tabBar_.SetTabs(names, activeTab_);
}

void MainWindow::RefreshButtons()
{
    if (config_.Tabs().empty()) return;
    activeTab_ = std::clamp(activeTab_, 0, static_cast<int>(config_.Tabs().size()) - 1);
    buttonPanel_.SetButtons(config_.Tabs()[activeTab_].buttons);
    UpdateBusyState();
}

void MainWindow::OnTabSelected(int index)
{
    if (index < 0 || index >= static_cast<int>(config_.Tabs().size())) return;
    activeTab_ = index;
    RefreshButtons();
}

void MainWindow::AddTab()
{
    CommandTab tab{ConfigManager::NewId(), L"新标签页", {}};
    if (!CommandDialog::PromptName(hwnd_, L"添加标签页", tab.name, tab.name)) return;
    config_.Tabs().push_back(std::move(tab));
    activeTab_ = static_cast<int>(config_.Tabs().size()) - 1;
    SaveConfig();
    RefreshTabs();
    RefreshButtons();
}

void MainWindow::RenameTab(int index)
{
    if (index < 0 || index >= static_cast<int>(config_.Tabs().size())) return;
    std::wstring name = config_.Tabs()[index].name;
    if (!CommandDialog::PromptName(hwnd_, L"重命名标签页", name, name)) return;
    config_.Tabs()[index].name = std::move(name);
    SaveConfig();
    RefreshTabs();
}

void MainWindow::UpdateBusyState()
{
    buttonPanel_.SetBusy(executor_.IsBusy());
    EnableWindow(clear_, TRUE);
    EnableWindow(ctrlC_, terminalReady_);
    EnableWindow(reset_, TRUE);
    EnableWindow(execute_, terminalReady_);
    if (executor_.IsBusy()) SetStatus(L"●  命令执行中");
    else if (terminalReady_) SetStatus(L"●  已就绪");
    else SetStatus(L"●  终端不可用");
}

void MainWindow::SetStatus(const std::wstring& text)
{
    SetWindowTextW(status_, text.c_str());
}

void MainWindow::AddDiagnostic(const std::wstring& text)
{
    AppendOutput(text);
}

void MainWindow::AppendOutput(std::wstring_view text)
{
    if (text.empty() || output_ == nullptr) return;
    const int length = GetWindowTextLengthW(output_);
    SendMessageW(output_, EM_SETSEL, static_cast<WPARAM>(length), static_cast<LPARAM>(length));
    const std::wstring value = NormalizeRichEditText(text);
    SendMessageW(output_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(value.c_str()));
    SendMessageW(output_, EM_SCROLLCARET, 0, 0);
    SendMessageW(output_, EM_SCROLL, SB_BOTTOM, 0);
    const int newLength = GetWindowTextLengthW(output_);
    if (newLength > 4 * 1024 * 1024) {
        SendMessageW(output_, EM_SETSEL, 0, newLength / 10);
        SendMessageW(output_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L""));
    }
}

void MainWindow::ClearOutput()
{
    SendMessageW(output_, EM_SETSEL, 0, -1);
    SendMessageW(output_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L""));
    SetStatus(L"●  输出已清空");
}

void MainWindow::ExitCommand()
{
    if (session_.SendRaw(std::string(1, '\x03'))) SetStatus(L"●  已发送退出当前命令");
}

void MainWindow::OnTerminalBytes(uint64_t generation, std::string bytes)
{
    if (hwnd_ == nullptr) return;
    const std::wstring parsed = parser_.Feed(std::span<const unsigned char>(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()));
    auto* packet = new TerminalOutputPacket{generation, parsed};
    if (!PostMessageW(hwnd_, WM_APP_TERMINAL_OUTPUT, 0, reinterpret_cast<LPARAM>(packet))) delete packet;
}

void MainWindow::OnTerminalExit(uint64_t generation)
{
    if (hwnd_ == nullptr) return;
    auto* packet = new TerminalExitPacket{generation};
    if (!PostMessageW(hwnd_, WM_APP_TERMINAL_EXITED, 0, reinterpret_cast<LPARAM>(packet))) delete packet;
}

void MainWindow::OnButton(int index)
{
    if (index < 0) AddButton();
    else if (!config_.Tabs().empty() && index < static_cast<int>(config_.Tabs()[activeTab_].buttons.size())) ExecuteButton(index);
}

void MainWindow::ExecuteButton(int index)
{
    auto& buttons = config_.Tabs()[activeTab_].buttons;
    const auto& button = buttons[index];
    if (button.confirm) {
        const std::wstring message = L"确认执行？\r\n\r\n按钮：" + button.name + L"\r\n\r\n命令：\r\n" + button.command;
        if (MessageBoxW(hwnd_, message.c_str(), L"确认执行", MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;
    }
    if (!executor_.ExecuteManaged(button.command)) {
        SetStatus(L"●  命令无法发送");
        return;
    }
    AppendOutput(L"\r\nPS> " + button.command + L"\r\n");
    UpdateBusyState();
}

void MainWindow::ExecuteInput()
{
    wchar_t buffer[8192]{};
    GetWindowTextW(input_, buffer, 8192);
    std::wstring command(buffer);
    if (command.empty()) return;
    SetWindowTextW(input_, L"");
    history_.push_back(command);
    historyPosition_ = -1;
    if (executor_.IsBusy()) {
        executor_.SendInteractiveInput(command);
    } else if (executor_.ExecuteManaged(command)) {
        AppendOutput(L"\r\nPS> " + command + L"\r\n");
        UpdateBusyState();
    } else {
        SetStatus(L"●  命令无法发送");
    }
}

void MainWindow::OnContext(int index, POINT point)
{
    if (index >= 0) ShowContextMenu(index, point);
}

void MainWindow::ShowContextMenu(int index, POINT point)
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, MenuEdit, L"编辑");
    AppendMenuW(menu, MF_STRING, MenuDuplicate, L"复制");
    AppendMenuW(menu, MF_STRING, MenuLeft, L"左移");
    AppendMenuW(menu, MF_STRING, MenuRight, L"右移");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuDelete, L"删除");
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    switch (command) {
    case MenuEdit: EditButton(index); break;
    case MenuDuplicate: DuplicateButton(index); break;
    case MenuLeft: MoveButton(index, -1); break;
    case MenuRight: MoveButton(index, 1); break;
    case MenuDelete: DeleteButton(index); break;
    default: break;
    }
}

void MainWindow::AddButton()
{
    if (config_.Tabs().empty()) return;
    CommandButton value;
    value.id = ConfigManager::NewId();
    if (!CommandDialog::Show(hwnd_, value, value)) return;
    config_.Tabs()[activeTab_].buttons.push_back(std::move(value));
    SaveConfig();
    RefreshButtons();
}

void MainWindow::EditButton(int index)
{
    auto& buttons = config_.Tabs()[activeTab_].buttons;
    if (index < 0 || index >= static_cast<int>(buttons.size())) return;
    CommandButton value;
    if (!CommandDialog::Show(hwnd_, buttons[index], value)) return;
    value.id = buttons[index].id;
    value.enabled = buttons[index].enabled;
    buttons[index] = std::move(value);
    SaveConfig();
    RefreshButtons();
}

void MainWindow::DuplicateButton(int index)
{
    auto& buttons = config_.Tabs()[activeTab_].buttons;
    if (index < 0 || index >= static_cast<int>(buttons.size())) return;
    CommandButton copy = buttons[index];
    copy.id = ConfigManager::NewId();
    copy.name += L" - 副本";
    buttons.insert(buttons.begin() + index + 1, std::move(copy));
    SaveConfig();
    RefreshButtons();
}

void MainWindow::DeleteButton(int index)
{
    auto& buttons = config_.Tabs()[activeTab_].buttons;
    if (index < 0 || index >= static_cast<int>(buttons.size())) return;
    const std::wstring message = L"确定删除“" + buttons[index].name + L"”？";
    if (MessageBoxW(hwnd_, message.c_str(), L"删除按钮", MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;
    buttons.erase(buttons.begin() + index);
    SaveConfig();
    RefreshButtons();
}

void MainWindow::MoveButton(int index, int direction)
{
    auto& buttons = config_.Tabs()[activeTab_].buttons;
    const int target = index + direction;
    if (index < 0 || target < 0 || target >= static_cast<int>(buttons.size())) return;
    std::swap(buttons[index], buttons[target]);
    SaveConfig();
    RefreshButtons();
}

bool MainWindow::SaveConfig()
{
    if (config_.Save()) return true;
    MessageBoxW(hwnd_, config_.LastError().c_str(), L"配置保存失败", MB_OK | MB_ICONERROR);
    return false;
}

void MainWindow::ResetTerminal(bool ask)
{
    if (ask && MessageBoxW(hwnd_, L"这会终止当前 PowerShell / ConPTY 会话，但不会退出 CommandPanel。\r\n\r\n是否继续？",
                           L"重置终端", MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;
    terminalReady_ = false;
    executor_.Reset();
    UpdateBusyState();
    session_.Restart();
    if (session_.IsRunning()) {
        parser_.Reset();
        currentGeneration_ = session_.Generation();
        terminalReady_ = true;
        SetWindowTextW(connection_, L"●  PowerShell 已连接");
        AddDiagnostic(L"[CommandPanel] PowerShell 会话已重新建立。\r\n");
    } else {
        AddDiagnostic(L"[CommandPanel] 终端重置失败：" + session_.LastError() + L"\r\n");
        SetWindowTextW(connection_, L"●  PowerShell 未连接");
        SetStatus(L"●  终端重置失败");
    }
    Layout();
    UpdateBusyState();
}

LRESULT MainWindow::HandleInputMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN) {
        if (wParam == VK_RETURN) { ExecuteInput(); return 0; }
        if (wParam == VK_ESCAPE) { SetWindowTextW(hwnd, L""); return 0; }
        if (wParam == VK_UP && !history_.empty()) {
            if (historyPosition_ < 0) historyPosition_ = static_cast<int>(history_.size()) - 1;
            else historyPosition_ = std::max(0, historyPosition_ - 1);
            SetWindowTextW(hwnd, history_[historyPosition_].c_str());
            SendMessageW(hwnd, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
            return 0;
        }
        if (wParam == VK_DOWN && historyPosition_ >= 0) {
            ++historyPosition_;
            if (historyPosition_ >= static_cast<int>(history_.size())) { historyPosition_ = -1; SetWindowTextW(hwnd, L""); }
            else SetWindowTextW(hwnd, history_[historyPosition_].c_str());
            SendMessageW(hwnd, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
            return 0;
        }
    }
    return CallWindowProcW(inputOriginalProc_, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::InputProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->HandleInputMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = reinterpret_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 760;
        info->ptMinTrackSize.y = 540;
        return 0;
    }
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd_, &point);
        RECT client{};
        GetClientRect(hwnd_, &client);
        constexpr int border = 8;
        const bool left = point.x < border;
        const bool right = point.x >= client.right - border;
        const bool top = point.y < border;
        const bool bottom = point.y >= client.bottom - border;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
        if (point.y < MulDiv(56, GetDpiForWindow(hwnd_), 96)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        HBRUSH surface = CreateSolidBrush(SurfaceColor);
        FillRect(dc, &client, surface);
        DeleteObject(surface);
        const int dpi = GetDpiForWindow(hwnd_);
        const int headerHeight = MulDiv(56, dpi, 96);
        const int tabsHeight = MulDiv(48, dpi, 96);
        const int inputHeight = MulDiv(58, dpi, 96);
        const int footerHeight = MulDiv(30, dpi, 96);
        const int buttonTop = headerHeight + tabsHeight;
        const int available = std::max(260, static_cast<int>(client.bottom) - buttonTop - inputHeight - footerHeight);
        const int buttonHeight = std::clamp(available * 20 / 100, MulDiv(168, dpi, 96), MulDiv(194, dpi, 96));
        const int terminalTop = buttonTop + buttonHeight;
        const int inputTop = static_cast<int>(client.bottom) - footerHeight - inputHeight;
        auto fill = [&](int top, int bottom, COLORREF color) {
            RECT rect{0, top, client.right, bottom};
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
        };
        fill(0, headerHeight, RGB(255, 255, 255));
        fill(headerHeight, terminalTop, RGB(255, 255, 255));
        fill(terminalTop, inputTop, RGB(248, 250, 252));
        fill(inputTop, static_cast<int>(client.bottom) - footerHeight, RGB(255, 255, 255));
        fill(static_cast<int>(client.bottom) - footerHeight, client.bottom, RGB(255, 255, 255));
        HPEN line = CreatePen(PS_SOLID, 1, LineColor);
        HGDIOBJ oldPen = SelectObject(dc, line);
        MoveToEx(dc, 0, headerHeight, nullptr); LineTo(dc, client.right, headerHeight);
        MoveToEx(dc, 0, terminalTop, nullptr); LineTo(dc, client.right, terminalTop);
        MoveToEx(dc, 0, inputTop, nullptr); LineTo(dc, client.right, inputTop);
        MoveToEx(dc, 0, static_cast<int>(client.bottom) - footerHeight, nullptr);
        LineTo(dc, client.right, static_cast<int>(client.bottom) - footerHeight);
        SelectObject(dc, oldPen);
        DeleteObject(line);

        auto roundedSurface = [&](RECT rect, COLORREF fillColor, COLORREF borderColor, int radius) {
            HBRUSH brush = CreateSolidBrush(fillColor);
            HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
            HGDIOBJ oldBrush = SelectObject(dc, brush);
            HGDIOBJ old = SelectObject(dc, pen);
            RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
            SelectObject(dc, old);
            SelectObject(dc, oldBrush);
            DeleteObject(pen);
            DeleteObject(brush);
        };
        roundedSurface(RECT{246, 10, 406, headerHeight - 10}, RGB(245, 249, 255), RGB(213, 225, 245), 10);
        roundedSurface(RECT{412, 10, 650, headerHeight - 10}, RGB(243, 252, 247), RGB(207, 235, 219), 10);
        roundedSurface(RECT{20, 16, 42, 38}, RGB(25, 111, 235), RGB(25, 111, 235), 4);
        roundedSurface(RECT{14, inputTop + 8, 98, inputTop + inputHeight - 8}, RGB(255, 255, 255), LineColor, 10);
        roundedSurface(RECT{98, inputTop + 8, client.right - 138, inputTop + inputHeight - 8}, RGB(255, 255, 255), LineColor, 10);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT iconRect{20, 15, 42, 39};
        DrawTextW(dc, L">_", -1, &iconRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        return 1;
    }
    case WM_SIZE:
        Layout(); return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, TRANSPARENT);
        if (control == connection_) SetTextColor(dc, RGB(20, 145, 80));
        else if (control == adminMode_) SetTextColor(dc, RGB(35, 96, 180));
        else if (control == hint_) SetTextColor(dc, RGB(105, 112, 122));
        else if (control == status_) SetTextColor(dc, RGB(22, 145, 82));
        else SetTextColor(dc, RGB(30, 36, 44));
        if (control == adminMode_ || control == connection_) {
            return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
        }
        if (control == terminalTitle_) {
            static HBRUSH terminalSurface = CreateSolidBrush(RGB(248, 250, 252));
            SetBkColor(dc, RGB(248, 250, 252));
            return reinterpret_cast<LRESULT>(terminalSurface);
        }
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(30, 36, 44));
        SetBkColor(dc, RGB(255, 255, 255));
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_DRAWITEM: {
        auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item != nullptr && item->CtlType == ODT_BUTTON) {
            DrawActionButton(*item, item->CtlID == IdExecute);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IdMinimize:
            ShowWindow(hwnd_, SW_MINIMIZE);
            return 0;
        case IdMaximize:
            ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
            return 0;
        case IdClose:
            SendMessageW(hwnd_, WM_CLOSE, 0, 0);
            return 0;
        case IdClear: ClearOutput(); return 0;
        case IdExit: ExitCommand(); return 0;
        case IdReset: ResetTerminal(true); return 0;
        case IdExecute: ExecuteInput(); return 0;
        default: break;
        }
        break;
    case WM_TIMER:
        if (wParam == WM_APP_RESTART_TIMER) {
            KillTimer(hwnd_, WM_APP_RESTART_TIMER);
            if (!shuttingDown_ && restartAttempts_ < 3) {
                ++restartAttempts_;
                if (!StartTerminal()) {
                    AddDiagnostic(L"[CommandPanel] PowerShell 重启失败：" + session_.LastError() + L"\r\n");
                    SetTimer(hwnd_, WM_APP_RESTART_TIMER, 400, nullptr);
                } else {
                    AddDiagnostic(L"[CommandPanel] PowerShell 已重新建立。\r\n");
                }
            } else if (restartAttempts_ >= 3) {
                SetStatus(L"●  终端连续启动失败，请点击“重置”");
            }
            return 0;
        }
        break;
    case WM_APP_TERMINAL_OUTPUT: {
        auto* packet = reinterpret_cast<TerminalOutputPacket*>(lParam);
        if (packet != nullptr) {
            if (packet->generation == currentGeneration_) {
                auto result = executor_.ConsumeOutput(packet->text);
                AppendOutput(result.display);
                if (result.exitCode.has_value()) {
                    SetStatus(*result.exitCode == 0 ? L"●  完成，退出码 0" : L"●  失败，退出码 " + std::to_wstring(*result.exitCode));
                    UpdateBusyState();
                }
            }
            delete packet;
        }
        return 0;
    }
    case WM_APP_TERMINAL_EXITED: {
        auto* packet = reinterpret_cast<TerminalExitPacket*>(lParam);
        if (packet != nullptr) {
            if (packet->generation == currentGeneration_ && !shuttingDown_) {
                terminalReady_ = false;
                executor_.Reset();
                UpdateBusyState();
                SetWindowTextW(connection_, L"●  PowerShell 未连接");
                AddDiagnostic(L"[CommandPanel] PowerShell 已退出，正在重新建立会话……\r\n");
                if (restartAttempts_ < 3) SetTimer(hwnd_, WM_APP_RESTART_TIMER, 400, nullptr);
            }
            delete packet;
        }
        return 0;
    }
    case WM_CLOSE:
        shuttingDown_ = true;
        EnableWindow(buttonPanel_.Hwnd(), FALSE);
        session_.Stop();
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
