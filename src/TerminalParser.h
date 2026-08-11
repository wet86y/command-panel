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
    Utf8StreamDecoder decoder_;
};
