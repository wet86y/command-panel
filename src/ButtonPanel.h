#pragma once

#include "ConfigManager.h"

#include <windows.h>

#include <functional>
#include <vector>

class ButtonPanel
{
public:
    using ClickCallback = std::function<void(int)>;
    using ContextCallback = std::function<void(int, POINT)>;

    ~ButtonPanel();
    bool Create(HWND parent);
    HWND Hwnd() const { return hwnd_; }
    void SetCallbacks(ClickCallback click, ContextCallback context);
    void SetButtons(const std::vector<CommandButton>& buttons);
    void SetBusy(bool busy);
    void Layout();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT, WPARAM, LPARAM);
    int ButtonIndex(HWND button) const;
    void Rebuild(const std::vector<CommandButton>& buttons);
    void ScrollTo(int position);

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    std::vector<HWND> controls_;
    std::vector<bool> enabled_;
    ClickCallback clickCallback_;
    ContextCallback contextCallback_;
    bool busy_ = false;
    int scrollPosition_ = 0;
};
