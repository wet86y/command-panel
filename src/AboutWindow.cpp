#include "AboutWindow.h"

#include "AboutButton.h"
#include "UiTheme.h"
#include "Version.h"

#include <shellapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <memory>
#include <string>

namespace {
constexpr int IdAboutTitle = 3101;
constexpr int IdAboutVersion = 3102;
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
    return id == IdAboutDownload || id == IdAboutInstall;
}
}

AboutWindow::~AboutWindow()
{
    if (hwnd_ != nullptr) DestroyWindow(hwnd_);
    if (titleFont_ != nullptr) DeleteObject(titleFont_);
    if (bodyFont_ != nullptr) DeleteObject(bodyFont_);
    if (surfaceBrush_ != nullptr) DeleteObject(surfaceBrush_);
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
    const auto initial = CalculateAboutLayout(Ui::Scale(620, dpi), Ui::Scale(560, dpi), dpi, AboutPresentation::Idle);
    hwnd_ = CreateWindowExW(0, className, L"关于快捷控制台",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, initial.minimumWidth + Ui::Scale(110, dpi),
        initial.minimumHeight + Ui::Scale(32, dpi), owner, nullptr, instance, this);
    if (hwnd_ == nullptr) return false;
    surfaceBrush_ = CreateSolidBrush(Ui::Surface);
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
        HWND control = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | extra,
            0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        Ui::TrackOwnerDrawButton(control);
        return control;
    };
    title_ = makeStatic(L"快捷控制台", IdAboutTitle, SS_LEFT | SS_CENTERIMAGE);
    const std::wstring versionText = L"版本 " + std::wstring(kCommandPanelVersion);
    version_ = makeStatic(versionText.c_str(), IdAboutVersion, SS_LEFT | SS_CENTERIMAGE);
    developer_ = makeStatic(L"开发者：wet86y · Apache-2.0", IdAboutDeveloper, SS_LEFT | SS_CENTERIMAGE);
    repository_ = makeButton(L"GitHub 项目仓库与更新记录", IdAboutRepository);
    status_ = makeStatic(L"", IdAboutStatus, SS_LEFT);
    notes_ = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_READONLY |
        ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 1, 1, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdAboutNotes)), instance_, nullptr);
    check_ = makeButton(L"检查更新", IdAboutCheck);
    download_ = makeButton(L"下载更新", IdAboutDownload);
    pauseResume_ = makeButton(L"暂停下载", IdAboutPauseResume);
    background_ = makeButton(L"后台下载", IdAboutBackground);
    cancel_ = makeButton(L"取消", IdAboutCancel);
    acceleration_ = makeButton(L"使用加速节点", IdAboutAcceleration, BS_AUTOCHECKBOX);
    nextNode_ = makeButton(L"切换加速节点", IdAboutNextNode);
    install_ = makeButton(L"立即安装", IdAboutInstall);
    return title_ && version_ && developer_ && repository_ && status_ && notes_ && check_ && download_ && pauseResume_ &&
           background_ && cancel_ && acceleration_ && nextNode_ && install_;
}

void AboutWindow::RecreateFonts()
{
    const UINT dpi = GetDpiForWindow(hwnd_);
    if (dpi == fontDpi_ && titleFont_ != nullptr && bodyFont_ != nullptr) return;
    if (titleFont_ != nullptr) DeleteObject(titleFont_);
    if (bodyFont_ != nullptr) DeleteObject(bodyFont_);
    titleFont_ = Ui::CreateFont(dpi, 18, FW_SEMIBOLD);
    bodyFont_ = Ui::CreateFont(dpi, 9);
    fontDpi_ = dpi;
    SetControlFont(title_, titleFont_);
    for (HWND control : {version_, developer_, repository_, status_, notes_, check_, download_, pauseResume_, background_,
                         cancel_, acceleration_, nextNode_, install_}) SetControlFont(control, bodyFont_);
}

void AboutWindow::Layout()
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    layout_ = CalculateAboutLayout(client.right - client.left, client.bottom - client.top,
                                   GetDpiForWindow(hwnd_), state_.presentation);
    Move(title_, layout_.name);
    Move(version_, layout_.version);
    Move(developer_, layout_.developer);
    Move(repository_, layout_.repository);
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
    SetWindowTextW(version_, (std::wstring(L"版本 ") + kCommandPanelVersion).c_str());
    SetWindowTextW(status_, state_.status.c_str());
    SetWindowTextW(notes_, state_.releaseNotes.empty() ? L"发现新版本后将在这里显示更新说明。" : state_.releaseNotes.c_str());
    const bool available = state_.presentation == AboutPresentation::Available;
    const bool downloading = state_.presentation == AboutPresentation::Downloading;
    const bool paused = state_.presentation == AboutPresentation::Paused;
    const bool completed = state_.presentation == AboutPresentation::Completed;
    const bool transfer = downloading || paused;
    const bool failed = state_.presentation == AboutPresentation::Failed || state_.presentation == AboutPresentation::Cancelled;
    SetWindowTextW(check_, failed ? L"重新检查" : L"检查更新");
    SetVisible(check_, !available && !transfer && !completed && state_.presentation != AboutPresentation::Launching);
    SetVisible(download_, available);
    SetVisible(pauseResume_, transfer);
    SetWindowTextW(pauseResume_, paused ? L"继续下载" : L"暂停下载");
    SetVisible(background_, transfer);
    SetVisible(cancel_, transfer);
    SetVisible(install_, completed);
    SetVisible(acceleration_, available);
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
    const int id = static_cast<int>(item.CtlID);
    AboutButtonVisual visual{};
    visual.kind = id == IdAboutRepository ? AboutButtonKind::Link :
                  id == IdAboutAcceleration ? AboutButtonKind::CheckBox :
                  IsPrimaryButton(id) ? AboutButtonKind::Primary : AboutButtonKind::Secondary;
    visual.enabled = (item.itemState & ODS_DISABLED) == 0;
    visual.pressed = (item.itemState & ODS_SELECTED) != 0;
    visual.hot = Ui::IsControlHot(item.hwndItem);
    visual.focused = (item.itemState & ODS_FOCUS) != 0;
    visual.checked = (item.itemState & ODS_CHECKED) != 0 ||
                     (id == IdAboutAcceleration && SendMessageW(item.hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED);
    visual.dpi = GetDpiForWindow(hwnd_);
    const HFONT font = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ old = font != nullptr ? SelectObject(dc, font) : nullptr;
    wchar_t text[160]{};
    GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
    DrawAboutButton(dc, client, text, visual);
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
    HBRUSH background = CreateSolidBrush(Ui::Window);
    FillRect(dc, &client, background);
    DeleteObject(background);
    const UINT dpi = GetDpiForWindow(hwnd_);
    HPEN separator = CreatePen(PS_SOLID, 1, Ui::Border);
    HGDIOBJ oldPen = SelectObject(dc, separator);
    MoveToEx(dc, layout_.status.left, layout_.status.top - Ui::Scale(9, dpi), nullptr);
    LineTo(dc, layout_.status.right, layout_.status.top - Ui::Scale(9, dpi));
    SelectObject(dc, oldPen);
    DeleteObject(separator);
    if (layout_.notes.bottom > layout_.notes.top) {
        RECT label{layout_.notes.left, layout_.notes.top - Ui::Scale(21, dpi), layout_.notes.right, layout_.notes.top - Ui::Scale(3, dpi)};
        HGDIOBJ oldFont = bodyFont_ != nullptr ? SelectObject(dc, bodyFont_) : nullptr;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, Ui::Text);
        DrawTextW(dc, L"更新说明", -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (oldFont != nullptr) SelectObject(dc, oldFont);
        Ui::DrawRoundedRect(dc, layout_.notes, Ui::Surface, Ui::Border, Ui::Scale(8, dpi));
    }
    if (layout_.progress.bottom > layout_.progress.top &&
        (state_.presentation == AboutPresentation::Downloading || state_.presentation == AboutPresentation::Paused ||
         state_.presentation == AboutPresentation::Completed)) {
        Ui::DrawRoundedRect(dc, layout_.progress, Ui::Surface, Ui::Border, Ui::Scale(5, dpi));
        if (state_.total > 0) {
            RECT fill = layout_.progress;
            fill.right = fill.left + static_cast<LONG>((fill.right - fill.left) *
                std::min(1.0, static_cast<double>(state_.received) / static_cast<double>(state_.total)));
            if (fill.right > fill.left) Ui::DrawRoundedRect(dc, fill, Ui::Success, Ui::Success, Ui::Scale(5, dpi));
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
        return reinterpret_cast<LRESULT>(surfaceBrush_ != nullptr ? surfaceBrush_ : GetStockObject(LTGRAY_BRUSH));
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
        case IdAboutPauseResume:
            if (state_.presentation == AboutPresentation::Paused) coordinator_->DownloadOrResume();
            else coordinator_->Pause();
            return 0;
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
