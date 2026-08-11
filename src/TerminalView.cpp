#include "TerminalView.h"

#include "UiTheme.h"
#include "Utf.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>

namespace {
constexpr UINT IndicatorTimer = 1;
constexpr UINT IndicatorDurationMs = 1200;
constexpr COLORREF TerminalBackground = RGB(10, 16, 21);
constexpr COLORREF SelectionBackground = RGB(38, 93, 151);
}

TerminalView::~TerminalView()
{
    if (hwnd_ != nullptr) DestroyWindow(hwnd_);
}

bool TerminalView::Create(HWND parent, HINSTANCE instance)
{
    constexpr wchar_t className[] = L"CommandPanelTerminalView";
    static bool registered = false;
    if (!registered) {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = WndProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
        windowClass.lpszClassName = className;
        registered = RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    if (!registered) return false;
    hwnd_ = CreateWindowExW(0, className, nullptr,
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            0, 0, 0, 0, parent, nullptr, instance, this);
    return hwnd_ != nullptr;
}

void TerminalView::SetModel(TerminalModel* model)
{
    model_ = model;
    scrollOffset_ = 0;
    lastTotalLines_ = model_ != nullptr ? model_->TotalLineCount() : 0;
    hasSelection_ = false;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void TerminalView::SetFont(HFONT font)
{
    font_ = font;
    UpdateMetrics();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void TerminalView::UpdateMetrics()
{
    if (hwnd_ == nullptr) return;
    HDC dc = GetDC(hwnd_);
    HGDIOBJ oldFont = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    cellWidth_ = std::max(1, static_cast<int>(metrics.tmAveCharWidth));
    cellHeight_ = std::max(1, static_cast<int>(metrics.tmHeight + metrics.tmExternalLeading));
    if (oldFont != nullptr) SelectObject(dc, oldFont);
    ReleaseDC(hwnd_, dc);
    RECT client{};
    GetClientRect(hwnd_, &client);
    viewportRows_ = std::max(1, static_cast<int>(client.bottom) / cellHeight_);
}

void TerminalView::CalculateGrid(short& columns, short& rows) const
{
    RECT client{};
    if (hwnd_ != nullptr) GetClientRect(hwnd_, &client);
    columns = static_cast<short>(std::clamp(static_cast<int>(client.right) / std::max(1, cellWidth_), 20, 500));
    rows = static_cast<short>(std::clamp(static_cast<int>(client.bottom) / std::max(1, cellHeight_), 5, 300));
}

void TerminalView::OnModelChanged(bool followOutput)
{
    if (model_ != nullptr) {
        const std::size_t total = model_->TotalLineCount();
        if (scrollOffset_ > 0 && total > lastTotalLines_)
            scrollOffset_ += static_cast<int>(std::min<std::size_t>(total - lastTotalLines_, INT_MAX));
        const int maximum = std::max(0, static_cast<int>(std::min<std::size_t>(total, INT_MAX)) - viewportRows_);
        scrollOffset_ = std::clamp(scrollOffset_, 0, maximum);
        if (followOutput && scrollOffset_ == 0) scrollOffset_ = 0;
        lastTotalLines_ = total;
    }
    if (hwnd_ != nullptr) InvalidateRect(hwnd_, nullptr, FALSE);
}

void TerminalView::ScrollToBottom()
{
    scrollOffset_ = 0;
    if (hwnd_ != nullptr) InvalidateRect(hwnd_, nullptr, FALSE);
}

std::size_t TerminalView::VisibleStart() const
{
    if (model_ == nullptr) return 0;
    const std::size_t total = model_->TotalLineCount();
    const std::size_t visible = static_cast<std::size_t>(viewportRows_);
    const std::size_t bottomStart = total > visible ? total - visible : 0;
    return bottomStart > static_cast<std::size_t>(scrollOffset_)
        ? bottomStart - static_cast<std::size_t>(scrollOffset_) : 0;
}

std::pair<std::size_t, int> TerminalView::CellFromPoint(POINT point) const
{
    const int row = std::clamp(static_cast<int>(point.y) / std::max(1, cellHeight_), 0, viewportRows_ - 1);
    const int column = std::clamp(static_cast<int>(point.x) / std::max(1, cellWidth_), 0,
                                  model_ != nullptr ? model_->Columns() - 1 : 0);
    return {VisibleStart() + static_cast<std::size_t>(row), column};
}

void TerminalView::BeginSelection(POINT point)
{
    const auto [line, column] = CellFromPoint(point);
    selectionAnchorLine_ = selectionEndLine_ = line;
    selectionAnchorColumn_ = selectionEndColumn_ = column;
    selecting_ = true;
    hasSelection_ = false;
    SetCapture(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void TerminalView::UpdateSelection(POINT point)
{
    const auto [line, column] = CellFromPoint(point);
    selectionEndLine_ = line;
    selectionEndColumn_ = column;
    hasSelection_ = line != selectionAnchorLine_ || column != selectionAnchorColumn_;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool TerminalView::IsSelected(std::size_t line, int column) const
{
    if (!hasSelection_) return false;
    auto begin = std::pair{selectionAnchorLine_, selectionAnchorColumn_};
    auto end = std::pair{selectionEndLine_, selectionEndColumn_};
    if (end < begin) std::swap(begin, end);
    const auto cell = std::pair{line, column};
    return cell >= begin && cell <= end;
}

void TerminalView::CopySelection()
{
    if (!hasSelection_ || model_ == nullptr) return;
    auto begin = std::pair{selectionAnchorLine_, selectionAnchorColumn_};
    auto end = std::pair{selectionEndLine_, selectionEndColumn_};
    if (end < begin) std::swap(begin, end);
    std::wstring text;
    for (std::size_t lineIndex = begin.first; lineIndex <= end.first && lineIndex < model_->TotalLineCount(); ++lineIndex) {
        const auto& line = model_->LineAt(lineIndex);
        const int firstColumn = lineIndex == begin.first ? begin.second : 0;
        const int lastColumn = lineIndex == end.first ? end.second : static_cast<int>(line.size()) - 1;
        std::wstring row;
        for (int column = firstColumn; column <= lastColumn && column < static_cast<int>(line.size()); ++column) {
            if (line[static_cast<std::size_t>(column)].wideContinuation) continue;
            row.push_back(line[static_cast<std::size_t>(column)].character);
            row.append(line[static_cast<std::size_t>(column)].combining);
        }
        while (!row.empty() && row.back() == L' ') row.pop_back();
        text += row;
        if (lineIndex != end.first) text += L"\r\n";
    }
    if (!OpenClipboard(hwnd_)) return;
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory != nullptr) {
        void* target = GlobalLock(memory);
        if (target != nullptr) { memcpy(target, text.c_str(), bytes); GlobalUnlock(memory); SetClipboardData(CF_UNICODETEXT, memory); }
        else GlobalFree(memory);
    }
    CloseClipboard();
}

void TerminalView::PasteClipboard()
{
    if (model_ == nullptr || !OpenClipboard(hwnd_)) return;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr) {
        const auto* text = static_cast<const wchar_t*>(GlobalLock(data));
        if (text != nullptr) { Send(EncodeTerminalPaste(text, model_->BracketedPaste())); GlobalUnlock(data); }
    }
    CloseClipboard();
}

void TerminalView::Send(std::string value)
{
    if (!value.empty() && inputCallback_) inputCallback_(std::move(value));
}

void TerminalView::Paint()
{
    PAINTSTRUCT paint{};
    HDC target = BeginPaint(hwnd_, &paint);
    RECT client{};
    GetClientRect(hwnd_, &client);
    HDC dc = nullptr;
    HPAINTBUFFER buffer = BeginBufferedPaint(target, &client, BPBF_COMPATIBLEBITMAP, nullptr, &dc);
    if (buffer == nullptr) dc = target;
    HBRUSH background = CreateSolidBrush(TerminalBackground);
    FillRect(dc, &client, background);
    DeleteObject(background);
    HGDIOBJ oldFont = font_ != nullptr ? SelectObject(dc, font_) : nullptr;
    SetBkMode(dc, OPAQUE);

    if (model_ != nullptr) {
        const std::size_t start = VisibleStart();
        const std::size_t total = model_->TotalLineCount();
        for (int row = 0; row < viewportRows_; ++row) {
            const std::size_t lineIndex = start + static_cast<std::size_t>(row);
            if (lineIndex >= total) break;
            const auto& line = model_->LineAt(lineIndex);
            for (int column = 0; column < model_->Columns() && column < static_cast<int>(line.size()); ++column) {
                const auto& cell = line[static_cast<std::size_t>(column)];
                if (cell.wideContinuation) continue;
                COLORREF foreground = cell.attributes.foreground;
                COLORREF cellBackground = cell.attributes.background;
                if (cell.attributes.inverse) std::swap(foreground, cellBackground);
                if (IsSelected(lineIndex, column)) cellBackground = SelectionBackground;
                SetTextColor(dc, foreground);
                SetBkColor(dc, cellBackground);
                std::wstring glyph(1, cell.character);
                glyph += cell.combining;
                const int width = column + 1 < static_cast<int>(line.size()) &&
                                  line[static_cast<std::size_t>(column + 1)].wideContinuation ? 2 : 1;
                RECT cellRect{column * cellWidth_, row * cellHeight_,
                              std::min(static_cast<int>(client.right), (column + width) * cellWidth_),
                              (row + 1) * cellHeight_};
                ExtTextOutW(dc, column * cellWidth_, row * cellHeight_, ETO_OPAQUE | ETO_CLIPPED,
                            &cellRect, glyph.c_str(), static_cast<UINT>(glyph.size()), nullptr);
                if (cell.attributes.underline) {
                    HPEN pen = CreatePen(PS_SOLID, 1, foreground);
                    HGDIOBJ oldPen = SelectObject(dc, pen);
                    MoveToEx(dc, cellRect.left, cellRect.bottom - 2, nullptr);
                    LineTo(dc, cellRect.right, cellRect.bottom - 2);
                    SelectObject(dc, oldPen); DeleteObject(pen);
                }
            }
        }
        if (GetFocus() == hwnd_ && model_->CursorVisible() && scrollOffset_ == 0) {
            const int cursorX = model_->CursorColumn() * cellWidth_;
            const std::size_t cursorLine = model_->ScrollbackSize() + static_cast<std::size_t>(model_->CursorRow());
            const int cursorY = static_cast<int>(static_cast<long long>(cursorLine) -
                                                 static_cast<long long>(start)) * cellHeight_;
            RECT cursor{cursorX, cursorY + cellHeight_ - 2, cursorX + cellWidth_, cursorY + cellHeight_};
            HBRUSH cursorBrush = CreateSolidBrush(RGB(214, 223, 235)); FillRect(dc, &cursor, cursorBrush); DeleteObject(cursorBrush);
        }
        if (indicatorVisible_ && model_->TotalLineCount() > static_cast<std::size_t>(viewportRows_)) {
            const int trackHeight = std::max(1, static_cast<int>(client.bottom) - Ui::Scale(12, GetDpiForWindow(hwnd_)));
            const int minimum = Ui::Scale(18, GetDpiForWindow(hwnd_));
            const int totalLines = static_cast<int>(std::min<std::size_t>(model_->TotalLineCount(), INT_MAX));
            const int thumbHeight = std::max(minimum, MulDiv(trackHeight, viewportRows_, totalLines));
            const int maximumOffset = std::max(1, totalLines - viewportRows_);
            const int thumbTop = Ui::Scale(6, GetDpiForWindow(hwnd_)) +
                MulDiv(trackHeight - thumbHeight, maximumOffset - std::min(scrollOffset_, maximumOffset), maximumOffset);
            RECT thumb{client.right - Ui::Scale(5, GetDpiForWindow(hwnd_)), thumbTop,
                       client.right - Ui::Scale(2, GetDpiForWindow(hwnd_)), thumbTop + thumbHeight};
            Ui::DrawRoundedRect(dc, thumb, RGB(100, 116, 139), RGB(100, 116, 139), Ui::Scale(2, GetDpiForWindow(hwnd_)));
        }
    }
    if (oldFont != nullptr) SelectObject(dc, oldFont);
    if (buffer != nullptr) EndBufferedPaint(buffer, TRUE);
    EndPaint(hwnd_, &paint);
}

LRESULT TerminalView::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: Paint(); return 0;
    case WM_SIZE: UpdateMetrics(); InvalidateRect(hwnd_, nullptr, FALSE); return 0;
    case WM_GETDLGCODE: return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTARROWS;
    case WM_SETFOCUS: case WM_KILLFOCUS: InvalidateRect(hwnd_, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN: {
        SetFocus(hwnd_);
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        BeginSelection(point);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (selecting_) { POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; UpdateSelection(point); }
        return 0;
    case WM_LBUTTONUP:
        if (selecting_) { selecting_ = false; ReleaseCapture(); }
        return 0;
    case WM_RBUTTONUP:
        if (hasSelection_) CopySelection();
        return 0;
    case WM_MOUSEWHEEL:
        if (model_ != nullptr) {
            wheelRemainder_ += GET_WHEEL_DELTA_WPARAM(wParam);
            const int steps = wheelRemainder_ / WHEEL_DELTA;
            wheelRemainder_ %= WHEEL_DELTA;
            const int maximum = std::max(0, static_cast<int>(model_->TotalLineCount()) - viewportRows_);
            scrollOffset_ = std::clamp(scrollOffset_ + steps * 3, 0, maximum);
            indicatorVisible_ = true; SetTimer(hwnd_, IndicatorTimer, IndicatorDurationMs, nullptr);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_TIMER:
        if (wParam == IndicatorTimer) { KillTimer(hwnd_, IndicatorTimer); indicatorVisible_ = false; InvalidateRect(hwnd_, nullptr, FALSE); return 0; }
        break;
    case WM_KEYDOWN: {
        if (model_ == nullptr) return 0;
        const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (control && wParam == 'C' && (shift || hasSelection_)) { CopySelection(); return 0; }
        if (control && shift && wParam == 'V') { PasteClipboard(); return 0; }
        const std::string encoded = EncodeTerminalKey(static_cast<UINT>(wParam), control, alt, shift,
                                                       model_->ApplicationCursorKeys());
        if (!encoded.empty()) { Send(encoded); return 0; }
        break;
    }
    case WM_CHAR:
        if (wParam >= L' ' && (GetKeyState(VK_CONTROL) & 0x8000) == 0) {
            const wchar_t character = static_cast<wchar_t>(wParam);
            if (character >= 0xd800 && character <= 0xdbff) {
                pendingHighSurrogate_ = character;
            } else {
                std::wstring value;
                if (pendingHighSurrogate_ != 0) { value.push_back(pendingHighSurrogate_); pendingHighSurrogate_ = 0; }
                value.push_back(character);
                Send(WideToUtf8(value));
            }
            return 0;
        }
        return 0;
    case WM_SYSCHAR:
        if (wParam >= L' ') {
            Send(std::string(1, '\x1b') + WideToUtf8(std::wstring(1, static_cast<wchar_t>(wParam))));
            return 0;
        }
        break;
    default: break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

LRESULT CALLBACK TerminalView::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<TerminalView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = reinterpret_cast<TerminalView*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self != nullptr ? self->HandleMessage(message, wParam, lParam)
                           : DefWindowProcW(hwnd, message, wParam, lParam);
}
