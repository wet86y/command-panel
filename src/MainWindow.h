#pragma once

#include "ButtonPanel.h"
#include "CommandExecutor.h"
#include "ConfigManager.h"
#include "TerminalParser.h"
#include "TerminalSession.h"
#include "TabBar.h"

#include <windows.h>

#include <string>
#include <vector>

constexpr UINT WM_APP_TERMINAL_OUTPUT = WM_APP + 1;
constexpr UINT WM_APP_TERMINAL_EXITED = WM_APP + 2;
constexpr UINT WM_APP_RESTART_TIMER = 1;

struct TerminalOutputPacket
{
    uint64_t generation;
    std::wstring text;
};

struct TerminalExitPacket
{
    uint64_t generation;
};

class MainWindow
{
public:
    MainWindow();
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
    void AppendOutput(std::wstring_view text);
    void ClearOutput();
    void ExitCommand();
    void SetStatus(const std::wstring& text);
    void OnTerminalBytes(uint64_t generation, std::string bytes);
    void OnTerminalExit(uint64_t generation);
    bool StartTerminal();
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

    HWND hwnd_ = nullptr;
    HWND output_ = nullptr;
    HWND input_ = nullptr;
    HWND runtimeState_ = nullptr;
    HWND title_ = nullptr;
    HWND adminMode_ = nullptr;
    HWND connection_ = nullptr;
    HWND terminalTitle_ = nullptr;
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
    ConfigManager config_;
    TerminalParser parser_;
    TerminalSession session_;
    CommandExecutor executor_;
    std::vector<std::wstring> history_;
    int historyPosition_ = -1;
    bool shuttingDown_ = false;
    bool terminalReady_ = false;
    bool configLoadSucceeded_ = true;
    int restartAttempts_ = 0;
    uint64_t currentGeneration_ = 0;
    int activeTab_ = 0;
    short terminalColumns_ = 120;
    short terminalRows_ = 30;
    int buttonSectionHeight_ = -1;
    int inputSectionHeight_ = -1;
    int activeSplitter_ = 0;
    int hotSplitter_ = 0;
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
