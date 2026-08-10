#include "TerminalParser.h"

std::wstring TerminalParser::Feed(std::span<const unsigned char> bytes)
{
    const std::wstring decoded = decoder_.Feed(bytes);
    std::wstring output;
    output.reserve(decoded.size());

    for (wchar_t ch : decoded) {
        switch (state_) {
        case State::Normal:
            if (ch == L'\x1b') state_ = State::Escape;
            else if (ch == L'\a' || ch == L'\0') {}
            else if (ch == L'\b') {
                if (!output.empty() && output.back() != L'\n' && output.back() != L'\r') output.pop_back();
            } else {
                output.push_back(ch);
            }
            break;
        case State::Escape:
            if (ch == L'[') state_ = State::Csi;
            else if (ch == L']') state_ = State::Osc;
            else state_ = State::Normal;
            break;
        case State::Csi:
            if (ch >= L'@' && ch <= L'~') state_ = State::Normal;
            break;
        case State::Osc:
            if (ch == L'\a') state_ = State::Normal;
            else if (ch == L'\x1b') state_ = State::OscEscape;
            break;
        case State::OscEscape:
            state_ = ch == L'\\' ? State::Normal : State::Osc;
            break;
        }
    }
    return output;
}

void TerminalParser::Reset()
{
    state_ = State::Normal;
    decoder_.Reset();
}
