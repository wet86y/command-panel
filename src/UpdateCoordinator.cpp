#include "UpdateCoordinator.h"

#include "LocalUpdateDiagnostics.h"
#include "Version.h"

#include <algorithm>
#include <format>
#include <limits>
#include <utility>

namespace {
std::wstring WidenUtf8(const std::string& value)
{
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(std::max(0, length)), L'\0');
    if (length > 0) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

desktop_update_kit::ClientOptions MakeClientOptions()
{
    return desktop_update_kit::ClientOptions{
        kCommandPanelApplicationId,
        "wet86y/command-panel",
        "quick-command-panel.exe",
        "quick-command-panel.exe.sha256",
        desktop_update_kit::Version{kCommandPanelVersionMajor, kCommandPanelVersionMinor, kCommandPanelVersionPatch}};
}

AboutPresentation MapPresentation(desktop_update_kit::SessionState state)
{
    switch (state) {
    case desktop_update_kit::SessionState::downloading: return AboutPresentation::Downloading;
    case desktop_update_kit::SessionState::paused: return AboutPresentation::Paused;
    case desktop_update_kit::SessionState::completed: return AboutPresentation::Completed;
    case desktop_update_kit::SessionState::failed: return AboutPresentation::Failed;
    case desktop_update_kit::SessionState::cancelled: return AboutPresentation::Cancelled;
    case desktop_update_kit::SessionState::idle: return AboutPresentation::Idle;
    }
    return AboutPresentation::Idle;
}
}

struct UpdateCoordinator::ObserverSlot {
    std::recursive_mutex mutex;
    UpdateObserver observer;
    bool active{true};
};

UpdateCoordinator::UpdateCoordinator(HINSTANCE instance, std::filesystem::path executable, ExitCallback exitCallback)
    : instance_(instance), executable_(std::move(executable)), exitCallback_(std::move(exitCallback)),
      client_(MakeClientOptions()), session_(client_)
{
    session_.set_changed_callback([this](const desktop_update_kit::SessionSnapshot& snapshot) {
        ApplySnapshot(snapshot);
    });
}

UpdateCoordinator::~UpdateCoordinator() { (void)Shutdown(INFINITE); }

void UpdateCoordinator::SetObserver(UpdateObserver observer)
{
    auto replacement = observer ? std::make_shared<ObserverSlot>() : nullptr;
    if (replacement) replacement->observer = std::move(observer);
    std::shared_ptr<ObserverSlot> previous;
    UpdateViewState state;
    {
        std::scoped_lock lock(mutex_);
        previous = std::exchange(observerSlot_, replacement);
        state = state_;
    }
    DeactivateObserver(previous);
    if (replacement) {
        std::scoped_lock lock(replacement->mutex);
        if (replacement->active && replacement->observer) replacement->observer(state);
    }
}

UpdateViewState UpdateCoordinator::State() const
{
    std::scoped_lock lock(mutex_);
    return state_;
}

void UpdateCoordinator::Check()
{
    {
        std::scoped_lock lock(mutex_);
        if (shuttingDown_ || state_.presentation == AboutPresentation::Checking ||
            state_.presentation == AboutPresentation::Downloading || state_.presentation == AboutPresentation::Paused ||
            state_.presentation == AboutPresentation::Launching) return;
    }
    if (checkThread_.joinable()) checkThread_.join();
    auto checking = State();
    checking.presentation = AboutPresentation::Checking;
    checking.status = L"正在检查 GitHub Release 更新…";
    Publish(checking);
    {
        std::scoped_lock lock(mutex_);
        checkFinished_ = false;
    }
    checkThread_ = std::jthread([this](std::stop_token token) {
        try {
            const auto release = client_.check_for_update(token);
            auto next = State();
            if (release) {
                {
                    std::scoped_lock lock(mutex_);
                    release_ = *release;
                }
                next.presentation = AboutPresentation::Available;
                next.version = Widen(desktop_update_kit::format_version(release->version));
                next.releaseNotes = Widen(release->notes);
                next.status = L"发现新版本 " + next.version + L"，请阅读说明后开始下载。";
            } else {
                next.presentation = AboutPresentation::Idle;
                next.status = L"当前已是最新版本。";
                next.version.clear();
                next.releaseNotes.clear();
            }
            Publish(next);
        } catch (const std::exception& error) {
            auto failed = State();
            failed.presentation = AboutPresentation::Failed;
            failed.status = L"检查更新失败：" + Widen(error.what());
            Publish(failed);
        } catch (...) {
            auto failed = State();
            failed.presentation = AboutPresentation::Failed;
            failed.status = L"检查更新失败。";
            Publish(failed);
        }
        {
            std::scoped_lock lock(mutex_);
            checkFinished_ = true;
        }
        checkFinishedSignal_.notify_all();
    });
}

void UpdateCoordinator::DownloadOrResume()
{
    const auto state = State();
    if (state.presentation == AboutPresentation::Paused) {
        (void)session_.resume();
        return;
    }
    std::optional<desktop_update_kit::Release> release;
    {
        std::scoped_lock lock(mutex_);
        if (shuttingDown_) return;
        release = release_;
    }
    if (release) (void)session_.start(*release, state.acceleration);
}

void UpdateCoordinator::Pause() { (void)session_.pause(); }
void UpdateCoordinator::ContinueInBackground() { (void)session_.continue_in_background(); }
void UpdateCoordinator::Cancel() { (void)session_.cancel(); }

void UpdateCoordinator::SetAcceleration(bool enabled)
{
    auto state = State();
    state.acceleration = enabled;
    Publish(state);
    (void)session_.set_acceleration(enabled);
}

void UpdateCoordinator::NextNode() { (void)session_.next_accelerated_node(); }

bool UpdateCoordinator::Install()
{
    const auto snapshot = session_.snapshot();
    if (snapshot.state != desktop_update_kit::SessionState::completed || snapshot.downloaded_path.empty() ||
        !snapshot.release || snapshot.release->sha256.empty()) {
        RecordLocalUpdateDiagnostic(L"install rejected stage=precondition");
        return false;
    }
    auto launching = State();
    launching.presentation = AboutPresentation::Launching;
    launching.status = L"更新助手已启动，快捷控制台即将退出。";
    Publish(launching);
    const auto stub = UpdaterStub();
    if (stub.empty()) {
        auto failed = State();
        failed.presentation = AboutPresentation::Failed;
        failed.status = L"内嵌更新助手不可用。";
        Publish(failed);
        RecordLocalUpdateDiagnostic(L"install failed stage=load-stub");
        return false;
    }
    RecordLocalUpdateDiagnostic(L"install requested target=" + std::filesystem::absolute(executable_).wstring() +
                                L" downloaded=" + snapshot.downloaded_path.wstring());
    const auto result = desktop_update_kit::launch_update(stub, snapshot.downloaded_path,
        std::filesystem::absolute(executable_), snapshot.release->sha256,
        static_cast<int>(GetCurrentProcessId()));
    if (!result.started) {
        auto failed = State();
        failed.presentation = AboutPresentation::Failed;
        failed.status = L"无法启动更新助手：" + Widen(result.error);
        Publish(failed);
        RecordLocalUpdateDiagnostic(L"install failed stage=launch-stub error=" + Widen(result.error));
        return false;
    }
    {
        std::scoped_lock lock(mutex_);
        installationStarted_ = true;
    }
    RecordLocalUpdateDiagnostic(L"install started stub-bytes=" + std::to_wstring(stub.size()));
    if (exitCallback_) exitCallback_();
    return true;
}

void UpdateCoordinator::AboutClosed()
{
    if (!session_.snapshot().background) (void)session_.pause_when_ui_closes();
}

bool UpdateCoordinator::Shutdown(DWORD timeoutMs)
{
    std::shared_ptr<ObserverSlot> observer;
    {
        std::scoped_lock lock(mutex_);
        if (shuttingDown_) return true;
        shuttingDown_ = true;
        observer = std::exchange(observerSlot_, {});
    }
    DeactivateObserver(observer);
    if (checkThread_.joinable()) checkThread_.request_stop();
    client_.cancel_active_requests();
    (void)session_.cancel();
    session_.set_changed_callback({});
    if (checkThread_.joinable()) checkThread_.join();
    const bool stopped = session_.wait_for_stop(timeoutMs == INFINITE
        ? std::chrono::milliseconds::max() : std::chrono::milliseconds(timeoutMs));
    const auto snapshot = session_.snapshot();
    bool installationStarted{};
    {
        std::scoped_lock lock(mutex_);
        installationStarted = installationStarted_;
    }
    if (stopped && !installationStarted && snapshot.state == desktop_update_kit::SessionState::completed)
        session_.discard_completed();
    return stopped;
}

void UpdateCoordinator::Publish(UpdateViewState state)
{
    std::shared_ptr<ObserverSlot> observer;
    UpdateViewState snapshot;
    {
        std::scoped_lock lock(mutex_);
        if (shuttingDown_) return;
        state_ = std::move(state);
        snapshot = state_;
        observer = observerSlot_;
    }
    if (observer) {
        std::scoped_lock lock(observer->mutex);
        if (observer->active && observer->observer) {
            try { observer->observer(snapshot); } catch (...) {}
        }
    }
}

void UpdateCoordinator::ApplySnapshot(const desktop_update_kit::SessionSnapshot& snapshot)
{
    auto state = State();
    state.presentation = MapPresentation(snapshot.state);
    state.background = snapshot.background;
    state.acceleration = snapshot.acceleration;
    if (snapshot.release) {
        state.version = Widen(desktop_update_kit::format_version(snapshot.release->version));
        state.releaseNotes = Widen(snapshot.release->notes);
    }
    if (snapshot.progress) {
        state.received = snapshot.progress->received;
        state.total = snapshot.progress->total;
        state.bytesPerSecond = snapshot.progress->bytes_per_second;
        state.node = Widen(snapshot.progress->node_id);
        state.connections = snapshot.progress->connections;
        state.parallelFallback = snapshot.progress->parallel_fallback;
    }
    switch (snapshot.state) {
    case desktop_update_kit::SessionState::downloading:
        state.status = std::format(L"正在下载：{} / {}，{}/s，节点 {}（{} 路{}）",
            FormatBytes(static_cast<double>(state.received)), FormatBytes(static_cast<double>(state.total)),
            FormatBytes(state.bytesPerSecond), state.node.empty() ? L"准备中" : state.node,
            state.connections, state.parallelFallback ? L"，已回退单路" : L"");
        break;
    case desktop_update_kit::SessionState::paused: state.status = L"下载已暂停。"; break;
    case desktop_update_kit::SessionState::completed: state.status = L"下载和 SHA-256 校验完成，点击“立即安装”。"; break;
    case desktop_update_kit::SessionState::failed: state.status = L"下载失败：" + Widen(snapshot.error); break;
    case desktop_update_kit::SessionState::cancelled: state.status = L"下载已取消。"; break;
    case desktop_update_kit::SessionState::idle: break;
    }
    Publish(state);
}

std::vector<std::byte> UpdateCoordinator::UpdaterStub() const
{
    HRSRC resource = FindResourceW(instance_, MAKEINTRESOURCEW(kUpdaterStubResourceId), RT_RCDATA);
    if (!resource) return {};
    HGLOBAL loaded = LoadResource(instance_, resource);
    if (!loaded) return {};
    const DWORD size = SizeofResource(instance_, resource);
    const auto* bytes = static_cast<const std::byte*>(LockResource(loaded));
    return bytes && size ? std::vector<std::byte>(bytes, bytes + size) : std::vector<std::byte>{};
}

std::wstring UpdateCoordinator::Widen(const std::string& value) { return WidenUtf8(value); }

std::wstring UpdateCoordinator::FormatBytes(double value)
{
    constexpr const wchar_t* units[]{L"B", L"KiB", L"MiB", L"GiB"};
    std::size_t unit{};
    while (value >= 1024.0 && unit + 1 < std::size(units)) { value /= 1024.0; ++unit; }
    return std::format(L"{:.1f} {}", value, units[unit]);
}

void UpdateCoordinator::DeactivateObserver(const std::shared_ptr<ObserverSlot>& slot) noexcept
{
    if (!slot) return;
    std::scoped_lock lock(slot->mutex);
    slot->active = false;
    slot->observer = {};
}
