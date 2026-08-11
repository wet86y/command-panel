#include "MainWindow.h"

#include "CommandDialog.h"
#include "CommandMenu.h"
#include "UiScroll.h"
#include "UiTheme.h"
#include "Utf.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cwctype>
#include <iterator>

namespace {
constexpr int IdInput = 2002;
constexpr int IdRuntimeState = 2003;
constexpr int IdTitle = 2004;
constexpr int IdAdmin = 2005;
constexpr int IdConnection = 2006;
constexpr int IdTerminalTitle = 2008;
constexpr int IdClear = 2009;
constexpr int IdExit = 2010;
constexpr int IdReset = 2011;
constexpr int IdInputPrefix = 2012;
constexpr int IdExecute = 2013;
constexpr int IdMinimize = 2014;
constexpr int IdMaximize = 2015;
constexpr int IdClose = 2016;
constexpr int IdOutputScroll = 2017;
constexpr int IdInputScroll = 2018;
constexpr int IdOutputHost = 2019;
constexpr int IdInputHost = 2020;
constexpr int IdTerminalPowerShell = 2021;
constexpr int IdTerminalWsl = 2022;
constexpr int IdAppIcon = 101;
constexpr UINT MsgBeginWindowResize = WM_APP + 30;
LPCWSTR ResizeCursor(int hit);

LRESULT CALLBACK ScrollHostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    const bool inputHost = GetWindowLongPtrW(hwnd, GWLP_USERDATA) != 0;
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH brush = CreateSolidBrush(inputHost ? Ui::Window : RGB(10, 16, 21));
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(30, 36, 44));
        SetBkColor(dc, Ui::Window);
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_COMMAND:
        return SendMessageW(GetParent(hwnd), message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK ResizeEdgeProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    const int hit = static_cast<int>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_SETCURSOR) {
        if (const LPCWSTR cursor = ResizeCursor(hit)) {
            SetCursor(LoadCursorW(nullptr, cursor));
            return TRUE;
        }
    }
    if (message == WM_LBUTTONDOWN) {
        SendMessageW(GetParent(hwnd), MsgBeginWindowResize, static_cast<WPARAM>(hit), 0);
        return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void SetFont(HWND control, HGDIOBJ font)
{
    if (control != nullptr) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

int ResizeEdgeAt(HWND window, POINT point)
{
    RECT client{};
    GetClientRect(window, &client);
    const int border = std::max(8, Ui::Scale(12, GetDpiForWindow(window)));
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
    return HTNOWHERE;
}

LPCWSTR ResizeCursor(int hit)
{
    if (hit == HTLEFT || hit == HTRIGHT) return IDC_SIZEWE;
    if (hit == HTTOP || hit == HTBOTTOM) return IDC_SIZENS;
    if (hit == HTTOPLEFT || hit == HTBOTTOMRIGHT) return IDC_SIZENWSE;
    if (hit == HTTOPRIGHT || hit == HTBOTTOMLEFT) return IDC_SIZENESW;
    return nullptr;
}

void DrawActionButtonContent(const DRAWITEMSTRUCT& item, bool primary)
{
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool hot = Ui::IsControlHot(item.hwndItem);
    const bool closeButton = item.CtlID == IdClose;
    const bool captionButton = item.CtlID == IdMinimize || item.CtlID == IdMaximize || closeButton;
    if (captionButton) {
        COLORREF fill = Ui::Window;
        if (closeButton && (hot || selected)) fill = selected ? RGB(204, 31, 44) : RGB(232, 54, 67);
        else if (selected) fill = RGB(225, 230, 237);
        else if (hot) fill = Ui::SurfaceHover;
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(item.hDC, &item.rcItem, brush);
        DeleteObject(brush);
        const UINT dpi = GetDpiForWindow(GetParent(item.hwndItem));
        const int cx = (item.rcItem.left + item.rcItem.right) / 2;
        const int cy = (item.rcItem.top + item.rcItem.bottom) / 2;
        const int arm = Ui::Scale(5, dpi);
        const COLORREF glyph = closeButton && (hot || selected) ? RGB(255, 255, 255) : Ui::Text;
        HPEN pen = CreatePen(PS_SOLID, std::max(1, Ui::Scale(1, dpi)), glyph);
        HGDIOBJ oldPen = SelectObject(item.hDC, pen);
        HGDIOBJ oldBrush = SelectObject(item.hDC, GetStockObject(NULL_BRUSH));
        if (item.CtlID == IdMinimize) {
            MoveToEx(item.hDC, cx - arm, cy + Ui::Scale(2, dpi), nullptr);
            LineTo(item.hDC, cx + arm + 1, cy + Ui::Scale(2, dpi));
        } else if (item.CtlID == IdMaximize) {
            if (IsZoomed(GetParent(item.hwndItem))) {
                Rectangle(item.hDC, cx - arm + Ui::Scale(2, dpi), cy - arm,
                          cx + arm + Ui::Scale(2, dpi), cy + arm);
                Rectangle(item.hDC, cx - arm - Ui::Scale(2, dpi), cy - arm + Ui::Scale(3, dpi),
                          cx + arm - Ui::Scale(2, dpi), cy + arm + Ui::Scale(3, dpi));
            } else {
                Rectangle(item.hDC, cx - arm, cy - arm, cx + arm + 1, cy + arm + 1);
            }
        } else {
            MoveToEx(item.hDC, cx - arm, cy - arm, nullptr); LineTo(item.hDC, cx + arm + 1, cy + arm + 1);
            MoveToEx(item.hDC, cx + arm, cy - arm, nullptr); LineTo(item.hDC, cx - arm - 1, cy + arm + 1);
        }
        SelectObject(item.hDC, oldBrush);
        SelectObject(item.hDC, oldPen);
        DeleteObject(pen);
        return;
    }
    HBRUSH controlBackground = CreateSolidBrush(Ui::Window);
    FillRect(item.hDC, &item.rcItem, controlBackground);
    DeleteObject(controlBackground);
    COLORREF fill = Ui::Window;
    if (disabled) fill = RGB(235, 238, 243);
    else if (primary) fill = selected ? Ui::PrimaryPressed : (hot ? RGB(40, 125, 240) : Ui::Primary);
    else if (selected) fill = RGB(232, 238, 246);
    else if (hot) fill = Ui::SurfaceHover;
    const COLORREF borderColor = primary ? fill : (hot ? Ui::BorderStrong : Ui::Border);
    Ui::DrawRoundedRect(item.hDC, item.rcItem, fill, borderColor, 10);

    wchar_t text[256]{};
    GetWindowTextW(item.hwndItem, text, 256);
    RECT textRect = item.rcItem;
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
    if (font == nullptr) font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ previousFont = SelectObject(item.hDC, font);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, disabled ? RGB(150, 157, 168) : (primary ? RGB(255, 255, 255) : Ui::Text));
    DrawTextW(item.hDC, text, -1, &textRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    SelectObject(item.hDC, previousFont);
}

void DrawActionButton(const DRAWITEMSTRUCT& item, bool primary)
{
    HDC dc = nullptr;
    HPAINTBUFFER buffer = BeginBufferedPaint(item.hDC, &item.rcItem, BPBF_COMPATIBLEBITMAP, nullptr, &dc);
    if (buffer == nullptr) {
        DrawActionButtonContent(item, primary);
        return;
    }
    DRAWITEMSTRUCT bufferedItem = item;
    bufferedItem.hDC = dc;
    DrawActionButtonContent(bufferedItem, primary);
    EndBufferedPaint(buffer, TRUE);
}

void DrawStatusIndicator(HDC dc, float left, float top, float diameter, COLORREF color)
{
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    Gdiplus::SolidBrush brush(
        Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
    graphics.FillEllipse(&brush, left, top, diameter, diameter);
}

void DrawTerminalButton(const DRAWITEMSTRUCT& item, bool active, bool ready, bool busy)
{
    HDC dc = nullptr;
    HPAINTBUFFER buffer = BeginBufferedPaint(item.hDC, &item.rcItem, BPBF_COMPATIBLEBITMAP, nullptr, &dc);
    if (buffer == nullptr) dc = item.hDC;
    HBRUSH background = CreateSolidBrush(Ui::Window);
    FillRect(dc, &item.rcItem, background);
    DeleteObject(background);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool hot = Ui::IsControlHot(item.hwndItem);
    const COLORREF fill = active ? Ui::Primary : (pressed ? RGB(232,238,246) : (hot ? Ui::SurfaceHover : Ui::Window));
    const COLORREF border = active ? Ui::Primary : Ui::Border;
    const UINT dpi = GetDpiForWindow(item.hwndItem);
    Ui::DrawRoundedRect(dc, item.rcItem, fill, border, Ui::Scale(7, dpi));
    wchar_t text[64]{};
    GetWindowTextW(item.hwndItem, text, 64);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ oldFont = font != nullptr ? SelectObject(dc, font) : nullptr;
    SIZE textSize{};
    GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &textSize);
    const int diameter = Ui::Scale(12, dpi);
    const int gap = Ui::Scale(8, dpi);
    const int groupWidth = diameter + gap + textSize.cx;
    const int buttonWidth = static_cast<int>(item.rcItem.right - item.rcItem.left);
    const int groupLeft = static_cast<int>(item.rcItem.left) +
        std::max(0, (buttonWidth - groupWidth) / 2);
    RECT textRect{groupLeft + diameter + gap, item.rcItem.top,
                  item.rcItem.right, item.rcItem.bottom};
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, active ? RGB(255,255,255) : Ui::Text);
    DrawTextW(dc, text, -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    const int top = (item.rcItem.top + item.rcItem.bottom - diameter) / 2;
    DrawStatusIndicator(dc, static_cast<float>(groupLeft), static_cast<float>(top),
                        static_cast<float>(diameter),
                        !ready ? Ui::Danger : (busy ? RGB(224,157,38) : Ui::Success));
    if (oldFont != nullptr) SelectObject(dc, oldFont);
    if (buffer != nullptr) EndBufferedPaint(buffer, TRUE);
}

}

MainWindow::MainWindow(bool elevated, std::optional<TerminalKind> startupTerminal, std::wstring startupCommand)
    : elevated_(elevated), startupCommand_(std::move(startupCommand)), startupTerminal_(startupTerminal)
{
    terminals_[0] = std::make_unique<TerminalContext>(TerminalKind::PowerShell);
    terminals_[1] = std::make_unique<TerminalContext>(TerminalKind::Wsl);
    Gdiplus::GdiplusStartupInput startupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken_, &startupInput, nullptr);
    for (TerminalKind kind : {TerminalKind::PowerShell, TerminalKind::Wsl}) {
        Context(kind).session.SetCallbacks(
            [this, kind](uint64_t generation, std::string bytes) { OnTerminalBytes(kind, generation, std::move(bytes)); },
            [this, kind](uint64_t generation) { OnTerminalExit(kind, generation); });
    }
}

MainWindow::~MainWindow()
{
    shuttingDown_ = true;
    for (auto& terminal : terminals_) terminal->session.Stop();
    if (outputFont_ != nullptr) DeleteObject(outputFont_);
    if (headerFont_ != nullptr) DeleteObject(headerFont_);
    if (sectionFont_ != nullptr) DeleteObject(sectionFont_);
    if (bodyFont_ != nullptr) DeleteObject(bodyFont_);
    if (gdiplusToken_ != 0) Gdiplus::GdiplusShutdown(gdiplusToken_);
}

bool MainWindow::Create(HINSTANCE instance)
{
    instance_ = instance;
    configLoadSucceeded_ = config_.Load();
    appIcon_ = LoadIconW(instance_, MAKEINTRESOURCEW(IdAppIcon));
    static bool registered = false;
    const wchar_t* className = L"CommandPanelMainWindow";
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = className;
        wc.hIcon = appIcon_ != nullptr ? appIcon_ : LoadIconW(nullptr, IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        registered = true;
    }
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const UINT initialDpi = GetDpiForSystem();
    const UiState& savedUi = config_.Ui();
    const int desiredWidth = savedUi.windowWidth > 0
        ? MulDiv(savedUi.windowWidth, static_cast<int>(initialDpi), 96) : Ui::Scale(1100, initialDpi);
    const int desiredHeight = savedUi.windowHeight > 0
        ? MulDiv(savedUi.windowHeight, static_cast<int>(initialDpi), 96) : Ui::Scale(780, initialDpi);
    const int windowWidth = std::clamp(desiredWidth, Ui::Scale(820, initialDpi),
                                       static_cast<int>(workArea.right - workArea.left - Ui::Scale(40, initialDpi)));
    const int windowHeight = std::clamp(desiredHeight, Ui::Scale(650, initialDpi),
                                        static_cast<int>(workArea.bottom - workArea.top - Ui::Scale(40, initialDpi)));
    const int windowLeft = workArea.left + (workArea.right - workArea.left - windowWidth) / 2;
    const int windowTop = workArea.top + (workArea.bottom - workArea.top - windowHeight) / 2;
    hwnd_ = CreateWindowExW(0, className, L"快捷控制台",
                            WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN,
                            windowLeft, windowTop, windowWidth, windowHeight, nullptr, nullptr, instance_, this);
    if (hwnd_ != nullptr) {
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIcon_));
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIcon_));
        const BOOL darkMode = FALSE;
        const COLORREF captionColor = RGB(255, 255, 255);
        const COLORREF textColor = RGB(30, 36, 44);
        DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
        DwmSetWindowAttribute(hwnd_, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
        DwmSetWindowAttribute(hwnd_, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));
        const COLORREF borderColor = 0xFFFFFFFE; // DWMWA_COLOR_NONE
        DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
        Ui::EnableRoundedCorners(hwnd_);
    }
    return hwnd_ != nullptr;
}

bool MainWindow::Initialize()
{
    LoadLibraryW(L"Msftedit.dll");

    title_ = CreateWindowExW(0, L"STATIC", L"快捷控制台", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                             20, 0, 240, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdTitle)), instance_, nullptr);
    adminMode_ = CreateWindowExW(0, L"STATIC", L"管理员模式", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                 260, 0, 150, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdAdmin)), instance_, nullptr);
    connection_ = CreateWindowExW(0, L"STATIC", L"PowerShell 未连接", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                   420, 0, 220, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdConnection)), instance_, nullptr);
    runtimeState_ = CreateWindowExW(0, L"STATIC", L"终端不可用", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                    0, 0, 160, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdRuntimeState)), instance_, nullptr);
    tabBar_.Create(hwnd_);
    tabBar_.SetCallbacks([this](int index) { OnTabSelected(index); },
                         [this] { AddTab(); },
                         [this](int index, const std::wstring& name) { return RenameTab(index, name); },
                         [this](int index) { DeleteTab(index); });
    buttonPanel_.Create(hwnd_);
    buttonPanel_.SetCallbacks([this](int index) { OnButton(index); },
                              [this](int index, POINT point) { OnContext(index, point); });
    terminalPowerShell_ = CreateWindowExW(0, L"BUTTON", L"PowerShell", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                          20, 0, 130, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdTerminalPowerShell)), instance_, nullptr);
    terminalWsl_ = CreateWindowExW(0, L"BUTTON", L"WSL", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                   150, 0, 90, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdTerminalWsl)), instance_, nullptr);
    clear_ = CreateWindowExW(0, L"BUTTON", L"清空", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                             0, 0, 80, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdClear)), instance_, nullptr);
    ctrlC_ = CreateWindowExW(0, L"BUTTON", L"退出", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                             0, 0, 80, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdExit)), instance_, nullptr);
    reset_ = CreateWindowExW(0, L"BUTTON", L"重置", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                              0, 0, 80, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdReset)), instance_, nullptr);

    static bool scrollHostClassRegistered = false;
    constexpr wchar_t scrollHostClassName[] = L"CommandPanelScrollHost";
    if (!scrollHostClassRegistered) {
        WNDCLASSW scrollHostClass{};
        scrollHostClass.lpfnWndProc = ScrollHostProc;
        scrollHostClass.hInstance = instance_;
        scrollHostClass.lpszClassName = scrollHostClassName;
        scrollHostClassRegistered = RegisterClassW(&scrollHostClass) != 0 ||
                                    GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    if (!scrollHostClassRegistered) return false;
    outputHost_ = CreateWindowExW(0, scrollHostClassName, nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                  0, 0, 0, 0, hwnd_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdOutputHost)),
                                  instance_, nullptr);
    if (!terminalView_.Create(outputHost_, instance_)) return false;
    static bool outputScrollClassRegistered = false;
    constexpr wchar_t outputScrollClassName[] = L"CommandPanelOutputScroll";
    if (!outputScrollClassRegistered) {
        WNDCLASSW outputScrollClass{};
        outputScrollClass.lpfnWndProc = ScrollIndicatorProc;
        outputScrollClass.hInstance = instance_;
        outputScrollClass.lpszClassName = outputScrollClassName;
        outputScrollClassRegistered = RegisterClassW(&outputScrollClass) != 0 ||
                                     GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    inputPrefix_ = CreateWindowExW(0, L"STATIC", L"PS  >", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                                   0, 0, 84, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdInputPrefix)), instance_, nullptr);
    inputHost_ = CreateWindowExW(0, scrollHostClassName, nullptr,
                                 WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                 0, 0, 0, 0, hwnd_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdInputHost)),
                                 instance_, reinterpret_cast<void*>(1));
    input_ = CreateWindowExW(0, L"EDIT", nullptr,
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN |
                                  ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                             0, 0, 0, 0, inputHost_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdInput)), instance_, nullptr);
    if (outputScrollClassRegistered) {
        inputScroll_ = CreateWindowExW(0, outputScrollClassName, nullptr,
                                        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, inputHost_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdInputScroll)), instance_, this);
    }
    execute_ = CreateWindowExW(0, L"BUTTON", L"执行", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                0, 0, 110, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdExecute)), instance_, nullptr);
    if (!title_ || !adminMode_ || !connection_ || !tabBar_.Hwnd() || !buttonPanel_.Hwnd() ||
        !runtimeState_ || !terminalPowerShell_ || !terminalWsl_ || !outputHost_ ||
        !terminalView_.Hwnd() || !inputHost_ || !input_ || !execute_) return false;

    SetWindowTextW(title_, L"快捷控制台");
    SetWindowTextW(adminMode_, elevated_ ? L"管理员权限" : L"普通权限");
    SetWindowTextW(connection_, L"PowerShell 已连接");
    SetWindowTextW(runtimeState_, L"终端空闲");
    SetWindowTextW(clear_, L"清空");
    SetWindowTextW(ctrlC_, L"中止");
    SetWindowTextW(reset_, L"重置终端");
    SetWindowTextW(inputPrefix_, L"PS  ›");
    SetWindowTextW(execute_, L"执行");
    minimize_ = CreateWindowExW(0, L"BUTTON", L"—", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                0, 0, 46, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdMinimize)), instance_, nullptr);
    maximize_ = CreateWindowExW(0, L"BUTTON", L"□", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                0, 0, 46, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdMaximize)), instance_, nullptr);
    close_ = CreateWindowExW(0, L"BUTTON", L"×", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                             0, 0, 46, 40, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdClose)), instance_, nullptr);
    if (!minimize_ || !maximize_ || !close_) return false;
    static bool resizeClassRegistered = false;
    constexpr wchar_t resizeClassName[] = L"CommandPanelResizeEdge";
    if (!resizeClassRegistered) {
        WNDCLASSW resizeClass{};
        resizeClass.lpfnWndProc = ResizeEdgeProc;
        resizeClass.hInstance = instance_;
        resizeClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        resizeClass.lpszClassName = resizeClassName;
        if (!RegisterClassW(&resizeClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        resizeClassRegistered = true;
    }
    constexpr int resizeHits[] = {HTTOPLEFT, HTTOP, HTTOPRIGHT, HTRIGHT,
                                  HTBOTTOMRIGHT, HTBOTTOM, HTBOTTOMLEFT, HTLEFT};
    for (size_t index = 0; index < std::size(resizeEdges_); ++index) {
        resizeEdges_[index] = CreateWindowExW(WS_EX_TRANSPARENT, resizeClassName, nullptr,
                                              WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr,
                                              instance_, reinterpret_cast<void*>(static_cast<INT_PTR>(resizeHits[index])));
        if (resizeEdges_[index] == nullptr) return false;
    }
    for (HWND button : {terminalPowerShell_, terminalWsl_, clear_, ctrlC_, reset_, execute_, minimize_, maximize_, close_}) {
        SetWindowLongPtrW(button, GWL_STYLE, GetWindowLongPtrW(button, GWL_STYLE) | BS_OWNERDRAW);
        Ui::TrackOwnerDrawButton(button);
    }
    RecreateFonts();
    terminalView_.SetInputCallback([this](std::string input) {
        CurrentTerminal().session.SendRaw(input);
    });
    SendMessageW(input_, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(L"输入当前终端命令，按 Ctrl+Enter 执行"));

    inputOriginalProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(input_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(InputProc)));
    SetWindowLongPtrW(input_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    if (!configLoadSucceeded_) AddDiagnostic(L"[快捷控制台] " + config_.LastError() + L"\r\n");
    const UINT layoutDpi = GetDpiForWindow(hwnd_);
    if (config_.Ui().buttonSectionHeight > 0)
        buttonSectionHeight_ = MulDiv(config_.Ui().buttonSectionHeight, static_cast<int>(layoutDpi), 96);
    if (config_.Ui().inputSectionHeight > 0)
        inputSectionHeight_ = MulDiv(config_.Ui().inputSectionHeight, static_cast<int>(layoutDpi), 96);
    activeTerminal_ = startupTerminal_.value_or(config_.Ui().activeTerminal);
    terminalView_.SetModel(&CurrentTerminal().model);
    RefreshTabs();
    RefreshButtons();
    Layout();
    for (TerminalKind kind : {TerminalKind::PowerShell, TerminalKind::Wsl}) {
        if (!StartTerminal(kind))
            AppendOutput(kind, L"[快捷控制台] " + Context(kind).session.LastError() + L"\r\n");
    }
    SwitchTerminal(activeTerminal_, false);
    const TerminalKind startupKind = startupTerminal_.value_or(TerminalKind::PowerShell);
    if (!startupCommand_.empty() && ExecuteManagedCommand(startupKind, startupCommand_)) {
        AppendOutput(startupKind, std::wstring(L"\r\n") +
                     (startupKind == TerminalKind::Wsl ? L"WSL> " : L"PS> ") + startupCommand_ + L"\r\n");
        SwitchTerminal(startupKind, false);
        UpdateBusyState();
    }
    wchar_t systemRoot[MAX_PATH]{};
    const DWORD rootLength = GetEnvironmentVariableW(L"SystemRoot", systemRoot, MAX_PATH);
    std::wstring wslPath(systemRoot, rootLength);
    wslPath += L"\\System32\\wsl.exe";
    if (rootLength == 0 || GetFileAttributesW(wslPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        AddDiagnostic(L"[快捷控制台] 未检测到 wsl.exe，WSL 相关按钮可能无法执行。\r\n");
    }
    SetFocus(input_);
    return true;
}

bool MainWindow::StartTerminal(TerminalKind kind)
{
    auto& terminal = Context(kind);
    terminal.parser.Reset();
    terminal.executor.Reset();
    terminal.permissionProbe.clear();
    terminal.windowsElevationRequired = false;
    CalculateTerminalGrid(terminalColumns_, terminalRows_);
    wchar_t systemRoot[MAX_PATH]{};
    const DWORD rootLength = GetEnvironmentVariableW(L"SystemRoot", systemRoot, MAX_PATH);
    if (rootLength == 0 || rootLength >= MAX_PATH) return false;
    std::wstring root(systemRoot, rootLength);
    TerminalLaunchSpec spec;
    if (kind == TerminalKind::PowerShell) {
        spec.executable = root + L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
        spec.arguments = L"-NoLogo -NoProfile";
        wchar_t profile[MAX_PATH]{};
        const DWORD profileLength = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
        if (profileLength > 0 && profileLength < MAX_PATH) spec.workingDirectory.assign(profile, profileLength);
        spec.displayName = L"PowerShell";
    } else {
        spec.executable = root + L"\\System32\\wsl.exe";
        spec.arguments = L"--cd ~ --exec bash -l";
        spec.displayName = L"WSL";
    }
    if (!terminal.session.Start(spec, terminalColumns_, terminalRows_)) {
        terminal.ready = false;
        RequestElevationIfNeeded(kind, terminal.session.LastError());
        if (kind == activeTerminal_) SwitchTerminal(kind, false);
        return false;
    }
    terminal.generation = terminal.session.Generation();
    terminal.ready = true;
    if (kind == activeTerminal_) SwitchTerminal(kind, false);
    UpdateBusyState();
    return true;
}

void MainWindow::CalculateTerminalGrid(short& columns, short& rows) const
{
    terminalView_.CalculateGrid(columns, rows);
}

void MainWindow::RecreateFonts()
{
    fontDpi_ = GetDpiForWindow(hwnd_);
    for (HFONT* font : {&outputFont_, &headerFont_, &sectionFont_, &bodyFont_}) {
        if (*font != nullptr) { DeleteObject(*font); *font = nullptr; }
    }
    outputFont_ = Ui::CreateFont(fontDpi_, 10, FW_NORMAL, L"Consolas");
    headerFont_ = Ui::CreateFont(fontDpi_, 15, FW_SEMIBOLD);
    sectionFont_ = Ui::CreateFont(fontDpi_, 11, FW_SEMIBOLD);
    bodyFont_ = Ui::CreateFont(fontDpi_, 10);
    SetFont(title_, headerFont_); SetFont(adminMode_, bodyFont_); SetFont(connection_, bodyFont_);
    SetFont(runtimeState_, bodyFont_); SetFont(terminalPowerShell_, bodyFont_); SetFont(terminalWsl_, bodyFont_);
    terminalView_.SetFont(outputFont_);
    SetFont(input_, bodyFont_); SetFont(inputPrefix_, sectionFont_); SetFont(execute_, bodyFont_);
    SetFont(clear_, bodyFont_); SetFont(ctrlC_, bodyFont_); SetFont(reset_, bodyFont_);
}

void MainWindow::Layout()
{
    const RECT previousButtonCard = buttonCardRect_;
    const RECT previousTerminalCard = terminalCardRect_;
    const RECT previousInputGroup = inputGroupRect_;
    const RECT previousUpperSplitter = upperSplitterRect_;
    const RECT previousLowerSplitter = lowerSplitterRect_;
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = static_cast<int>(client.right);
    const int height = static_cast<int>(client.bottom);
    const UINT dpi = GetDpiForWindow(hwnd_);
    if (fontDpi_ != dpi) RecreateFonts();
    const auto s = [dpi](int value) { return Ui::Scale(value, dpi); };
    const auto c = [dpi](int value) { return Ui::CompactScale(value, dpi); };
    const int headerHeight = s(60);
    const int tabsHeight = s(62);
    const int contentTop = headerHeight + tabsHeight;
    const int splitterThickness = std::max(4, s(6));
    const int buttonMinimum = c(64) + c(24) + s(20);
    const int inputMinimum = std::max(c(72), s(54));
    const int terminalMinimum = s(48) + s(24) + 4 * std::max(10, s(16));
    const int available = std::max(0, height - contentTop);
    if (buttonSectionHeight_ < 0) buttonSectionHeight_ = s(196);
    if (inputSectionHeight_ < 0) inputSectionHeight_ = c(92);
    const int buttonMaximum = std::max(buttonMinimum,
        available - splitterThickness * 2 - inputMinimum - terminalMinimum);
    buttonSectionHeight_ = std::clamp(buttonSectionHeight_, buttonMinimum, buttonMaximum);
    const int inputMaximum = std::max(inputMinimum,
        available - splitterThickness * 2 - buttonSectionHeight_ - terminalMinimum);
    inputSectionHeight_ = std::clamp(inputSectionHeight_, inputMinimum, inputMaximum);

    upperSplitterRect_ = RECT{0, contentTop + buttonSectionHeight_, width,
                              contentTop + buttonSectionHeight_ + splitterThickness};
    lowerSplitterRect_ = RECT{0, height - inputSectionHeight_ - splitterThickness, width,
                              height - inputSectionHeight_};
    const int terminalCardTop = upperSplitterRect_.bottom + s(10);
    const int terminalCardBottom = lowerSplitterRect_.top - s(10);
    const int terminalHeaderHeight = s(48);
    const int captionWidth = s(48);
    HDWP positions = BeginDeferWindowPos(18);
    const bool liveSplitterDrag = activeSplitter_ != 0;
    const auto moveChild = [this, &positions, liveSplitterDrag](
                               HWND child, int x, int y, int childWidth, int childHeight) {
        RECT current{};
        GetWindowRect(child, &current);
        MapWindowPoints(HWND_DESKTOP, hwnd_, reinterpret_cast<POINT*>(&current), 2);
        if (current.left == x && current.top == y && current.right - current.left == childWidth &&
            current.bottom - current.top == childHeight) {
            return;
        }
        const UINT flags = SWP_NOACTIVATE | SWP_NOZORDER |
                           (liveSplitterDrag ? SWP_NOREDRAW : 0);
        if (positions != nullptr) {
            positions = DeferWindowPos(positions, child, nullptr, x, y, childWidth, childHeight, flags);
        } else {
            SetWindowPos(child, nullptr, x, y, childWidth, childHeight, flags);
        }
    };

    moveChild(title_, s(56), 0, s(160), headerHeight);
    moveChild(adminMode_, s(274), 0, s(96), headerHeight);
    moveChild(connection_, s(398), 0, s(145), headerHeight);
    moveChild(runtimeState_, s(558), 0, s(118), headerHeight);
    moveChild(minimize_, width - captionWidth * 3, 0, captionWidth, headerHeight);
    moveChild(maximize_, width - captionWidth * 2, 0, captionWidth, headerHeight);
    moveChild(close_, width - captionWidth, 0, captionWidth, headerHeight);
    moveChild(tabBar_.Hwnd(), 0, headerHeight, width, tabsHeight);

    buttonCardRect_ = RECT{s(20), contentTop + s(8), width - s(20),
                           static_cast<int>(upperSplitterRect_.top) - s(8)};
    moveChild(buttonPanel_.Hwnd(), buttonCardRect_.left + s(6), buttonCardRect_.top + s(6),
              std::max(1, static_cast<int>(buttonCardRect_.right - buttonCardRect_.left) - s(12)),
              std::max(1, static_cast<int>(buttonCardRect_.bottom - buttonCardRect_.top) - s(12)));

    const int terminalSelectorLeft = s(36);
    const int terminalSelectorWidth = s(105);
    const int terminalSelectorGap = s(4);
    moveChild(terminalPowerShell_, terminalSelectorLeft, terminalCardTop + s(8), terminalSelectorWidth, s(32));
    moveChild(terminalWsl_, terminalSelectorLeft + terminalSelectorWidth + terminalSelectorGap,
              terminalCardTop + s(8), terminalSelectorWidth, s(32));
    const int toolHeight = s(34);
    const int toolTop = terminalCardTop + (terminalHeaderHeight - toolHeight) / 2;
    const int toolGap = s(10);
    const int toolRight = width - s(32);
    const int resetWidth = s(92);
    const int clearWidth = s(70);
    const int stopWidth = s(74);
    moveChild(reset_, toolRight - resetWidth, toolTop, resetWidth, toolHeight);
    moveChild(clear_, toolRight - resetWidth - toolGap - clearWidth, toolTop, clearWidth, toolHeight);
    moveChild(ctrlC_, toolRight - resetWidth - toolGap - clearWidth - toolGap - stopWidth,
              toolTop, stopWidth, toolHeight);
    const int terminalBodyInset = s(6);
    const int outputWidth = std::max(1, width - s(40) - terminalBodyInset * 2);
    const int outputHeight = std::max(
        1, terminalCardBottom - terminalCardTop - terminalHeaderHeight - terminalBodyInset * 2);
    moveChild(outputHost_, s(20) + terminalBodyInset,
              terminalCardTop + terminalHeaderHeight + terminalBodyInset,
              outputWidth, outputHeight);
    Ui::ApplyRoundedRegion(outputHost_, outputWidth, outputHeight, s(8));
    terminalCardRect_ = RECT{s(20), terminalCardTop, width - s(20), terminalCardBottom};

    const int inputTop = lowerSplitterRect_.bottom;
    const int inputY = inputTop + s(8);
    const int inputControlHeight = std::max(1, inputSectionHeight_ - s(16));
    const int executeWidth = c(120);
    const int executeLeft = width - s(20) - executeWidth;
    const int prefixWidth = c(100);
    inputGroupRect_ = RECT{s(20), inputY, executeLeft - s(10), inputY + inputControlHeight};
    moveChild(inputPrefix_, inputGroupRect_.left, inputGroupRect_.top, prefixWidth, inputControlHeight);
    const int inputLeft = inputGroupRect_.left + prefixWidth + s(2);
    const int inputControlWidth = std::max(
        s(80), static_cast<int>(inputGroupRect_.right - inputGroupRect_.left) - prefixWidth - s(6));
    const int inputEditHeight = std::max(1, inputControlHeight - s(6));
    moveChild(inputHost_, inputLeft, inputGroupRect_.top + s(3), inputControlWidth, inputEditHeight);
    const int executeHeight = std::min(inputControlHeight, c(58));
    moveChild(execute_, executeLeft, inputY + (inputControlHeight - executeHeight) / 2,
              executeWidth, executeHeight);
    if (positions != nullptr) EndDeferWindowPos(positions);
    const int nativeScrollWidth = std::max(s(8), GetSystemMetricsForDpi(SM_CXVSCROLL, dpi));
    const auto resizeHostedControl = [liveSplitterDrag](
                                         HWND control, int controlWidth, int controlHeight) {
        RECT current{};
        GetWindowRect(control, &current);
        if (current.right - current.left == controlWidth && current.bottom - current.top == controlHeight) return;
        SetWindowPos(control, nullptr, 0, 0, controlWidth, controlHeight,
                     SWP_NOACTIVATE | SWP_NOZORDER |
                         (liveSplitterDrag ? SWP_NOREDRAW : 0));
    };
    resizeHostedControl(terminalView_.Hwnd(), outputWidth,
                        liveSplitterDrag ? std::max(1, height) : outputHeight);
    resizeHostedControl(input_, inputControlWidth + nativeScrollWidth,
                        liveSplitterDrag ? std::max(1, height) : inputEditHeight);
    const int overlayWidth = std::max(2, s(6));
    if (inputScroll_ != nullptr) {
        const UINT scrollFlags = SWP_NOACTIVATE | SWP_SHOWWINDOW |
                                 (liveSplitterDrag ? SWP_NOREDRAW : 0);
        SetWindowPos(inputScroll_, HWND_TOP, inputControlWidth - overlayWidth, s(3), overlayWidth,
                     std::max(1, inputEditHeight - s(6)), scrollFlags);
    }
    const int resizeEdge = std::max(7, s(10));
    const UINT edgeFlags = SWP_NOACTIVATE | SWP_NOREDRAW;
    const auto placeEdge = [&](size_t index, int x, int y, int edgeWidth, int edgeHeight) {
        SetWindowPos(resizeEdges_[index], HWND_TOP, x, y, std::max(1, edgeWidth),
                     std::max(1, edgeHeight), edgeFlags);
    };
    placeEdge(0, 0, 0, resizeEdge, resizeEdge);
    placeEdge(1, resizeEdge, 0, width - resizeEdge * 2, resizeEdge);
    placeEdge(2, width - resizeEdge, 0, resizeEdge, resizeEdge);
    placeEdge(3, width - resizeEdge, resizeEdge, resizeEdge, height - resizeEdge * 2);
    placeEdge(4, width - resizeEdge, height - resizeEdge, resizeEdge, resizeEdge);
    placeEdge(5, resizeEdge, height - resizeEdge, width - resizeEdge * 2, resizeEdge);
    placeEdge(6, 0, height - resizeEdge, resizeEdge, resizeEdge);
    placeEdge(7, 0, resizeEdge, resizeEdge, height - resizeEdge * 2);
    RECT dirty{};
    const auto addDirty = [&dirty](const RECT& first, const RECT& second) {
        if (EqualRect(&first, &second)) return;
        RECT pair{};
        UnionRect(&pair, &first, &second);
        RECT combined{};
        UnionRect(&combined, &dirty, &pair);
        dirty = combined;
    };
    addDirty(previousButtonCard, buttonCardRect_);
    addDirty(previousTerminalCard, terminalCardRect_);
    addDirty(previousInputGroup, inputGroupRect_);
    addDirty(previousUpperSplitter, upperSplitterRect_);
    addDirty(previousLowerSplitter, lowerSplitterRect_);
    if (IsRectEmpty(&dirty)) dirty = RECT{0, contentTop, width, height};
    InvalidateRect(hwnd_, activeSplitter_ != 0 ? &dirty : nullptr, FALSE);
    if (activeSplitter_ != 0) {
        RedrawWindow(hwnd_, &dirty, nullptr,
                     RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    PostMessageW(hwnd_, WM_APP_REFRESH_SCROLLS, 0, 0);
    if (activeSplitter_ == 0) {
        short columns = 0, rows = 0;
        CalculateTerminalGrid(columns, rows);
        if (columns != terminalColumns_ || rows != terminalRows_) {
            terminalColumns_ = columns;
            terminalRows_ = rows;
            for (auto& terminal : terminals_) {
                terminal->model.Resize(columns, rows);
                if (terminal->ready) terminal->session.Resize(columns, rows);
            }
            terminalView_.OnModelChanged();
        }
    }
}

void MainWindow::RefreshTabs()
{
    activeTab_ = config_.Tabs().empty()
        ? -1 : std::clamp(activeTab_, 0, static_cast<int>(config_.Tabs().size()) - 1);
    std::vector<std::wstring> names;
    for (const auto& tab : config_.Tabs()) names.push_back(tab.name);
    tabBar_.SetTabs(names, activeTab_);
}

void MainWindow::RefreshButtons()
{
    if (config_.Tabs().empty()) {
        buttonPanel_.SetButtons({});
        return;
    }
    activeTab_ = std::clamp(activeTab_, 0, static_cast<int>(config_.Tabs().size()) - 1);
    const auto& buttons = config_.Tabs()[activeTab_].buttons;
    buttonPanel_.SetButtons(buttons);
    std::vector<bool> availability;
    availability.reserve(buttons.size());
    for (const auto& button : buttons) {
        const auto& terminal = Context(button.terminal);
        availability.push_back(terminal.ready && !terminal.executor.IsBusy());
    }
    buttonPanel_.SetAvailability(std::move(availability));
}

void MainWindow::OnTabSelected(int index)
{
    if (index < 0 || index >= static_cast<int>(config_.Tabs().size())) return;
    activeTab_ = index;
    RefreshTabs();
    RefreshButtons();
}

void MainWindow::AddTab()
{
    std::wstring name = L"新标签页";
    auto exists = [&](const std::wstring& candidate) {
        return std::any_of(config_.Tabs().begin(), config_.Tabs().end(), [&](const CommandTab& tab) {
            return _wcsicmp(tab.name.c_str(), candidate.c_str()) == 0;
        });
    };
    for (int suffix = 2; exists(name); ++suffix) name = L"新标签页 " + std::to_wstring(suffix);
    CommandTab tab{ConfigManager::NewId(), std::move(name), {}};
    const int previousActive = activeTab_;
    config_.Tabs().push_back(std::move(tab));
    activeTab_ = static_cast<int>(config_.Tabs().size()) - 1;
    if (!SaveConfig()) {
        config_.Tabs().pop_back();
        activeTab_ = previousActive;
    }
    RefreshTabs();
    RefreshButtons();
}

bool MainWindow::RenameTab(int index, const std::wstring& name)
{
    if (index < 0 || index >= static_cast<int>(config_.Tabs().size())) return false;
    const std::wstring previous = config_.Tabs()[index].name;
    config_.Tabs()[index].name = name;
    if (SaveConfig()) return true;
    config_.Tabs()[index].name = previous;
    return false;
}

void MainWindow::DeleteTab(int index)
{
    if (index < 0 || index >= static_cast<int>(config_.Tabs().size())) return;
    const std::wstring message = L"确定删除标签页“" + config_.Tabs()[index].name + L"”及其中的全部按钮吗？";
    if (!CommandDialog::Confirm(hwnd_, L"删除标签页", message, L"删除")) return;

    const auto previousTabs = config_.Tabs();
    const int previousActive = activeTab_;
    config_.Tabs().erase(config_.Tabs().begin() + index);
    if (config_.Tabs().empty()) activeTab_ = -1;
    else if (activeTab_ > index) --activeTab_;
    else if (activeTab_ == index) activeTab_ = std::min(index, static_cast<int>(config_.Tabs().size()) - 1);

    if (!SaveConfig()) {
        config_.Tabs() = previousTabs;
        activeTab_ = previousActive;
    }
    RefreshTabs();
    RefreshButtons();
}

void MainWindow::UpdateBusyState()
{
    const auto& terminal = CurrentTerminal();
    buttonPanel_.SetBusy(false);
    EnableWindow(clear_, TRUE);
    EnableWindow(ctrlC_, terminal.ready);
    EnableWindow(reset_, TRUE);
    EnableWindow(execute_, terminal.ready);
    if (terminal.executor.IsBusy()) {
        SetStatus(L"命令执行中");
    } else if (terminal.ready) {
        SetStatus(L"终端空闲");
    } else {
        SetStatus(L"终端不可用");
    }
    if (!config_.Tabs().empty() && activeTab_ >= 0) {
        std::vector<bool> availability;
        const auto& buttons = config_.Tabs()[activeTab_].buttons;
        availability.reserve(buttons.size());
        for (const auto& button : buttons) {
            const auto& target = Context(button.terminal);
            availability.push_back(target.ready && !target.executor.IsBusy());
        }
        buttonPanel_.SetAvailability(std::move(availability));
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::SwitchTerminal(TerminalKind kind, bool focus)
{
    activeTerminal_ = kind;
    auto& terminal = CurrentTerminal();
    terminalView_.SetModel(&terminal.model);
    SetWindowTextW(inputPrefix_, kind == TerminalKind::Wsl ? L"WSL  ›" : L"PS  ›");
    const std::wstring connection = std::wstring(kind == TerminalKind::Wsl ? L"WSL " : L"PowerShell ") +
                                    (terminal.ready ? L"已连接" : L"未连接");
    SetWindowTextW(connection_, connection.c_str());
    InvalidateRect(terminalPowerShell_, nullptr, FALSE);
    InvalidateRect(terminalWsl_, nullptr, FALSE);
    UpdateBusyState();
    if (focus && kind == TerminalKind::Wsl) {
        terminal.elevationOnFailure = true;
        if (terminal.windowsElevationRequired) {
            SetStatus(L"●  WSL 需要管理员权限，正在请求提升");
            if (LaunchElevatedTerminal(TerminalKind::Wsl)) return;
        }
    }
    if (focus) SetFocus(terminalView_.Hwnd());
}

void MainWindow::SetStatus(const std::wstring& text)
{
    std::wstring normalized = text;
    if (!normalized.empty() && normalized.front() == L'●') {
        normalized.erase(normalized.begin());
        while (!normalized.empty() && iswspace(normalized.front())) normalized.erase(normalized.begin());
    }
    SetWindowTextW(runtimeState_, normalized.c_str());
}

void MainWindow::AddDiagnostic(const std::wstring& text)
{
    AppendOutput(activeTerminal_, text);
}

void MainWindow::AppendOutput(TerminalKind kind, std::wstring_view text)
{
    if (text.empty()) return;
    auto& terminal = Context(kind);
    terminal.model.Feed(text);
    const std::string response = terminal.model.TakeResponse();
    if (!response.empty() && terminal.ready) terminal.session.SendRaw(response);
    if (kind == activeTerminal_) terminalView_.OnModelChanged();
}

void MainWindow::ShowInputScrollBar()
{
    ShowScrollIndicator(input_, inputScroll_, WM_APP_INPUT_SCROLL_HIDE_TIMER);
}

void MainWindow::HideInputScrollBar()
{
    HideScrollIndicator(inputScroll_, WM_APP_INPUT_SCROLL_HIDE_TIMER);
}

bool MainWindow::HandleTextWheel(HWND target, int& remainder, WPARAM wParam)
{
    const bool moved = UiScroll::ScrollTextControl(
        target, GET_WHEEL_DELTA_WPARAM(wParam), UiScroll::SystemWheelScrollLines(), remainder);
    if (target == input_) ShowInputScrollBar();
    return moved;
}

void MainWindow::RefreshVisibleScrollIndicators()
{
    if (activeSplitter_ != 0) {
        HideInputScrollBar();
        return;
    }
    if (inputScrollVisible_) ShowInputScrollBar();
    else if (inputScroll_ != nullptr) InvalidateRect(inputScroll_, nullptr, FALSE);
}

void MainWindow::ShowScrollIndicator(HWND target, HWND indicator, UINT timer)
{
    if (target == nullptr || indicator == nullptr) return;
    if (!UiScroll::IsScrollable(UiScroll::ReadMetrics(target))) {
        HideScrollIndicator(indicator, timer);
        return;
    }
    if (indicator == inputScroll_) inputScrollVisible_ = true;
    InvalidateRect(indicator, nullptr, FALSE);
    SetTimer(hwnd_, timer, 1200, nullptr);
}

void MainWindow::HideScrollIndicator(HWND indicator, UINT timer)
{
    KillTimer(hwnd_, timer);
    if (indicator == inputScroll_) inputScrollVisible_ = false;
    if (indicator != nullptr) InvalidateRect(indicator, nullptr, FALSE);
}

LRESULT CALLBACK MainWindow::ScrollIndicatorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = reinterpret_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    switch (message) {
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEWHEEL:
        if (self != nullptr) {
            SendMessageW(self->input_, message, wParam, lParam);
        }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC targetDc = BeginPaint(hwnd, &paint);
        RECT client{};
        GetClientRect(hwnd, &client);
        HDC dc = nullptr;
        HPAINTBUFFER buffer = BeginBufferedPaint(targetDc, &client, BPBF_COMPATIBLEBITMAP, nullptr, &dc);
        if (buffer == nullptr) dc = targetDc;
        HBRUSH background = CreateSolidBrush(Ui::Window);
        FillRect(dc, &client, background);
        DeleteObject(background);
        const bool thumbActive = self != nullptr && self->inputScrollVisible_;
        if (thumbActive) {
            const HWND target = self->input_;
            const int height = std::max(1, static_cast<int>(client.bottom - client.top));
            const UiScroll::Thumb thumb = UiScroll::CalculateThumb(
                UiScroll::ReadMetrics(target), height,
                Ui::Scale(18, GetDpiForWindow(self->hwnd_)));
            if (thumb.visible) {
                const int width = std::max(2, Ui::Scale(4, GetDpiForWindow(self->hwnd_)));
                const int left = std::max(0, (static_cast<int>(client.right) - width) / 2);
                RECT thumbRect{left, thumb.top, std::min(static_cast<int>(client.right), left + width),
                               thumb.top + thumb.height};
                Ui::DrawRoundedRect(dc, thumbRect, RGB(100, 116, 139), RGB(100, 116, 139), width);
            }
        }
        if (buffer != nullptr) EndBufferedPaint(buffer, TRUE);
        EndPaint(hwnd, &paint);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void MainWindow::ClearOutput()
{
    CurrentTerminal().model.Clear();
    terminalView_.OnModelChanged();
    SetStatus(L"●  输出已清空");
}

void MainWindow::ExitCommand()
{
    if (CurrentTerminal().session.SendRaw(std::string(1, '\x03'))) SetStatus(L"●  已发送退出当前命令");
}

void MainWindow::OnTerminalBytes(TerminalKind kind, uint64_t generation, std::string bytes)
{
    if (hwnd_ == nullptr) return;
    const std::wstring parsed = Context(kind).parser.Feed(
        std::span<const unsigned char>(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()));
    auto* packet = new TerminalOutputPacket{kind, generation, parsed};
    if (!PostMessageW(hwnd_, WM_APP_TERMINAL_OUTPUT, 0, reinterpret_cast<LPARAM>(packet))) delete packet;
}

void MainWindow::OnTerminalExit(TerminalKind kind, uint64_t generation)
{
    if (hwnd_ == nullptr) return;
    auto* packet = new TerminalExitPacket{kind, generation};
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
        if (!CommandDialog::Confirm(hwnd_, L"确认执行", message, L"执行")) return;
    }
    if (!ExecuteManagedCommand(button.terminal, button.command)) {
        SetStatus(L"●  命令无法发送");
        return;
    }
    SwitchTerminal(button.terminal);
    AppendOutput(button.terminal,
                 std::wstring(L"\r\n") + (button.terminal == TerminalKind::Wsl ? L"WSL> " : L"PS> ") +
                 button.command + L"\r\n");
    UpdateBusyState();
}

bool MainWindow::ExecuteManagedCommand(TerminalKind kind, const std::wstring& command, bool elevateWsl)
{
    auto& terminal = Context(kind);
    if (!terminal.executor.ExecuteManaged(command, elevateWsl)) return false;
    terminal.currentCommand = command;
    if (!elevateWsl) terminal.elevationRequested = false;
    terminal.linuxElevationPending = false;
    terminal.permissionProbe.clear();
    return true;
}

void MainWindow::ExecuteInput()
{
    wchar_t buffer[8192]{};
    GetWindowTextW(input_, buffer, 8192);
    std::wstring command(buffer);
    if (command.empty()) return;
    SetWindowTextW(input_, L"");
    auto& terminal = CurrentTerminal();
    terminal.history.push_back(command);
    terminal.historyPosition = -1;
    if (terminal.executor.IsBusy()) {
        terminal.executor.SendInteractiveInput(command);
    } else if (ExecuteManagedCommand(activeTerminal_, command)) {
        AppendOutput(activeTerminal_, std::wstring(L"\r\n") +
                     (activeTerminal_ == TerminalKind::Wsl ? L"WSL> " : L"PS> ") + command + L"\r\n");
        UpdateBusyState();
    } else {
        SetStatus(L"●  命令无法发送");
    }
}

bool MainWindow::RequestElevationIfNeeded(TerminalKind kind, std::wstring_view output)
{
    auto& terminal = Context(kind);
    if (terminal.elevationRequested) return false;
    terminal.permissionProbe.append(output);
    constexpr size_t probeLimit = 4096;
    if (terminal.permissionProbe.size() > probeLimit)
        terminal.permissionProbe.erase(0, terminal.permissionProbe.size() - probeLimit);
    std::wstring lower(terminal.permissionProbe);
    std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
    const bool windowsElevation = lower.find(L"requires elevation") != std::wstring::npos ||
                                  lower.find(L"requested operation requires elevation") != std::wstring::npos ||
                                  lower.find(L"\x8BF7\x6C42\x7684\x64CD\x4F5C\x9700\x8981\x63D0\x5347") != std::wstring::npos ||
                                  terminal.permissionProbe.find(L"操作需要提升") != std::wstring::npos ||
                                  terminal.permissionProbe.find(L"需要管理员权限") != std::wstring::npos;
    const bool accessDenied = lower.find(L"access is denied") != std::wstring::npos ||
                              lower.find(L"permission denied") != std::wstring::npos ||
                              windowsElevation ||
                              terminal.permissionProbe.find(L"拒绝访问") != std::wstring::npos ||
                              terminal.permissionProbe.find(L"权限不足") != std::wstring::npos;
    if (!accessDenied) return false;

    if (kind == TerminalKind::Wsl && windowsElevation && terminal.currentCommand.empty()) {
        terminal.windowsElevationRequired = true;
        if (terminal.elevationOnFailure) return LaunchElevatedTerminal(TerminalKind::Wsl);
        return false;
    }
    if (kind == TerminalKind::Wsl && !windowsElevation && !terminal.currentCommand.empty()) {
        terminal.elevationRequested = true;
        terminal.linuxElevationPending = true;
        return false;
    }
    if (terminal.currentCommand.empty()) return false;
    return LaunchElevatedTerminal(kind);
}

bool MainWindow::LaunchElevatedTerminal(TerminalKind kind)
{
    auto& terminal = Context(kind);
    if (elevated_ || terminal.elevationRequested) return false;
    terminal.elevationRequested = true;
    wchar_t executable[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        SetStatus(L"● 无法定位程序，未能请求管理员权限");
        return false;
    }
    std::wstring parameters = L"--elevated-terminal " +
        Utf8ToWide(std::string(TerminalKindName(kind)));
    if (!terminal.currentCommand.empty()) {
        parameters += L" --elevated-command " +
            Utf8ToWide(Base64Encode(WideToUtf8(terminal.currentCommand)));
    }
    SHELLEXECUTEINFOW request{sizeof(request)};
    request.fMask = SEE_MASK_FLAG_NO_UI;
    request.hwnd = hwnd_;
    request.lpVerb = L"runas";
    request.lpFile = executable;
    request.lpParameters = parameters.c_str();
    request.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&request)) {
        terminal.elevationRequested = false;
        SetStatus(GetLastError() == ERROR_CANCELLED ? L"● 已取消管理员权限请求" : L"● 未能请求管理员权限");
        return false;
    }
    terminal.windowsElevationRequired = false;
    terminal.elevationOnFailure = false;
    SetStatus(L"● 已请求管理员权限，正在以管理员模式重新打开");
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    return true;
}

void MainWindow::OnContext(int index, POINT point)
{
    if (index >= 0) ShowContextMenu(index, point);
}

void MainWindow::ShowContextMenu(int index, POINT point)
{
    switch (CommandMenu::Show(hwnd_, point)) {
    case CommandMenuAction::Edit: EditButton(index); break;
    case CommandMenuAction::Delete: DeleteButton(index); break;
    case CommandMenuAction::None: break;
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
    if (!CommandDialog::Confirm(hwnd_, L"删除按钮", message, L"删除")) return;
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

void MainWindow::PersistUiState()
{
    if (hwnd_ == nullptr) return;
    WINDOWPLACEMENT placement{sizeof(placement)};
    RECT windowRect{};
    if (GetWindowPlacement(hwnd_, &placement)) windowRect = placement.rcNormalPosition;
    else GetWindowRect(hwnd_, &windowRect);
    const UINT dpi = GetDpiForWindow(hwnd_);
    UiState& state = config_.Ui();
    state.windowWidth = std::max(1, MulDiv(windowRect.right - windowRect.left, 96, static_cast<int>(dpi)));
    state.windowHeight = std::max(1, MulDiv(windowRect.bottom - windowRect.top, 96, static_cast<int>(dpi)));
    state.buttonSectionHeight = std::max(0, MulDiv(buttonSectionHeight_, 96, static_cast<int>(dpi)));
    state.inputSectionHeight = std::max(0, MulDiv(inputSectionHeight_, 96, static_cast<int>(dpi)));
    state.activeTerminal = activeTerminal_;
    if (!config_.Save()) SetStatus(L"配置保存失败");
}

void MainWindow::ResetTerminal(bool ask)
{
    const TerminalKind kind = activeTerminal_;
    auto& terminal = CurrentTerminal();
    const std::wstring terminalName = kind == TerminalKind::Wsl ? L"WSL" : L"PowerShell";
    if (ask && !CommandDialog::Confirm(hwnd_, L"重置终端",
                                       L"这会终止当前 " + terminalName + L" / ConPTY 会话，但不会退出快捷控制台。\r\n\r\n是否继续？",
                                       L"重置")) return;
    terminal.ready = false;
    terminal.restartAttempts = 0;
    terminal.executor.Reset();
    terminal.session.Stop();
    terminal.parser.Reset();
    UpdateBusyState();
    CalculateTerminalGrid(terminalColumns_, terminalRows_);
    terminal.session.Restart(terminalColumns_, terminalRows_);
    if (terminal.session.IsRunning()) {
        terminal.generation = terminal.session.Generation();
        terminal.ready = true;
        AppendOutput(kind, L"[快捷控制台] " + terminalName + L" 会话已重新建立。\r\n");
    } else {
        AppendOutput(kind, L"[快捷控制台] 终端重置失败：" + terminal.session.LastError() + L"\r\n");
        SetStatus(L"●  终端重置失败");
    }
    SwitchTerminal(kind, false);
    Layout();
    UpdateBusyState();
}

LRESULT MainWindow::HandleInputMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                ExecuteInput();
                return 0;
            }
            const LRESULT result = CallWindowProcW(inputOriginalProc_, hwnd, message, wParam, lParam);
            PostMessageW(hwnd_, WM_APP_INPUT_SCROLLED, 0, 0);
            return result;
        }
        if (wParam == VK_ESCAPE) { SetWindowTextW(hwnd, L""); return 0; }
        const bool historyKey = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        auto& terminal = CurrentTerminal();
        if (historyKey && wParam == VK_UP && !terminal.history.empty()) {
            if (terminal.historyPosition < 0) terminal.historyPosition = static_cast<int>(terminal.history.size()) - 1;
            else terminal.historyPosition = std::max(0, terminal.historyPosition - 1);
            SetWindowTextW(hwnd, terminal.history[terminal.historyPosition].c_str());
            SendMessageW(hwnd, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
            return 0;
        }
        if (historyKey && wParam == VK_DOWN && terminal.historyPosition >= 0) {
            ++terminal.historyPosition;
            if (terminal.historyPosition >= static_cast<int>(terminal.history.size())) { terminal.historyPosition = -1; SetWindowTextW(hwnd, L""); }
            else SetWindowTextW(hwnd, terminal.history[terminal.historyPosition].c_str());
            SendMessageW(hwnd, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
            return 0;
        }
    }
    if (message == WM_MOUSEWHEEL) {
        HandleTextWheel(input_, inputWheelRemainder_, wParam);
        return 0;
    }
    if (message == WM_VSCROLL) {
        const LRESULT result = CallWindowProcW(inputOriginalProc_, hwnd, message, wParam, lParam);
        PostMessageW(hwnd_, WM_APP_INPUT_SCROLLED, 0, 0);
        return result;
    }
    if (message == WM_CHAR && wParam == L'\r' && (GetKeyState(VK_CONTROL) & 0x8000) != 0)
        return 0;
    const LRESULT result = CallWindowProcW(inputOriginalProc_, hwnd, message, wParam, lParam);
    if (message == WM_KEYDOWN &&
        (wParam == VK_PRIOR || wParam == VK_NEXT || wParam == VK_HOME || wParam == VK_END))
        PostMessageW(hwnd_, WM_APP_INPUT_SCROLLED, 0, 0);
    return result;
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
    case WM_NCCALCSIZE:
        if (wParam != FALSE) return 0;
        break;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        const UINT dpi = GetDpiForWindow(hwnd_);
        info->ptMinTrackSize.x = Ui::Scale(820, dpi);
        info->ptMinTrackSize.y = Ui::Scale(650, dpi);
        MONITORINFO monitorInfo{sizeof(monitorInfo)};
        if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
            info->ptMaxPosition.x = monitorInfo.rcWork.left - monitorInfo.rcMonitor.left;
            info->ptMaxPosition.y = monitorInfo.rcWork.top - monitorInfo.rcMonitor.top;
            info->ptMaxSize.x = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
            info->ptMaxSize.y = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
        }
        return 0;
    }
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd_, &point);
        if (ResizeEdgeAt(hwnd_, point) != HTNOWHERE) return HTCLIENT;
        if (point.y < Ui::Scale(60, GetDpiForWindow(hwnd_))) return HTCAPTION;
        return HTCLIENT;
    }
    case MsgBeginWindowResize:
        if (!IsZoomed(hwnd_)) {
            resizeHitTest_ = static_cast<int>(wParam);
            GetCursorPos(&resizeDragOrigin_);
            GetWindowRect(hwnd_, &resizeWindowOrigin_);
            SetCapture(hwnd_);
            if (const LPCWSTR cursor = ResizeCursor(resizeHitTest_))
                SetCursor(LoadCursorW(nullptr, cursor));
        }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int resizeEdge = IsZoomed(hwnd_) ? HTNOWHERE : ResizeEdgeAt(hwnd_, point);
        if (resizeEdge != HTNOWHERE) {
            resizeHitTest_ = resizeEdge;
            GetCursorPos(&resizeDragOrigin_);
            GetWindowRect(hwnd_, &resizeWindowOrigin_);
            SetCapture(hwnd_);
            if (const LPCWSTR cursor = ResizeCursor(resizeEdge)) SetCursor(LoadCursorW(nullptr, cursor));
            return 0;
        }
        if (PtInRect(&upperSplitterRect_, point) || PtInRect(&lowerSplitterRect_, point)) {
            activeSplitter_ = PtInRect(&upperSplitterRect_, point) ? 1 : 2;
            splitterDragOriginY_ = point.y;
            splitterDragOriginSize_ = activeSplitter_ == 1 ? buttonSectionHeight_ : inputSectionHeight_;
            SetCapture(hwnd_);
            SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (resizeHitTest_ != HTNOWHERE) {
            POINT screenPoint{};
            GetCursorPos(&screenPoint);
            const int dx = screenPoint.x - resizeDragOrigin_.x;
            const int dy = screenPoint.y - resizeDragOrigin_.y;
            RECT next = resizeWindowOrigin_;
            if (resizeHitTest_ == HTLEFT || resizeHitTest_ == HTTOPLEFT || resizeHitTest_ == HTBOTTOMLEFT)
                next.left += dx;
            if (resizeHitTest_ == HTRIGHT || resizeHitTest_ == HTTOPRIGHT || resizeHitTest_ == HTBOTTOMRIGHT)
                next.right += dx;
            if (resizeHitTest_ == HTTOP || resizeHitTest_ == HTTOPLEFT || resizeHitTest_ == HTTOPRIGHT)
                next.top += dy;
            if (resizeHitTest_ == HTBOTTOM || resizeHitTest_ == HTBOTTOMLEFT || resizeHitTest_ == HTBOTTOMRIGHT)
                next.bottom += dy;
            const int minWidth = Ui::Scale(820, GetDpiForWindow(hwnd_));
            const int minHeight = Ui::Scale(650, GetDpiForWindow(hwnd_));
            if (next.right - next.left < minWidth) {
                if (resizeHitTest_ == HTLEFT || resizeHitTest_ == HTTOPLEFT || resizeHitTest_ == HTBOTTOMLEFT)
                    next.left = next.right - minWidth;
                else next.right = next.left + minWidth;
            }
            if (next.bottom - next.top < minHeight) {
                if (resizeHitTest_ == HTTOP || resizeHitTest_ == HTTOPLEFT || resizeHitTest_ == HTTOPRIGHT)
                    next.top = next.bottom - minHeight;
                else next.bottom = next.top + minHeight;
            }
            SetWindowPos(hwnd_, nullptr, next.left, next.top, next.right - next.left, next.bottom - next.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        if (!IsZoomed(hwnd_)) {
            const int resizeEdge = ResizeEdgeAt(hwnd_, point);
            if (const LPCWSTR cursor = ResizeCursor(resizeEdge)) {
                SetCursor(LoadCursorW(nullptr, cursor));
                return 0;
            }
        }
        if (activeSplitter_ != 0) {
            const int delta = point.y - splitterDragOriginY_;
            if (activeSplitter_ == 1) buttonSectionHeight_ = splitterDragOriginSize_ + delta;
            else inputSectionHeight_ = splitterDragOriginSize_ - delta;
            Layout();
            return 0;
        }
        const int nextHot = PtInRect(&upperSplitterRect_, point) ? 1 :
                            (PtInRect(&lowerSplitterRect_, point) ? 2 : 0);
        if (nextHot != hotSplitter_) {
            hotSplitter_ = nextHot;
            SetCursor(LoadCursorW(nullptr, nextHot != 0 ? IDC_SIZENS : IDC_ARROW));
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        if (nextHot != 0) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd_, 0};
            TrackMouseEvent(&tracking);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (hotSplitter_ != 0 && activeSplitter_ == 0) {
            hotSplitter_ = 0;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (resizeHitTest_ != HTNOWHERE) {
            resizeHitTest_ = HTNOWHERE;
            ReleaseCapture();
            PersistUiState();
            return 0;
        }
        if (activeSplitter_ != 0) {
            activeSplitter_ = 0;
            ReleaseCapture();
            Layout();
            PersistUiState();
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        resizeHitTest_ = HTNOWHERE;
        if (activeSplitter_ != 0) {
            activeSplitter_ = 0;
            Layout();
            PersistUiState();
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC targetDc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        HDC dc = nullptr;
        HPAINTBUFFER buffer = BeginBufferedPaint(targetDc, &client, BPBF_COMPATIBLEBITMAP, nullptr, &dc);
        if (buffer == nullptr) dc = targetDc;
        const UINT dpi = GetDpiForWindow(hwnd_);
        const auto s = [dpi](int value) { return Ui::Scale(value, dpi); };
        const int headerHeight = s(60);
        HBRUSH windowBrush = CreateSolidBrush(Ui::Window);
        FillRect(dc, &client, windowBrush);
        DeleteObject(windowBrush);
        RECT terminalSurface{0, upperSplitterRect_.bottom, client.right, lowerSplitterRect_.top};
        HBRUSH surfaceBrush = CreateSolidBrush(Ui::Surface);
        FillRect(dc, &terminalSurface, surfaceBrush);
        DeleteObject(surfaceBrush);
        HPEN line = CreatePen(PS_SOLID, 1, Ui::Border);
        HGDIOBJ oldPen = SelectObject(dc, line);
        MoveToEx(dc, 0, headerHeight, nullptr); LineTo(dc, client.right, headerHeight);
        SelectObject(dc, oldPen);
        DeleteObject(line);

        if (appIcon_ != nullptr) {
            HBRUSH headerBrush = CreateSolidBrush(Ui::Window);
            DrawIconEx(dc, s(18), s(14), appIcon_, s(32), s(32), 0, headerBrush, DI_NORMAL);
            DeleteObject(headerBrush);
        }

        Ui::DrawRoundedRect(dc, buttonCardRect_, Ui::Window, Ui::Border, s(12));

        Ui::DrawRoundedRect(dc, terminalCardRect_,
                            Ui::Window, Ui::Border, s(12));
        HPEN cardLine = CreatePen(PS_SOLID, 1, Ui::Border);
        HGDIOBJ oldCardPen = SelectObject(dc, cardLine);
        MoveToEx(dc, terminalCardRect_.left, terminalCardRect_.top + s(48), nullptr);
        LineTo(dc, terminalCardRect_.right, terminalCardRect_.top + s(48));
        SelectObject(dc, oldCardPen); DeleteObject(cardLine);

        Ui::DrawRoundedRect(dc, inputGroupRect_, Ui::Window, Ui::Border, s(10));
        HPEN inputLine = CreatePen(PS_SOLID, 1, Ui::Border);
        HGDIOBJ oldInputPen = SelectObject(dc, inputLine);
        const int prefixRight = inputGroupRect_.left + Ui::CompactScale(100, dpi);
        MoveToEx(dc, prefixRight, inputGroupRect_.top, nullptr);
        LineTo(dc, prefixRight, inputGroupRect_.bottom);
        SelectObject(dc, oldInputPen); DeleteObject(inputLine);

        auto drawSplitter = [&](RECT rect, bool active) {
            const COLORREF color = active ? Ui::Primary : Ui::BorderStrong;
            const int centerY = (rect.top + rect.bottom) / 2;
            HPEN splitterPen = CreatePen(PS_SOLID, std::max(1, s(2)), color);
            HGDIOBJ previous = SelectObject(dc, splitterPen);
            MoveToEx(dc, client.right / 2 - s(24), centerY, nullptr);
            LineTo(dc, client.right / 2 + s(24), centerY);
            SelectObject(dc, previous); DeleteObject(splitterPen);
        };
        drawSplitter(upperSplitterRect_, activeSplitter_ == 1 || hotSplitter_ == 1);
        drawSplitter(lowerSplitterRect_, activeSplitter_ == 2 || hotSplitter_ == 2);

        auto drawStatusDot = [&](int x, COLORREF color) {
            const float scale = 0.75F * static_cast<float>(dpi) / 96.0F;
            const float diameter = 12.0F * scale;
            const float left = static_cast<float>(x) * scale;
            const float top = (static_cast<float>(headerHeight) - diameter) / 2.0F;
            DrawStatusIndicator(dc, left, top, diameter, color);
        };
        drawStatusDot(260, Ui::Primary);
        const auto& terminal = CurrentTerminal();
        drawStatusDot(384, terminal.ready ? Ui::Success : Ui::Danger);
        drawStatusDot(544, terminal.ready ? (terminal.executor.IsBusy() ? RGB(224, 157, 38) : Ui::Success) : Ui::Danger);
        if (buffer != nullptr) EndBufferedPaint(buffer, TRUE);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        Layout();
        InvalidateRect(maximize_, nullptr, FALSE);
        return 0;
    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        RecreateFonts();
        Layout();
        InvalidateRect(hwnd_, nullptr, TRUE);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, TRANSPARENT);
        if (control == connection_) SetTextColor(dc, CurrentTerminal().ready ? Ui::Success : Ui::Danger);
        else if (control == adminMode_) SetTextColor(dc, RGB(35, 96, 180));
        else if (control == runtimeState_) SetTextColor(dc, Ui::TextMuted);
        else SetTextColor(dc, Ui::Text);
        if (control == inputPrefix_) {
            SetBkMode(dc, OPAQUE); SetBkColor(dc, Ui::Window);
            return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
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
            if (item->CtlID == IdTerminalPowerShell || item->CtlID == IdTerminalWsl) {
                const TerminalKind kind = item->CtlID == IdTerminalWsl ? TerminalKind::Wsl : TerminalKind::PowerShell;
                const auto& terminal = Context(kind);
                DrawTerminalButton(*item, kind == activeTerminal_, terminal.ready, terminal.executor.IsBusy());
                return TRUE;
            }
            DrawActionButton(*item, item->CtlID == IdExecute);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IdTerminalPowerShell: SwitchTerminal(TerminalKind::PowerShell); return 0;
        case IdTerminalWsl: SwitchTerminal(TerminalKind::Wsl); return 0;
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
        if (wParam == WM_APP_INPUT_SCROLL_HIDE_TIMER) {
            HideInputScrollBar();
            return 0;
        }
        if (wParam == WM_APP_RESTART_TIMER_PS || wParam == WM_APP_RESTART_TIMER_WSL) {
            const TerminalKind kind = wParam == WM_APP_RESTART_TIMER_WSL ? TerminalKind::Wsl : TerminalKind::PowerShell;
            auto& terminal = Context(kind);
            KillTimer(hwnd_, wParam);
            if (!shuttingDown_ && terminal.restartAttempts < 3) {
                ++terminal.restartAttempts;
                if (!StartTerminal(kind)) {
                    AppendOutput(kind, L"[快捷控制台] 终端重启失败：" + terminal.session.LastError() + L"\r\n");
                    SetTimer(hwnd_, wParam, 400, nullptr);
                } else {
                    AppendOutput(kind, L"[快捷控制台] 终端已重新建立。\r\n");
                }
            } else if (terminal.restartAttempts >= 3 && kind == activeTerminal_) {
                SetStatus(L"●  终端连续启动失败，请点击“重置”");
            }
            return 0;
        }
        break;
    case WM_APP_INPUT_SCROLLED:
        ShowInputScrollBar();
        return 0;
    case WM_APP_REFRESH_SCROLLS:
        RefreshVisibleScrollIndicators();
        return 0;
    case WM_APP_TERMINAL_OUTPUT: {
        auto* packet = reinterpret_cast<TerminalOutputPacket*>(lParam);
        if (packet != nullptr) {
            auto& terminal = Context(packet->kind);
            if (packet->generation == terminal.generation) {
                const bool elevationStarted = RequestElevationIfNeeded(packet->kind, packet->text);
                auto result = terminal.executor.ConsumeOutput(packet->text);
                AppendOutput(packet->kind, result.display);
                if (result.exitCode.has_value()) {
                    bool linuxRetryStarted = false;
                    if (packet->kind == TerminalKind::Wsl && terminal.linuxElevationPending &&
                        *result.exitCode != 0 && !terminal.currentCommand.empty()) {
                        const std::wstring command = terminal.currentCommand;
                        terminal.linuxElevationPending = false;
                        terminal.permissionProbe.clear();
                        linuxRetryStarted = ExecuteManagedCommand(TerminalKind::Wsl, command, true);
                        if (linuxRetryStarted) {
                            AppendOutput(TerminalKind::Wsl,
                                         L"\r\n[快捷控制台] 权限不足，正在通过 sudo 重试；如有提示请在终端输入密码。\r\n");
                            if (packet->kind == activeTerminal_) SetStatus(L"●  正在请求 WSL sudo 权限");
                        }
                    }
                    if (!elevationStarted && !linuxRetryStarted && packet->kind == activeTerminal_) {
                        SetStatus(*result.exitCode == 0 ? L"●  完成，退出码 0" : L"●  失败，退出码 " + std::to_wstring(*result.exitCode));
                    }
                    if (!linuxRetryStarted) {
                        terminal.currentCommand.clear();
                        terminal.permissionProbe.clear();
                        terminal.linuxElevationPending = false;
                    }
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
            auto& terminal = Context(packet->kind);
            if (packet->generation == terminal.generation && !shuttingDown_) {
                terminal.ready = false;
                terminal.executor.Reset();
                UpdateBusyState();
                AppendOutput(packet->kind, L"[快捷控制台] 终端已退出，正在重新建立会话……\r\n");
                const UINT_PTR timer = packet->kind == TerminalKind::Wsl ? WM_APP_RESTART_TIMER_WSL : WM_APP_RESTART_TIMER_PS;
                if (terminal.restartAttempts < 3) SetTimer(hwnd_, timer, 400, nullptr);
                if (packet->kind == activeTerminal_) SwitchTerminal(packet->kind, false);
            }
            delete packet;
        }
        return 0;
    }
    case WM_CLOSE:
        shuttingDown_ = true;
        PersistUiState();
        EnableWindow(buttonPanel_.Hwnd(), FALSE);
        for (auto& terminal : terminals_) terminal->session.Stop();
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
