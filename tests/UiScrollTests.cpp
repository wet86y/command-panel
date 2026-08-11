#include "ButtonLayout.h"
#include "UiScroll.h"
#include "TerminalModel.h"
#include "TerminalParser.h"
#include "ConfigManager.h"

#include <richedit.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAILED: " << message << '\n';
}

void TestThumbCalculation()
{
    UiScroll::Metrics metrics{0, 999, 100, 0};
    UiScroll::Thumb thumb = UiScroll::CalculateThumb(metrics, 200, 18);
    Check(thumb.visible, "overflow range should have a thumb");
    Check(thumb.top == 0, "top position should map to top of track");
    Check(thumb.height == 20, "page ratio should determine thumb height");

    metrics.position = 900;
    thumb = UiScroll::CalculateThumb(metrics, 200, 18);
    Check(thumb.top + thumb.height == 200, "maximum position should map to bottom of track");

    metrics.position = 450;
    thumb = UiScroll::CalculateThumb(metrics, 200, 18);
    Check(thumb.top == 90, "middle position should map to middle of track");

    metrics = UiScroll::Metrics{0, 99, 100, 0};
    Check(!UiScroll::CalculateThumb(metrics, 200, 18).visible,
          "non-overflow range should not have a thumb");
}

void TestWheelAccumulation()
{
    int remainder = 0;
    for (int index = 0; index < 3; ++index) {
        const UiScroll::WheelAction action = UiScroll::AccumulateWheel(30, 3, remainder);
        Check(action.lines == 0 && action.pages == 0, "partial wheel deltas must accumulate");
    }
    const UiScroll::WheelAction lines = UiScroll::AccumulateWheel(30, 3, remainder);
    Check(lines.lines == 3 && remainder == 0, "one detent should honor the system line count");

    const UiScroll::WheelAction pages = UiScroll::AccumulateWheel(-120, WHEEL_PAGESCROLL, remainder);
    Check(pages.pages == -1 && pages.lines == 0, "page-scroll setting should produce page actions");
}

void TestButtonLayout()
{
    const ButtonLayoutResult wide = CalculateButtonLayout(1000, 120, 96, 9, 0);
    Check(wide.columns == 4, "wide button panel should use four columns");
    Check(wide.cards.size() == 9, "layout should return every card");
    Check(wide.contentHeight > 120 && wide.maximumScroll == wide.contentHeight - 120,
          "overflow height should produce an exact scroll range");

    const ButtonLayoutResult scrolled = CalculateButtonLayout(1000, 120, 96, 9, wide.maximumScroll);
    Check(scrolled.cards.front().top < wide.cards.front().top,
          "scroll position should translate every card");
    Check(scrolled.cards.back().bottom <= 120,
          "maximum scroll should bring the final row into view");

    const ButtonLayoutResult narrow = CalculateButtonLayout(360, 300, 144, 5, 0);
    Check(narrow.columns >= 1 && narrow.columns < 4, "narrow high-DPI panel should reduce columns");
}

std::wstring LongText()
{
    std::wstring text;
    for (int index = 0; index < 400; ++index)
        text += L"line " + std::to_wstring(index) + L" 012345678901234567890123456789\r\n";
    return text;
}

void TestRealEditControls()
{
    LoadLibraryW(L"Msftedit.dll");
    HWND parent = CreateWindowExW(0, L"STATIC", nullptr, WS_POPUP | WS_VISIBLE,
                                  -3000, -3000, 420, 320, nullptr, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
    HWND rich = CreateWindowExW(0, MSFTEDIT_CLASS, nullptr,
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                    ES_AUTOVSCROLL | ES_READONLY,
                                0, 0, 300, 120, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    HWND edit = CreateWindowExW(0, L"EDIT", nullptr,
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                    ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                0, 130, 300, 120, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(parent != nullptr && rich != nullptr && edit != nullptr, "real edit controls should be created");
    if (parent == nullptr || rich == nullptr || edit == nullptr) {
        if (parent != nullptr) DestroyWindow(parent);
        return;
    }

    const std::wstring text = LongText();
    SetWindowTextW(rich, text.c_str());
    SetWindowTextW(edit, text.c_str());
    UpdateWindow(parent);
    SendMessageW(rich, EM_LINESCROLL, 0, 100);
    SendMessageW(edit, EM_LINESCROLL, 0, 100);

    const UiScroll::Metrics richBefore = UiScroll::ReadMetrics(rich);
    const UiScroll::Metrics editBefore = UiScroll::ReadMetrics(edit);
    Check(UiScroll::IsScrollable(richBefore), "RichEdit should expose its scroll range");
    Check(UiScroll::IsScrollable(editBefore), "multiline Edit should expose its scroll range");

    SendMessageW(edit, EM_SETSEL, 10, 20);
    DWORD selectionStart = 0;
    DWORD selectionEnd = 0;
    SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&selectionStart),
                 reinterpret_cast<LPARAM>(&selectionEnd));
    int richRemainder = 0;
    int editRemainder = 0;
    Check(UiScroll::ScrollTextControl(rich, -120, 3, richRemainder),
          "explicit RichEdit wheel handling should scroll");
    Check(UiScroll::ScrollTextControl(edit, -120, 3, editRemainder),
          "explicit multiline Edit wheel handling should scroll");
    const UiScroll::Metrics richAfter = UiScroll::ReadMetrics(rich);
    const UiScroll::Metrics editAfter = UiScroll::ReadMetrics(edit);
    if (richAfter.position <= richBefore.position) {
        std::cerr << "RichEdit metrics before=" << richBefore.minimum << '/' << richBefore.maximum
                  << " page=" << richBefore.page << " pos=" << richBefore.position
                  << " after=" << richAfter.minimum << '/' << richAfter.maximum
                  << " page=" << richAfter.page << " pos=" << richAfter.position << '\n';
    }
    if (editAfter.position <= editBefore.position) {
        std::cerr << "Edit metrics before=" << editBefore.minimum << '/' << editBefore.maximum
                  << " page=" << editBefore.page << " pos=" << editBefore.position
                  << " after=" << editAfter.minimum << '/' << editAfter.maximum
                  << " page=" << editAfter.page << " pos=" << editAfter.position << '\n';
    }
    Check(richAfter.position > richBefore.position, "RichEdit position should advance after wheel scroll");
    Check(editAfter.position > editBefore.position, "Edit position should advance after wheel scroll");

    DWORD selectionStartAfter = 0;
    DWORD selectionEndAfter = 0;
    SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&selectionStartAfter),
                 reinterpret_cast<LPARAM>(&selectionEndAfter));
    Check(selectionStart == selectionStartAfter && selectionEnd == selectionEndAfter,
          "wheel scrolling must not move the input selection");
    DestroyWindow(parent);
}

void TestTerminalModel()
{
    TerminalModel model(8, 3);
    model.Feed(L"abc\r\ndef");
    Check(model.CursorRow() == 1 && model.CursorColumn() == 3,
          "terminal should track CRLF cursor movement");
    Check(model.LineAt(0)[0].character == L'a' && model.LineAt(1)[2].character == L'f',
          "terminal should retain screen cells");

    model.Feed(L"\x1b[2;2H\x1b[31mX\x1b[0m");
    Check(model.LineAt(model.ScrollbackSize() + 1)[1].character == L'X',
          "CUP should place text at the requested cell");
    Check(model.LineAt(model.ScrollbackSize() + 1)[1].attributes.foreground == RGB(205,49,49),
          "SGR should persist color on the written cell");

    model.Feed(L"\x1b[?1h\x1b[?2004h");
    Check(model.ApplicationCursorKeys(), "DECCKM should enable application cursor keys");
    Check(model.BracketedPaste(), "private mode 2004 should enable bracketed paste");
    model.Feed(L"\x1b[6n");
    Check(!model.TakeResponse().empty(), "cursor position queries should receive a terminal response");

    model.Feed(L"\x1b[?1049hALT");
    Check(model.AlternateScreen(), "private mode 1049 should enter alternate screen");
    Check(model.LineAt(model.ScrollbackSize())[0].character == L'A',
          "alternate screen should receive TUI output");
    model.Feed(L"\x1b[?1049l");
    Check(!model.AlternateScreen(), "private mode 1049 reset should restore normal screen");
    Check(model.LineAt(model.ScrollbackSize())[0].character == L'a',
          "normal screen should be restored after TUI exit");

    model.Clear();
    model.Feed(L"中A");
    Check(model.CursorColumn() == 3, "wide CJK characters should occupy two cells");
    Check(model.LineAt(0)[1].wideContinuation, "wide CJK trailing cell should be marked");
}

void TestTerminalInputEncoding()
{
    Check(EncodeTerminalKey(VK_UP, false, false, false, false) == "\x1b[A",
          "normal cursor up should use CSI");
    Check(EncodeTerminalKey(VK_UP, false, false, false, true) == "\x1bOA",
          "application cursor up should use SS3");
    Check(EncodeTerminalKey('C', true, false, false, false) == std::string(1, '\x03'),
          "Ctrl+C should encode ETX");
    Check(EncodeTerminalKey(VK_F5, false, false, false, false) == "\x1b[15~",
          "F5 should use the standard VT sequence");
    Check(EncodeTerminalPaste(L"hello", true) == "\x1b[200~hello\x1b[201~",
          "bracketed paste should wrap clipboard content");
}

void TestTerminalUtf8Streaming()
{
    TerminalParser parser;
    const unsigned char first[] = {0xe4, 0xb8};
    const unsigned char second[] = {0xad};
    Check(parser.Feed(first).empty(), "partial UTF-8 must remain buffered");
    Check(parser.Feed(second) == L"中", "split UTF-8 should decode after the final byte");
    const unsigned char escape[] = {0x1b, '[', '3', '1', 'm'};
    Check(parser.Feed(escape) == L"\x1b[31m", "decoder must preserve VT sequences for the screen model");
}

void TestConfigV3Migration()
{
    TerminalKind invalidKind = TerminalKind::PowerShell;
    Check(!ParseTerminalKind("cmd", invalidKind), "unknown terminal names must be rejected");
    const std::filesystem::path directory = std::filesystem::temp_directory_path() /
        (L"CommandPanelTests-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const std::filesystem::path path = directory / L"config.json";
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << "{\"version\":2,\"ui\":{},\"tabs\":[{\"id\":\"t\",\"name\":\"T\",\"buttons\":["
                  "{\"id\":\"b\",\"name\":\"B\",\"command\":\"echo ok\",\"confirm\":false,\"enabled\":true}]}]}";
    }
    ConfigManager config(path);
    Check(config.Load(), "version 2 config should remain readable");
    Check(config.Tabs().size() == 1 && config.Tabs()[0].buttons[0].terminal == TerminalKind::PowerShell,
          "legacy buttons should default to PowerShell without command guessing");
    config.Tabs()[0].buttons[0].terminal = TerminalKind::Wsl;
    config.Ui().activeTerminal = TerminalKind::Wsl;
    Check(config.Save(), "migrated config should save as version 3");
    ConfigManager roundTrip(path);
    Check(roundTrip.Load(), "saved version 3 config should load");
    Check(roundTrip.Ui().activeTerminal == TerminalKind::Wsl &&
          roundTrip.Tabs()[0].buttons[0].terminal == TerminalKind::Wsl,
          "version 3 terminal fields should round trip");
    std::filesystem::remove(path, error);
    std::filesystem::remove(directory, error);
}

} // namespace

int wmain()
{
    TestThumbCalculation();
    TestWheelAccumulation();
    TestButtonLayout();
    TestRealEditControls();
    TestTerminalModel();
    TestTerminalInputEncoding();
    TestTerminalUtf8Streaming();
    TestConfigV3Migration();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All UI scroll tests passed\n";
    return EXIT_SUCCESS;
}
