#pragma once

#include "ButtonLayout.h"
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
    void SetAvailability(std::vector<bool> availability);
    void Layout();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT, WPARAM, LPARAM);
    void Rebuild(const std::vector<CommandButton>& buttons);
    void ScrollTo(int position, bool revealIndicator);
    void ShowScrollIndicator();
    void HideScrollIndicator();
    int HitTest(POINT point) const;
    bool IsCardEnabled(int index) const;
    int CardCount() const { return static_cast<int>(buttons_.size()) + 1; }

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    UINT fontDpi_ = 0;
    std::vector<CommandButton> buttons_;
    std::vector<bool> availability_;
    ButtonLayoutResult layout_;
    ClickCallback clickCallback_;
    ContextCallback contextCallback_;
    bool busy_ = false;
    bool trackingMouse_ = false;
    bool scrollIndicatorVisible_ = false;
    int scrollPosition_ = 0;
    int wheelRemainder_ = 0;
    int hotIndex_ = -1;
    int pressedIndex_ = -1;
    int focusedIndex_ = -1;
};
