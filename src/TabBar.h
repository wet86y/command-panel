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
    using RenameCallback = std::function<bool(int, const std::wstring&)>;
    using DeleteCallback = std::function<void(int)>;

    ~TabBar();
    bool Create(HWND parent);
    HWND Hwnd() const { return hwnd_; }
    void SetTabs(const std::vector<std::wstring>& names, int active);
    void SetCallbacks(SelectCallback select, AddCallback add, RenameCallback rename, DeleteCallback remove);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT, WPARAM, LPARAM);
    LRESULT HandleEditMessage(HWND, UINT, WPARAM, LPARAM);
    int HitTest(POINT point) const;
    RECT AddRect() const;
    bool CommitEdit();
    void CancelEdit();
    void BeginEdit(int index);

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    UINT fontDpi_ = 0;
    std::vector<std::wstring> names_;
    std::vector<RECT> tabRects_;
    int active_ = 0;
    int hot_ = -2;
    bool trackingMouse_ = false;
    HWND edit_ = nullptr;
    WNDPROC editOriginalProc_ = nullptr;
    int editIndex_ = -1;
    bool endingEdit_ = false;
    SelectCallback selectCallback_;
    AddCallback addCallback_;
    RenameCallback renameCallback_;
    DeleteCallback deleteCallback_;
};
