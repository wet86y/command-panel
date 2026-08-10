#pragma once

#include "Utf.h"

#include <span>
#include <string>

class TerminalParser
{
public:
    std::wstring Feed(std::span<const unsigned char> bytes);
    void Reset();

private:
    enum class State { Normal, Escape, Csi, Osc, OscEscape };
    State state_ = State::Normal;
    Utf8StreamDecoder decoder_;
};
