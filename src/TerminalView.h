#pragma once

#include "TerminalModel.h"

#include <windows.h>

#include <functional>
#include <string>

class TerminalView
{
public:
    using InputCallback = std::function<void(std::string)>;

    ~TerminalView();
    bool Create(HWND parent, HINSTANCE instance);
    HWND Hwnd() const { return hwnd_; }
    void SetModel(TerminalModel* model);
    void SetInputCallback(InputCallback callback) { inputCallback_ = std::move(callback); }
    void SetFont(HFONT font);
    void OnModelChanged(bool followOutput = true);
    void ScrollToBottom();
    void CalculateGrid(short& columns, short& rows) const;

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT, WPARAM, LPARAM);
    void Paint();
    void UpdateMetrics();
    void Send(std::string value);
    void PasteClipboard();
    void CopySelection();
    void BeginSelection(POINT point);
    void UpdateSelection(POINT point);
    std::size_t VisibleStart() const;
    std::pair<std::size_t, int> CellFromPoint(POINT point) const;
    bool IsSelected(std::size_t line, int column) const;

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    TerminalModel* model_ = nullptr;
    InputCallback inputCallback_;
    int cellWidth_ = 8;
    int cellHeight_ = 18;
    int viewportRows_ = 1;
    int scrollOffset_ = 0;
    std::size_t lastTotalLines_ = 0;
    int wheelRemainder_ = 0;
    bool indicatorVisible_ = false;
    bool selecting_ = false;
    wchar_t pendingHighSurrogate_ = 0;
    bool hasSelection_ = false;
    std::size_t selectionAnchorLine_ = 0;
    int selectionAnchorColumn_ = 0;
    std::size_t selectionEndLine_ = 0;
    int selectionEndColumn_ = 0;
};
