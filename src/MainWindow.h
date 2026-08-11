#pragma once

#include "ButtonPanel.h"
#include "CommandExecutor.h"
#include "ConfigManager.h"
#include "TerminalParser.h"
#include "TerminalSession.h"
#include "TerminalTypes.h"
#include "TerminalView.h"
#include "TabBar.h"

#include <windows.h>

#include <string>
#include <array>
#include <memory>
#include <optional>
#include <vector>

constexpr UINT WM_APP_TERMINAL_OUTPUT = WM_APP + 1;
constexpr UINT WM_APP_TERMINAL_EXITED = WM_APP + 2;
constexpr UINT WM_APP_INPUT_SCROLLED = WM_APP + 4;
constexpr UINT WM_APP_REFRESH_SCROLLS = WM_APP + 6;
constexpr UINT WM_APP_RESTART_TIMER_PS = 10;
constexpr UINT WM_APP_RESTART_TIMER_WSL = 11;
constexpr UINT WM_APP_INPUT_SCROLL_HIDE_TIMER = 3;

struct TerminalOutputPacket
{
    TerminalKind kind;
    uint64_t generation;
    std::wstring text;
};

struct TerminalExitPacket
{
    TerminalKind kind;
    uint64_t generation;
};

class MainWindow
{
public:
    explicit MainWindow(bool elevated, std::optional<TerminalKind> startupTerminal = std::nullopt,
                        std::wstring startupCommand = {});
    ~MainWindow();

    bool Create(HINSTANCE instance);
    HWND Hwnd() const { return hwnd_; }
    bool Initialize();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK InputProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT, WPARAM, LPARAM);
    LRESULT HandleInputMessage(HWND, UINT, WPARAM, LPARAM);
    void Layout();
    void RecreateFonts();
    void CalculateTerminalGrid(short& columns, short& rows) const;
    void SwitchTerminal(TerminalKind kind, bool focus = true);
    void RefreshTabs();
    void RefreshButtons();
    void OnTabSelected(int index);
    void AddTab();
    bool RenameTab(int index, const std::wstring& name);
    void DeleteTab(int index);
    void OnButton(int index);
    void OnContext(int index, POINT point);
    void ExecuteButton(int index);
    void ExecuteInput();
    bool ExecuteManagedCommand(TerminalKind kind, const std::wstring& command, bool elevateWsl = false);
    bool RequestElevationIfNeeded(TerminalKind kind, std::wstring_view output);
    bool LaunchElevatedTerminal(TerminalKind kind);
    void AppendOutput(TerminalKind kind, std::wstring_view text);
    void ShowInputScrollBar();
    void HideInputScrollBar();
    bool HandleTextWheel(HWND target, int& remainder, WPARAM wParam);
    void RefreshVisibleScrollIndicators();
    void ShowScrollIndicator(HWND target, HWND indicator, UINT timer);
    void HideScrollIndicator(HWND indicator, UINT timer);
    static LRESULT CALLBACK ScrollIndicatorProc(HWND, UINT, WPARAM, LPARAM);
    void ClearOutput();
    void ExitCommand();
    void SetStatus(const std::wstring& text);
    void OnTerminalBytes(TerminalKind kind, uint64_t generation, std::string bytes);
    void OnTerminalExit(TerminalKind kind, uint64_t generation);
    bool StartTerminal(TerminalKind kind);
    void ResetTerminal(bool ask);
    void ShowContextMenu(int index, POINT point);
    void EditButton(int index);
    void AddButton();
    void DuplicateButton(int index);
    void DeleteButton(int index);
    void MoveButton(int index, int direction);
    bool SaveConfig();
    void UpdateBusyState();
    void AddDiagnostic(const std::wstring& text);
    void PersistUiState();

    struct TerminalContext
    {
        explicit TerminalContext(TerminalKind value) : kind(value), executor(session, value) {}
        TerminalKind kind;
        TerminalSession session;
        CommandExecutor executor;
        TerminalParser parser;
        TerminalModel model;
        std::vector<std::wstring> history;
        int historyPosition = -1;
        bool ready = false;
        int restartAttempts = 0;
        uint64_t generation = 0;
        std::wstring currentCommand;
        std::wstring permissionProbe;
        bool elevationRequested = false;
        bool linuxElevationPending = false;
        bool windowsElevationRequired = false;
        bool elevationOnFailure = false;
    };
    TerminalContext& Context(TerminalKind kind) { return *terminals_[TerminalIndex(kind)]; }
    const TerminalContext& Context(TerminalKind kind) const { return *terminals_[TerminalIndex(kind)]; }
    TerminalContext& CurrentTerminal() { return Context(activeTerminal_); }
    const TerminalContext& CurrentTerminal() const { return Context(activeTerminal_); }

    HWND hwnd_ = nullptr;
    HWND outputHost_ = nullptr;
    HWND input_ = nullptr;
    HWND inputHost_ = nullptr;
    HWND inputScroll_ = nullptr;
    HWND runtimeState_ = nullptr;
    HWND title_ = nullptr;
    HWND adminMode_ = nullptr;
    HWND connection_ = nullptr;
    HWND terminalPowerShell_ = nullptr;
    HWND terminalWsl_ = nullptr;
    HWND clear_ = nullptr;
    HWND ctrlC_ = nullptr;
    HWND reset_ = nullptr;
    HWND inputPrefix_ = nullptr;
    HWND execute_ = nullptr;
    HWND minimize_ = nullptr;
    HWND maximize_ = nullptr;
    HWND close_ = nullptr;
    HWND resizeEdges_[8]{};
    WNDPROC inputOriginalProc_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HFONT outputFont_ = nullptr;
    HFONT headerFont_ = nullptr;
    HFONT sectionFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HICON appIcon_ = nullptr;
    ULONG_PTR gdiplusToken_ = 0;
    UINT fontDpi_ = 0;

    TabBar tabBar_;
    ButtonPanel buttonPanel_;
    TerminalView terminalView_;
    ConfigManager config_;
    std::array<std::unique_ptr<TerminalContext>, 2> terminals_;
    int inputWheelRemainder_ = 0;
    bool inputScrollVisible_ = false;
    bool shuttingDown_ = false;
    bool elevated_ = false;
    bool configLoadSucceeded_ = true;
    TerminalKind activeTerminal_ = TerminalKind::PowerShell;
    int activeTab_ = 0;
    short terminalColumns_ = 120;
    short terminalRows_ = 30;
    int buttonSectionHeight_ = -1;
    int inputSectionHeight_ = -1;
    int activeSplitter_ = 0;
    int hotSplitter_ = 0;
    std::wstring startupCommand_;
    std::optional<TerminalKind> startupTerminal_;
    int splitterDragOriginY_ = 0;
    int splitterDragOriginSize_ = 0;
    RECT upperSplitterRect_{};
    RECT lowerSplitterRect_{};
    RECT buttonCardRect_{};
    RECT terminalCardRect_{};
    RECT inputGroupRect_{};
    int resizeHitTest_ = HTNOWHERE;
    POINT resizeDragOrigin_{};
    RECT resizeWindowOrigin_{};
};
