#pragma once

#include "UpdateTypes.h"

#include <DesktopUpdateKit/UpdateKit.h>
#include <windows.h>

#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

inline constexpr int kUpdaterStubResourceId = 201;

class UpdateCoordinator final {
public:
    using ExitCallback = std::function<void()>;

    UpdateCoordinator(HINSTANCE instance, std::filesystem::path executable, ExitCallback exitCallback);
    ~UpdateCoordinator();

    UpdateCoordinator(const UpdateCoordinator&) = delete;
    UpdateCoordinator& operator=(const UpdateCoordinator&) = delete;

    void SetObserver(UpdateObserver observer);
    [[nodiscard]] UpdateViewState State() const;
    void Check();
    void DownloadOrResume();
    void Pause();
    void ContinueInBackground();
    void Cancel();
    void SetAcceleration(bool enabled);
    void NextNode();
    bool Install();
    void AboutClosed();
    bool Shutdown(DWORD timeoutMs);

private:
    struct ObserverSlot;

    void Publish(UpdateViewState state);
    void ApplySnapshot(const desktop_update_kit::SessionSnapshot& snapshot);
    std::vector<std::byte> UpdaterStub() const;
    static std::wstring Widen(const std::string& value);
    static std::wstring FormatBytes(double value);
    static void DeactivateObserver(const std::shared_ptr<ObserverSlot>& slot) noexcept;

    HINSTANCE instance_{};
    std::filesystem::path executable_;
    ExitCallback exitCallback_;
    desktop_update_kit::UpdateClient client_;
    desktop_update_kit::DownloadSession session_;
    std::jthread checkThread_;
    mutable std::mutex mutex_;
    std::condition_variable checkFinishedSignal_;
    bool checkFinished_{true};
    bool shuttingDown_{};
    bool installationStarted_{};
    UpdateViewState state_;
    std::optional<desktop_update_kit::Release> release_;
    std::shared_ptr<ObserverSlot> observerSlot_;
};
