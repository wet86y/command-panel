#include "TerminalModel.h"

#include "Utf.h"

#include <algorithm>
#include <cwctype>

namespace {
constexpr COLORREF DefaultForeground = RGB(226, 232, 240);
constexpr COLORREF DefaultBackground = RGB(10, 16, 21);

bool IsCombining(wchar_t value)
{
    WORD type = 0;
    return GetStringTypeW(CT_CTYPE3, &value, 1, &type) &&
           (type & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK)) != 0;
}

std::string ModifierSuffix(bool control, bool alt, bool shift)
{
    const int modifier = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (control ? 4 : 0);
    return modifier == 1 ? std::string{} : ";" + std::to_string(modifier);
}
}

TerminalModel::TerminalModel(int columns, int rows)
{
    Resize(columns, rows);
}

TerminalModel::Line TerminalModel::BlankLine() const
{
    return Line(static_cast<std::size_t>(columns_), TerminalCell{L' ', {}, attributes_, false});
}

void TerminalModel::Resize(int columns, int rows)
{
    columns_ = std::clamp(columns, 1, 500);
    rows_ = std::clamp(rows, 1, 300);
    for (auto& line : screen_) line.resize(static_cast<std::size_t>(columns_));
    while (static_cast<int>(screen_.size()) < rows_) screen_.push_back(BlankLine());
    while (static_cast<int>(screen_.size()) > rows_) {
        if (!alternateScreen_) {
            scrollback_.push_back(std::move(screen_.front()));
            if (scrollback_.size() > MaxScrollback) scrollback_.erase(scrollback_.begin());
        }
        screen_.erase(screen_.begin());
    }
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
    ClampCursor();
}

void TerminalModel::Clear()
{
    attributes_ = {};
    screen_.assign(static_cast<std::size_t>(rows_), BlankLine());
    scrollback_.clear();
    cursorColumn_ = cursorRow_ = 0;
    wrapPending_ = false;
}

std::string TerminalModel::TakeResponse()
{
    std::string response = std::move(pendingResponse_);
    pendingResponse_.clear();
    return response;
}

const std::vector<TerminalCell>& TerminalModel::LineAt(std::size_t index) const
{
    if (index < scrollback_.size()) return scrollback_[index];
    return screen_[std::min(index - scrollback_.size(), screen_.size() - 1)];
}

void TerminalModel::ClampCursor()
{
    cursorColumn_ = std::clamp(cursorColumn_, 0, columns_ - 1);
    cursorRow_ = std::clamp(cursorRow_, 0, rows_ - 1);
}

int TerminalModel::CharacterWidth(wchar_t character)
{
    if (IsCombining(character)) return 0;
    const unsigned value = static_cast<unsigned>(character);
    if ((value >= 0x1100 && value <= 0x115f) || (value >= 0x2329 && value <= 0x232a) ||
        (value >= 0x2e80 && value <= 0xa4cf) || (value >= 0xac00 && value <= 0xd7a3) ||
        (value >= 0xf900 && value <= 0xfaff) || (value >= 0xfe10 && value <= 0xfe6f) ||
        (value >= 0xff00 && value <= 0xff60) || (value >= 0xffe0 && value <= 0xffe6)) return 2;
    return 1;
}

void TerminalModel::Put(wchar_t character)
{
    const int width = CharacterWidth(character);
    if (width == 0) {
        const int column = std::max(0, cursorColumn_ - 1);
        screen_[static_cast<std::size_t>(cursorRow_)][static_cast<std::size_t>(column)].combining.push_back(character);
        return;
    }
    if (wrapPending_ || (width == 2 && cursorColumn_ == columns_ - 1)) {
        CarriageReturn();
        LineFeed();
    }
    auto& line = screen_[static_cast<std::size_t>(cursorRow_)];
    line[static_cast<std::size_t>(cursorColumn_)] = TerminalCell{character, {}, attributes_, false};
    if (width == 2 && cursorColumn_ + 1 < columns_)
        line[static_cast<std::size_t>(cursorColumn_ + 1)] = TerminalCell{L' ', {}, attributes_, true};
    cursorColumn_ += width;
    if (cursorColumn_ >= columns_) {
        cursorColumn_ = columns_ - 1;
        wrapPending_ = true;
    }
}

void TerminalModel::CarriageReturn() { cursorColumn_ = 0; wrapPending_ = false; }
void TerminalModel::Backspace() { cursorColumn_ = std::max(0, cursorColumn_ - 1); wrapPending_ = false; }
void TerminalModel::Tab() { cursorColumn_ = std::min(columns_ - 1, ((cursorColumn_ / 8) + 1) * 8); }

void TerminalModel::LineFeed()
{
    wrapPending_ = false;
    if (cursorRow_ == scrollBottom_) ScrollUp();
    else cursorRow_ = std::min(rows_ - 1, cursorRow_ + 1);
}

void TerminalModel::ScrollUp(int count)
{
    count = std::clamp(count, 1, scrollBottom_ - scrollTop_ + 1);
    for (int index = 0; index < count; ++index) {
        if (scrollTop_ == 0 && scrollBottom_ == rows_ - 1 && !alternateScreen_) {
            scrollback_.push_back(screen_.front());
            if (scrollback_.size() > MaxScrollback) scrollback_.erase(scrollback_.begin());
        }
        screen_.erase(screen_.begin() + scrollTop_);
        screen_.insert(screen_.begin() + scrollBottom_, BlankLine());
    }
}

void TerminalModel::ScrollDown(int count)
{
    count = std::clamp(count, 1, scrollBottom_ - scrollTop_ + 1);
    for (int index = 0; index < count; ++index) {
        screen_.erase(screen_.begin() + scrollBottom_);
        screen_.insert(screen_.begin() + scrollTop_, BlankLine());
    }
}

int TerminalModel::Parameter(std::size_t index, int fallback) const
{
    if (index >= parameters_.size() || parameters_[index] == 0) return fallback;
    return parameters_[index];
}

COLORREF TerminalModel::IndexedColor(int index, bool bright)
{
    static constexpr COLORREF normal[8] = {
        RGB(10,16,21), RGB(205,49,49), RGB(13,188,121), RGB(229,229,16),
        RGB(36,114,200), RGB(188,63,188), RGB(17,168,205), RGB(229,229,229)};
    static constexpr COLORREF intense[8] = {
        RGB(102,102,102), RGB(241,76,76), RGB(35,209,139), RGB(245,245,67),
        RGB(59,142,234), RGB(214,112,214), RGB(41,184,219), RGB(255,255,255)};
    return bright ? intense[index & 7] : normal[index & 7];
}

void TerminalModel::ExecuteSgr()
{
    if (parameters_.empty()) parameters_.push_back(0);
    for (std::size_t index = 0; index < parameters_.size(); ++index) {
        const int value = parameters_[index];
        if (value == 0) attributes_ = {};
        else if (value == 1) attributes_.bold = true;
        else if (value == 4) attributes_.underline = true;
        else if (value == 7) attributes_.inverse = true;
        else if (value == 22) attributes_.bold = false;
        else if (value == 24) attributes_.underline = false;
        else if (value == 27) attributes_.inverse = false;
        else if (value >= 30 && value <= 37) attributes_.foreground = IndexedColor(value - 30, false);
        else if (value >= 90 && value <= 97) attributes_.foreground = IndexedColor(value - 90, true);
        else if (value >= 40 && value <= 47) attributes_.background = IndexedColor(value - 40, false);
        else if (value >= 100 && value <= 107) attributes_.background = IndexedColor(value - 100, true);
        else if (value == 39) attributes_.foreground = DefaultForeground;
        else if (value == 49) attributes_.background = DefaultBackground;
        else if ((value == 38 || value == 48) && index + 2 < parameters_.size() && parameters_[index + 1] == 5) {
            const int color = std::clamp(parameters_[index + 2], 0, 255);
            COLORREF rgb{};
            if (color < 16) rgb = IndexedColor(color & 7, color >= 8);
            else if (color < 232) {
                const int cube = color - 16;
                const auto component = [](int value) { return value == 0 ? 0 : 55 + value * 40; };
                rgb = RGB(component(cube / 36), component((cube / 6) % 6), component(cube % 6));
            } else { const int gray = 8 + (color - 232) * 10; rgb = RGB(gray, gray, gray); }
            (value == 38 ? attributes_.foreground : attributes_.background) = rgb;
            index += 2;
        } else if ((value == 38 || value == 48) && index + 4 < parameters_.size() && parameters_[index + 1] == 2) {
            const COLORREF rgb = RGB(std::clamp(parameters_[index + 2], 0, 255),
                                     std::clamp(parameters_[index + 3], 0, 255),
                                     std::clamp(parameters_[index + 4], 0, 255));
            (value == 38 ? attributes_.foreground : attributes_.background) = rgb;
            index += 4;
        }
    }
}

void TerminalModel::EraseLine(int mode)
{
    auto& line = screen_[static_cast<std::size_t>(cursorRow_)];
    int begin = mode == 1 ? 0 : cursorColumn_;
    int end = mode == 0 ? cursorColumn_ + 1 : columns_;
    if (mode == 2) { begin = 0; end = columns_; }
    std::fill(line.begin() + begin, line.begin() + end, TerminalCell{L' ', {}, attributes_, false});
}

void TerminalModel::EraseDisplay(int mode)
{
    if (mode == 2 || mode == 3) {
        for (auto& line : screen_) std::fill(line.begin(), line.end(), TerminalCell{L' ', {}, attributes_, false});
        if (mode == 3) scrollback_.clear();
        return;
    }
    EraseLine(mode == 0 ? 0 : 1);
    if (mode == 0) {
        for (int row = cursorRow_ + 1; row < rows_; ++row) screen_[static_cast<std::size_t>(row)] = BlankLine();
    } else {
        for (int row = 0; row < cursorRow_; ++row) screen_[static_cast<std::size_t>(row)] = BlankLine();
    }
}

void TerminalModel::InsertCharacters(int count)
{
    auto& line = screen_[static_cast<std::size_t>(cursorRow_)];
    count = std::clamp(count, 1, columns_ - cursorColumn_);
    std::move_backward(line.begin() + cursorColumn_, line.end() - count, line.end());
    std::fill(line.begin() + cursorColumn_, line.begin() + cursorColumn_ + count, TerminalCell{L' ', {}, attributes_, false});
}

void TerminalModel::DeleteCharacters(int count)
{
    auto& line = screen_[static_cast<std::size_t>(cursorRow_)];
    count = std::clamp(count, 1, columns_ - cursorColumn_);
    std::move(line.begin() + cursorColumn_ + count, line.end(), line.begin() + cursorColumn_);
    std::fill(line.end() - count, line.end(), TerminalCell{L' ', {}, attributes_, false});
}

void TerminalModel::InsertLines(int count)
{
    if (cursorRow_ < scrollTop_ || cursorRow_ > scrollBottom_) return;
    count = std::clamp(count, 1, scrollBottom_ - cursorRow_ + 1);
    for (int index = 0; index < count; ++index) {
        screen_.erase(screen_.begin() + scrollBottom_);
        screen_.insert(screen_.begin() + cursorRow_, BlankLine());
    }
}

void TerminalModel::DeleteLines(int count)
{
    if (cursorRow_ < scrollTop_ || cursorRow_ > scrollBottom_) return;
    count = std::clamp(count, 1, scrollBottom_ - cursorRow_ + 1);
    for (int index = 0; index < count; ++index) {
        screen_.erase(screen_.begin() + cursorRow_);
        screen_.insert(screen_.begin() + scrollBottom_, BlankLine());
    }
}

void TerminalModel::UseAlternateScreen(bool enabled)
{
    if (enabled == alternateScreen_) return;
    if (enabled) {
        normalScreen_ = screen_;
        normalCursorColumn_ = cursorColumn_;
        normalCursorRow_ = cursorRow_;
        screen_.assign(static_cast<std::size_t>(rows_), BlankLine());
        cursorColumn_ = cursorRow_ = 0;
    } else {
        if (!normalScreen_.empty()) screen_ = std::move(normalScreen_);
        for (auto& line : screen_) line.resize(static_cast<std::size_t>(columns_));
        while (static_cast<int>(screen_.size()) < rows_) screen_.push_back(BlankLine());
        while (static_cast<int>(screen_.size()) > rows_) screen_.erase(screen_.begin());
        cursorColumn_ = normalCursorColumn_;
        cursorRow_ = normalCursorRow_;
    }
    alternateScreen_ = enabled;
    ClampCursor();
}

void TerminalModel::SetPrivateMode(bool enabled)
{
    for (int value : parameters_) {
        if (value == 1) applicationCursorKeys_ = enabled;
        else if (value == 25) cursorVisible_ = enabled;
        else if (value == 1047 || value == 1049) UseAlternateScreen(enabled);
        else if (value == 2004) bracketedPaste_ = enabled;
    }
}

void TerminalModel::ExecuteCsi(wchar_t finalCharacter)
{
    parameters_.clear();
    int current = 0;
    bool hasDigits = false;
    csiPrivate_ = !csiText_.empty() && csiText_.front() == L'?';
    const std::size_t start = csiPrivate_ ? 1 : 0;
    for (std::size_t index = start; index <= csiText_.size(); ++index) {
        if (index < csiText_.size() && iswdigit(csiText_[index])) {
            current = current * 10 + (csiText_[index] - L'0'); hasDigits = true;
        } else if (index == csiText_.size() || csiText_[index] == L';') {
            parameters_.push_back(hasDigits ? current : 0); current = 0; hasDigits = false;
        }
    }
    const int count = Parameter(0, 1);
    switch (finalCharacter) {
    case L'A': cursorRow_ -= count; break;
    case L'B': cursorRow_ += count; break;
    case L'C': cursorColumn_ += count; break;
    case L'D': cursorColumn_ -= count; break;
    case L'E': cursorRow_ += count; cursorColumn_ = 0; break;
    case L'F': cursorRow_ -= count; cursorColumn_ = 0; break;
    case L'G': cursorColumn_ = Parameter(0, 1) - 1; break;
    case L'H': case L'f': cursorRow_ = Parameter(0, 1) - 1; cursorColumn_ = Parameter(1, 1) - 1; break;
    case L'J': EraseDisplay(parameters_.empty() ? 0 : parameters_[0]); break;
    case L'K': EraseLine(parameters_.empty() ? 0 : parameters_[0]); break;
    case L'@': InsertCharacters(count); break;
    case L'P': DeleteCharacters(count); break;
    case L'L': InsertLines(count); break;
    case L'M': DeleteLines(count); break;
    case L'S': ScrollUp(count); break;
    case L'T': ScrollDown(count); break;
    case L'm': ExecuteSgr(); break;
    case L'n':
        if (Parameter(0, 0) == 5) pendingResponse_ += "\x1b[0n";
        else if (Parameter(0, 0) == 6)
            pendingResponse_ += "\x1b[" + std::to_string(cursorRow_ + 1) + ";" +
                                std::to_string(cursorColumn_ + 1) + "R";
        break;
    case L'c': pendingResponse_ += "\x1b[?1;2c"; break;
    case L'r':
        scrollTop_ = std::clamp(Parameter(0, 1) - 1, 0, rows_ - 1);
        scrollBottom_ = std::clamp(Parameter(1, rows_) - 1, scrollTop_, rows_ - 1);
        cursorColumn_ = cursorRow_ = 0;
        break;
    case L's': savedColumn_ = cursorColumn_; savedRow_ = cursorRow_; break;
    case L'u': cursorColumn_ = savedColumn_; cursorRow_ = savedRow_; break;
    case L'h': if (csiPrivate_) SetPrivateMode(true); break;
    case L'l': if (csiPrivate_) SetPrivateMode(false); break;
    default: break;
    }
    wrapPending_ = false;
    ClampCursor();
}

void TerminalModel::Feed(std::wstring_view text)
{
    for (wchar_t character : text) {
        switch (parseState_) {
        case ParseState::Normal:
            if (character == L'\x1b') parseState_ = ParseState::Escape;
            else if (character == L'\r') CarriageReturn();
            else if (character == L'\n') LineFeed();
            else if (character == L'\b') Backspace();
            else if (character == L'\t') Tab();
            else if (character >= L' ') Put(character);
            break;
        case ParseState::Escape:
            if (character == L'[') { csiText_.clear(); parseState_ = ParseState::Csi; }
            else if (character == L']') parseState_ = ParseState::Osc;
            else if (character == L'7') { savedColumn_ = cursorColumn_; savedRow_ = cursorRow_; parseState_ = ParseState::Normal; }
            else if (character == L'8') { cursorColumn_ = savedColumn_; cursorRow_ = savedRow_; ClampCursor(); parseState_ = ParseState::Normal; }
            else if (character == L'D') { LineFeed(); parseState_ = ParseState::Normal; }
            else if (character == L'M') { if (cursorRow_ == scrollTop_) ScrollDown(); else --cursorRow_; parseState_ = ParseState::Normal; }
            else if (character == L'c') { Clear(); applicationCursorKeys_ = bracketedPaste_ = false; parseState_ = ParseState::Normal; }
            else parseState_ = ParseState::Normal;
            break;
        case ParseState::Csi:
            if (character >= L'@' && character <= L'~') { ExecuteCsi(character); parseState_ = ParseState::Normal; }
            else if (csiText_.size() < 128) csiText_.push_back(character);
            break;
        case ParseState::Osc:
            if (character == L'\a') parseState_ = ParseState::Normal;
            else if (character == L'\x1b') parseState_ = ParseState::OscEscape;
            break;
        case ParseState::OscEscape:
            parseState_ = character == L'\\' ? ParseState::Normal : ParseState::Osc;
            break;
        }
    }
}

std::string EncodeTerminalKey(UINT key, bool control, bool alt, bool shift, bool application)
{
    std::string sequence;
    const std::string modifier = ModifierSuffix(control, alt, shift);
    const auto cursor = [&](char finalCharacter) {
        if (modifier.empty()) return std::string(application ? "\x1bO" : "\x1b[") + finalCharacter;
        return std::string("\x1b[1") + modifier + finalCharacter;
    };
    const auto tilde = [&](int code) {
        return std::string("\x1b[") + std::to_string(code) + modifier + "~";
    };
    const auto function = [&](char finalCharacter) {
        if (modifier.empty()) return std::string("\x1bO") + finalCharacter;
        return std::string("\x1b[1") + modifier + finalCharacter;
    };
    switch (key) {
    case VK_UP: sequence = cursor('A'); break;
    case VK_DOWN: sequence = cursor('B'); break;
    case VK_RIGHT: sequence = cursor('C'); break;
    case VK_LEFT: sequence = cursor('D'); break;
    case VK_HOME: sequence = cursor('H'); break;
    case VK_END: sequence = cursor('F'); break;
    case VK_INSERT: sequence = tilde(2); break;
    case VK_DELETE: sequence = tilde(3); break;
    case VK_PRIOR: sequence = tilde(5); break;
    case VK_NEXT: sequence = tilde(6); break;
    case VK_F1: sequence = function('P'); break;
    case VK_F2: sequence = function('Q'); break;
    case VK_F3: sequence = function('R'); break;
    case VK_F4: sequence = function('S'); break;
    case VK_F5: sequence = tilde(15); break;
    case VK_F6: sequence = tilde(17); break;
    case VK_F7: sequence = tilde(18); break;
    case VK_F8: sequence = tilde(19); break;
    case VK_F9: sequence = tilde(20); break;
    case VK_F10: sequence = tilde(21); break;
    case VK_F11: sequence = tilde(23); break;
    case VK_F12: sequence = tilde(24); break;
    case VK_BACK: sequence.assign(1, '\x7f'); break;
    case VK_ESCAPE: sequence.assign(1, '\x1b'); break;
    case VK_RETURN: sequence.assign(1, '\r'); break;
    case VK_TAB: sequence = shift ? "\x1b[Z" : "\t"; break;
    default:
        if (control && key >= 'A' && key <= 'Z') sequence.assign(1, static_cast<char>(key - 'A' + 1));
        else if (control && key == VK_SPACE) sequence.assign(1, '\0');
        else if (control && key == VK_OEM_4) sequence.assign(1, '\x1b');
        else if (control && key == VK_OEM_5) sequence.assign(1, '\x1c');
        else if (control && key == VK_OEM_6) sequence.assign(1, '\x1d');
        break;
    }
    if (alt && !sequence.empty() && key >= 'A' && key <= 'Z') sequence.insert(sequence.begin(), '\x1b');
    return sequence;
}

std::string EncodeTerminalPaste(std::wstring_view text, bool bracketedPaste)
{
    std::string value = WideToUtf8(std::wstring(text));
    if (bracketedPaste) return "\x1b[200~" + value + "\x1b[201~";
    return value;
}
