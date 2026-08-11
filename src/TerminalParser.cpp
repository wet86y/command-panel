#include "TerminalParser.h"

std::wstring TerminalParser::Feed(std::span<const unsigned char> bytes)
{
    return decoder_.Feed(bytes);
}

void TerminalParser::Reset()
{
    decoder_.Reset();
}
