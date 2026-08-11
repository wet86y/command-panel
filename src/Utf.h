#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

std::wstring Utf8ToWide(std::string_view value);
std::string WideToUtf8(std::wstring_view value);
std::string Base64Encode(std::string_view value);
std::string Base64Decode(std::string_view value);
std::wstring Win32ErrorMessage(unsigned long error);

class Utf8StreamDecoder
{
public:
    std::wstring Feed(std::span<const unsigned char> bytes);
    void Reset();

private:
    std::string carry_;
};
