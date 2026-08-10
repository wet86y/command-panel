#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

class TabBar
{
public:
    using SelectCallback = std::function<void(int)>;
    using AddCallback = std::function<void()>;
    using RenameCallback = std::function<void(int)>;

    ~TabBar();
    bool Create(HWND parent);
    HWND Hwnd() const { return hwnd_; }
    void SetTabs(const std::vector<std::wstring>& names, int active);
    void SetCallbacks(SelectCallback select, AddCallback add, RenameCallback rename);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT, WPARAM, LPARAM);
    int HitTest(POINT point) const;
    RECT AddRect() const;

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    std::vector<std::wstring> names_;
    std::vector<RECT> tabRects_;
    int active_ = 0;
    SelectCallback selectCallback_;
    AddCallback addCallback_;
    RenameCallback renameCallback_;
};
