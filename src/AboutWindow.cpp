#include "AboutWindow.h"

#include "UiTheme.h"
#include "Version.h"

#include <shellapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <string>

namespace {
constexpr int IdAboutTitle = 3101;
constexpr int IdAboutCurrentVersion = 3102;
constexpr int IdAboutDeveloper = 3103;
constexpr int IdAboutStatus = 3104;
constexpr int IdAboutNotes = 3105;
constexpr int IdAboutCheck = 3106;
constexpr int IdAboutDownload = 3107;
constexpr int IdAboutPauseResume = 3108;
constexpr int IdAboutBackground = 3109;
constexpr int IdAboutCancel = 3110;
constexpr int IdAboutAcceleration = 3111;
constexpr int IdAboutNextNode = 3112;
constexpr int IdAboutInstall = 3113;
constexpr int IdAboutRepository = 3114;
constexpr UINT MsgAboutState = WM_APP + 80;

void SetControlFont(HWND control, HFONT font)
{
    if (control != nullptr) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void Move(HWND control, const RECT& rect)
{
    if (control == nullptr) return;
    MoveWindow(control, rect.left, rect.top, std::max(1L, rect.right - rect.left),
               std::max(1L, rect.bottom - rect.top), TRUE);
}

void SetVisible(HWND control, bool visible)
{
    if (control != nullptr) ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
}

bool IsPrimaryButton(int id)
{
    return id == IdAboutCheck || id == IdAboutDownload || id == IdAboutInstall;
}
}

AboutWindow::~AboutWindow()
{
    if (hwnd_ != nullptr) DestroyWindow(hwnd_);
    if (titleFont_ != nullptr) DeleteObject(titleFont_);
    if (bodyFont_ != nullptr) DeleteObject(bodyFont_);
}

void AboutWindow::Show(HWND owner, HINSTANCE instance, UpdateCoordinator& coordinator)
{
    if (hwnd_ == nullptr && !Create(owner, instance, coordinator)) return;
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
}

bool AboutWindow::Create(HWND owner, HINSTANCE instance, UpdateCoordinator& coordinator)
{
    constexpr wchar_t className[] = L"CommandPanelAboutWindow";
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    windowClass.lpszClassName = className;
    windowClass.hbrBackground = nullptr;
    RegisterClassExW(&windowClass);

    instance_ = instance;
    coordinator_ = &coordinator;
    const UINT dpi = GetDpiForWindow(owner);
    const auto initial = CalculateAboutLayout(Ui::Scale(640, dpi), Ui::Scale(570, dpi), dpi, AboutPresentation::Idle);
    hwnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME, className, L"关于快捷控制台",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, initial.minimumWidth + Ui::Scale(120, dpi),
        initial.minimumHeight + Ui::Scale(40, dpi), owner, nullptr, instance, this);
    if (hwnd_ == nullptr) return false;
    Ui::EnableRoundedCorners(hwnd_);
    if (!CreateControls()) {
        DestroyWindow(hwnd_);
        return false;
    }
    RecreateFonts();
    state_ = coordinator_->State();
    Layout();
    UpdateControls();
    coordinator_->SetObserver([this](const UpdateViewState& state) {
        auto* copy = new UpdateViewState(state);
        if (!PostMessageW(hwnd_, MsgAboutState, 0, reinterpret_cast<LPARAM>(copy))) delete copy;
    });
    return true;
}

bool AboutWindow::CreateControls()
{
    const auto makeStatic = [this](const wchar_t* text, int id, DWORD style = SS_LEFT) {
        return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    };
    const auto makeButton = [this](const wchar_t* text, int id, DWORD extra = 0) {
        return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | extra,
            0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    };
    title_ = makeStatic(L"快捷控制台", IdAboutTitle, SS_LEFT | SS_CENTERIMAGE);
    currentVersion_ = makeStatic(L"当前版本：1.0.0", IdAboutCurrentVersion, SS_LEFT | SS_CENTERIMAGE);
    developer_ = makeStatic(L"开发者：wet86y  ·  Apache-2.0", IdAboutDeveloper, SS_LEFT | SS_CENTERIMAGE);
    status_ = makeStatic(L"", IdAboutStatus, SS_LEFT);
    notes_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_READONLY |
        ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdAboutNotes)), instance_, nullptr);
    check_ = makeButton(L"检查更新", IdAboutCheck);
    download_ = makeButton(L"下载更新", IdAboutDownload);
    pauseResume_ = makeButton(L"暂停下载", IdAboutPauseResume);
    background_ = makeButton(L"后台下载", IdAboutBackground);
    cancel_ = makeButton(L"取消", IdAboutCancel);
    acceleration_ = CreateWindowExW(0, L"BUTTON", L"使用加速节点", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdAboutAcceleration)), instance_, nullptr);
    nextNode_ = makeButton(L"切换节点", IdAboutNextNode);
    install_ = makeButton(L"立即安装", IdAboutInstall);
    repository_ = makeButton(L"GitHub 项目", IdAboutRepository);
    return title_ && currentVersion_ && developer_ && status_ && notes_ && check_ && download_ && pauseResume_ &&
           background_ && cancel_ && acceleration_ && nextNode_ && install_ && repository_;
}

void AboutWindow::RecreateFonts()
{
    const UINT dpi = GetDpiForWindow(hwnd_);
    if (dpi == fontDpi_ && titleFont_ != nullptr && bodyFont_ != nullptr) return;
    if (titleFont_ != nullptr) DeleteObject(titleFont_);
    if (bodyFont_ != nullptr) DeleteObject(bodyFont_);
    titleFont_ = Ui::CreateFont(dpi, 14, FW_SEMIBOLD);
    bodyFont_ = Ui::CreateFont(dpi, 9);
    fontDpi_ = dpi;
    SetControlFont(title_, titleFont_);
    for (HWND control : {currentVersion_, developer_, status_, notes_, check_, download_, pauseResume_, background_,
                         cancel_, acceleration_, nextNode_, install_, repository_}) SetControlFont(control, bodyFont_);
}

void AboutWindow::Layout()
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    layout_ = CalculateAboutLayout(client.right - client.left, client.bottom - client.top,
                                   GetDpiForWindow(hwnd_), state_.presentation);
    Move(title_, layout_.name);
    Move(currentVersion_, layout_.currentVersion);
    Move(developer_, layout_.developer);
    Move(status_, layout_.status);
    Move(notes_, layout_.notes);
    Move(check_, layout_.check);
    Move(download_, layout_.download);
    Move(pauseResume_, layout_.pauseResume);
    Move(background_, layout_.background);
    Move(cancel_, layout_.cancel);
    Move(acceleration_, layout_.acceleration);
    Move(nextNode_, layout_.nextNode);
    Move(install_, layout_.install);
    Move(repository_, layout_.repository);
}

void AboutWindow::ApplyState(UpdateViewState state)
{
    state_ = std::move(state);
    Layout();
    UpdateControls();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void AboutWindow::UpdateControls()
{
    SetWindowTextW(currentVersion_, (std::wstring(L"当前版本：") + kCommandPanelVersion).c_str());
    SetWindowTextW(status_, state_.status.c_str());
    SetWindowTextW(notes_, state_.releaseNotes.empty() ? L"更新说明会在发现新版本后显示。" : state_.releaseNotes.c_str());
    const bool available = state_.presentation == AboutPresentation::Available;
    const bool downloading = state_.presentation == AboutPresentation::Downloading;
    const bool paused = state_.presentation == AboutPresentation::Paused;
    const bool completed = state_.presentation == AboutPresentation::Completed;
    const bool transfer = downloading || paused;
    const bool failed = state_.presentation == AboutPresentation::Failed || state_.presentation == AboutPresentation::Cancelled;
    SetWindowTextW(check_, failed ? L"重新检查" : L"检查更新");
    SetVisible(check_, !transfer && !completed && state_.presentation != AboutPresentation::Launching);
    SetVisible(download_, available || paused);
    SetWindowTextW(download_, paused ? L"继续下载" : L"下载更新");
    SetVisible(pauseResume_, downloading);
    SetVisible(background_, transfer);
    SetVisible(cancel_, transfer);
    SetVisible(install_, completed);
    SetVisible(acceleration_, available || transfer);
    SetVisible(nextNode_, available || transfer);
    SetVisible(notes_, available || transfer || completed || failed);
    EnableWindow(check_, state_.presentation != AboutPresentation::Checking);
    EnableWindow(nextNode_, state_.acceleration);
    SendMessageW(acceleration_, BM_SETCHECK, state_.acceleration ? BST_CHECKED : BST_UNCHECKED, 0);
    InvalidateRect(hwnd_, &layout_.progress, FALSE);
}

void AboutWindow::DrawButton(const DRAWITEMSTRUCT& item) const
{
    RECT client{};
    GetClientRect(item.hwndItem, &client);
    HDC bufferedDc = nullptr;
    HPAINTBUFFER buffer = BeginBufferedPaint(item.hDC, &client, BPBF_COMPATIBLEBITMAP, nullptr, &bufferedDc);
    HDC dc = buffer != nullptr ? bufferedDc : item.hDC;
    const bool enabled = (item.itemState & ODS_DISABLED) == 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool primary = IsPrimaryButton(static_cast<int>(item.CtlID));
    const COLORREF fill = !enabled ? Ui::Surface : (primary ? (pressed ? Ui::PrimaryPressed : Ui::Primary) :
        (pressed ? Ui::SurfaceHover : Ui::Window));
    const COLORREF border = primary ? fill : Ui::BorderStrong;
    Ui::DrawRoundedRect(dc, client, fill, border, Ui::Scale(7, GetDpiForWindow(hwnd_)));
    const HFONT font = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ old = font != nullptr ? SelectObject(dc, font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, !enabled ? Ui::TextMuted : (primary ? RGB(255, 255, 255) : Ui::Text));
    wchar_t text[128]{};
    GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
    DrawTextW(dc, text, -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (old != nullptr) SelectObject(dc, old);
    if (buffer != nullptr) EndBufferedPaint(buffer, TRUE);
}

void AboutWindow::DrawSurface(HDC target)
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    HDC bufferedDc = nullptr;
    HPAINTBUFFER buffer = BeginBufferedPaint(target, &client, BPBF_COMPATIBLEBITMAP, nullptr, &bufferedDc);
    HDC dc = buffer != nullptr ? bufferedDc : target;
    HBRUSH background = CreateSolidBrush(Ui::Surface);
    FillRect(dc, &client, background);
    DeleteObject(background);
    Ui::DrawRoundedRect(dc, layout_.productCard, Ui::Window, Ui::Border, Ui::Scale(12, GetDpiForWindow(hwnd_)));
    Ui::DrawRoundedRect(dc, layout_.updateCard, Ui::Window, Ui::Border, Ui::Scale(12, GetDpiForWindow(hwnd_)));
    HBRUSH iconBrush = CreateSolidBrush(Ui::PrimarySoft);
    FillRect(dc, &layout_.icon, iconBrush);
    DeleteObject(iconBrush);
    HICON icon = LoadIconW(instance_, MAKEINTRESOURCEW(101));
    if (icon != nullptr) DrawIconEx(dc, layout_.icon.left + Ui::Scale(8, GetDpiForWindow(hwnd_)),
        layout_.icon.top + Ui::Scale(8, GetDpiForWindow(hwnd_)), icon,
        std::max(1L, layout_.icon.right - layout_.icon.left - Ui::Scale(16, GetDpiForWindow(hwnd_))),
        std::max(1L, layout_.icon.bottom - layout_.icon.top - Ui::Scale(16, GetDpiForWindow(hwnd_))), 0, nullptr, DI_NORMAL);
    if (state_.presentation == AboutPresentation::Downloading || state_.presentation == AboutPresentation::Paused ||
        state_.presentation == AboutPresentation::Completed) {
        Ui::DrawRoundedRect(dc, layout_.progress, Ui::Surface, Ui::Border, Ui::Scale(5, GetDpiForWindow(hwnd_)));
        if (state_.total > 0) {
            RECT fill = layout_.progress;
            fill.right = fill.left + static_cast<LONG>((fill.right - fill.left) *
                std::min(1.0, static_cast<double>(state_.received) / static_cast<double>(state_.total)));
            Ui::DrawRoundedRect(dc, fill, Ui::Primary, Ui::Primary, Ui::Scale(5, GetDpiForWindow(hwnd_)));
        }
    }
    if (buffer != nullptr) EndBufferedPaint(buffer, TRUE);
}

void AboutWindow::OpenRepository() const
{
    ShellExecuteW(hwnd_, L"open", kCommandPanelRepositoryUrl, nullptr, nullptr, SW_SHOWNORMAL);
}

LRESULT CALLBACK AboutWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<AboutWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = reinterpret_cast<AboutWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) self->hwnd_ = hwnd;
    }
    return self != nullptr ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT AboutWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        DrawSurface(dc);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        const auto minimum = CalculateAboutLayout(0, 0, GetDpiForWindow(hwnd_), state_.presentation);
        info->ptMinTrackSize = POINT{minimum.minimumWidth, minimum.minimumHeight};
        return 0;
    }
    case WM_SIZE: Layout(); return 0;
    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
            suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        RecreateFonts();
        Layout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, control == status_ ? Ui::TextMuted : Ui::Text);
        return reinterpret_cast<LRESULT>(GetStockObject(HOLLOW_BRUSH));
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, Ui::Text);
        SetBkColor(dc, Ui::Surface);
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item != nullptr && item->CtlType == ODT_BUTTON) { DrawButton(*item); return TRUE; }
        break;
    }
    case WM_COMMAND:
        if (coordinator_ == nullptr) break;
        switch (LOWORD(wParam)) {
        case IdAboutCheck: coordinator_->Check(); return 0;
        case IdAboutDownload: coordinator_->DownloadOrResume(); return 0;
        case IdAboutPauseResume: coordinator_->Pause(); return 0;
        case IdAboutBackground:
            coordinator_->ContinueInBackground();
            DestroyWindow(hwnd_);
            return 0;
        case IdAboutCancel: coordinator_->Cancel(); return 0;
        case IdAboutAcceleration:
            coordinator_->SetAcceleration(SendMessageW(acceleration_, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        case IdAboutNextNode: coordinator_->NextNode(); return 0;
        case IdAboutInstall: coordinator_->Install(); return 0;
        case IdAboutRepository: OpenRepository(); return 0;
        default: break;
        }
        break;
    case MsgAboutState: {
        std::unique_ptr<UpdateViewState> state(reinterpret_cast<UpdateViewState*>(lParam));
        if (state) ApplyState(std::move(*state));
        return 0;
    }
    case WM_CLOSE:
        if (coordinator_ != nullptr) coordinator_->AboutClosed();
        DestroyWindow(hwnd_);
        return 0;
    case WM_NCDESTROY:
        if (coordinator_ != nullptr) coordinator_->SetObserver({});
        {
        const HWND window = hwnd_;
        hwnd_ = nullptr;
        return DefWindowProcW(window, message, wParam, lParam);
        }
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
