#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct TerminalAttributes
{
    COLORREF foreground = RGB(226, 232, 240);
    COLORREF background = RGB(10, 16, 21);
    bool bold = false;
    bool underline = false;
    bool inverse = false;

    bool operator==(const TerminalAttributes&) const = default;
};

struct TerminalCell
{
    wchar_t character = L' ';
    std::wstring combining;
    TerminalAttributes attributes;
    bool wideContinuation = false;
};

class TerminalModel
{
public:
    TerminalModel(int columns = 120, int rows = 30);

    void Feed(std::wstring_view text);
    void Resize(int columns, int rows);
    void Clear();
    std::string TakeResponse();

    int Columns() const { return columns_; }
    int Rows() const { return rows_; }
    int CursorColumn() const { return cursorColumn_; }
    int CursorRow() const { return cursorRow_; }
    bool CursorVisible() const { return cursorVisible_; }
    bool ApplicationCursorKeys() const { return applicationCursorKeys_; }
    bool BracketedPaste() const { return bracketedPaste_; }
    bool AlternateScreen() const { return alternateScreen_; }
    std::size_t ScrollbackSize() const { return scrollback_.size(); }
    std::size_t TotalLineCount() const { return scrollback_.size() + screen_.size(); }
    const std::vector<TerminalCell>& LineAt(std::size_t index) const;

private:
    enum class ParseState { Normal, Escape, Csi, Osc, OscEscape };

    using Line = std::vector<TerminalCell>;
    static constexpr std::size_t MaxScrollback = 10000;

    void Put(wchar_t character);
    void LineFeed();
    void CarriageReturn();
    void Backspace();
    void Tab();
    void ScrollUp(int count = 1);
    void ScrollDown(int count = 1);
    void ExecuteCsi(wchar_t finalCharacter);
    void ExecuteSgr();
    void SetPrivateMode(bool enabled);
    void UseAlternateScreen(bool enabled);
    void EraseDisplay(int mode);
    void EraseLine(int mode);
    void InsertCharacters(int count);
    void DeleteCharacters(int count);
    void InsertLines(int count);
    void DeleteLines(int count);
    int Parameter(std::size_t index, int fallback) const;
    Line BlankLine() const;
    void ClampCursor();
    static int CharacterWidth(wchar_t character);
    static COLORREF IndexedColor(int index, bool bright);

    int columns_ = 120;
    int rows_ = 30;
    int cursorColumn_ = 0;
    int cursorRow_ = 0;
    int savedColumn_ = 0;
    int savedRow_ = 0;
    int scrollTop_ = 0;
    int scrollBottom_ = 29;
    bool cursorVisible_ = true;
    bool applicationCursorKeys_ = false;
    bool bracketedPaste_ = false;
    bool alternateScreen_ = false;
    bool wrapPending_ = false;
    TerminalAttributes attributes_;
    ParseState parseState_ = ParseState::Normal;
    std::wstring csiText_;
    bool csiPrivate_ = false;
    std::vector<int> parameters_;
    std::vector<Line> screen_;
    std::vector<Line> scrollback_;
    std::vector<Line> normalScreen_;
    int normalCursorColumn_ = 0;
    int normalCursorRow_ = 0;
    std::string pendingResponse_;
};

std::string EncodeTerminalKey(UINT virtualKey, bool control, bool alt, bool shift,
                              bool applicationCursorKeys);
std::string EncodeTerminalPaste(std::wstring_view text, bool bracketedPaste);
