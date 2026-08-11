#pragma once

#include "AboutLayout.h"
#include "UpdateCoordinator.h"

#include <windows.h>

class AboutWindow final {
public:
    AboutWindow() = default;
    ~AboutWindow();

    AboutWindow(const AboutWindow&) = delete;
    AboutWindow& operator=(const AboutWindow&) = delete;

    void Show(HWND owner, HINSTANCE instance, UpdateCoordinator& coordinator);
    [[nodiscard]] HWND Hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool Create(HWND owner, HINSTANCE instance, UpdateCoordinator& coordinator);
    bool CreateControls();
    void RecreateFonts();
    void Layout();
    void ApplyState(UpdateViewState state);
    void UpdateControls();
    void DrawButton(const DRAWITEMSTRUCT& item) const;
    void DrawSurface(HDC dc);
    void OpenRepository() const;

    HWND hwnd_ = nullptr;
    HWND title_ = nullptr;
    HWND currentVersion_ = nullptr;
    HWND developer_ = nullptr;
    HWND status_ = nullptr;
    HWND notes_ = nullptr;
    HWND check_ = nullptr;
    HWND download_ = nullptr;
    HWND pauseResume_ = nullptr;
    HWND background_ = nullptr;
    HWND cancel_ = nullptr;
    HWND acceleration_ = nullptr;
    HWND nextNode_ = nullptr;
    HWND install_ = nullptr;
    HWND repository_ = nullptr;
    HINSTANCE instance_ = nullptr;
    UpdateCoordinator* coordinator_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    UINT fontDpi_{};
    AboutLayout layout_{};
    UpdateViewState state_{};
};
