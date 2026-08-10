#include "Utf.h"

#include <windows.h>
#include <wincrypt.h>

#include <algorithm>

std::wstring Utf8ToWide(std::string_view value)
{
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        const int fallback = MultiByteToWideChar(CP_UTF8, 0,
                                                 value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (fallback <= 0) {
            return {};
        }
        std::wstring result(static_cast<size_t>(fallback), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                            result.data(), fallback);
        return result;
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), length);
    return result;
}

std::string WideToUtf8(std::wstring_view value)
{
    if (value.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                           value.data(), static_cast<int>(value.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

std::string Base64Encode(std::string_view value)
{
    if (value.empty()) {
        return {};
    }
    DWORD outputLength = 0;
    if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(value.data()),
                              static_cast<DWORD>(value.size()),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              nullptr, &outputLength)) {
        return {};
    }
    std::string result(outputLength, '\0');
    if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(value.data()),
                              static_cast<DWORD>(value.size()),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              result.data(), &outputLength)) {
        return {};
    }
    result.resize(outputLength);
    return result;
}

std::wstring Win32ErrorMessage(unsigned long error)
{
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, error, 0,
                                        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring result = length > 0 ? std::wstring(buffer, length)
                                    : L"未知 Windows 错误";
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' ')) {
        result.pop_back();
    }
    return result;
}

void Utf8StreamDecoder::Reset()
{
    carry_.clear();
}

std::wstring Utf8StreamDecoder::Feed(std::span<const unsigned char> bytes)
{
    std::string data = carry_;
    data.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    carry_.clear();
    if (data.empty()) {
        return {};
    }

    size_t carryStart = data.size();
    const size_t begin = data.size() > 3 ? data.size() - 3 : 0;
    for (size_t i = data.size(); i-- > begin;) {
        const unsigned char byte = static_cast<unsigned char>(data[i]);
        size_t expected = 0;
        if ((byte & 0xE0u) == 0xC0u) expected = 2;
        else if ((byte & 0xF0u) == 0xE0u) expected = 3;
        else if ((byte & 0xF8u) == 0xF0u) expected = 4;
        if (expected == 0) {
            continue;
        }
        bool validContinuation = true;
        const size_t available = std::min(expected, data.size() - i);
        for (size_t j = i + 1; j < i + available; ++j) {
            if ((static_cast<unsigned char>(data[j]) & 0xC0u) != 0x80u) {
                validContinuation = false;
                break;
            }
        }
        if (validContinuation && available < expected && i + available == data.size()) {
            carryStart = i;
            break;
        }
    }
    if (carryStart < data.size()) {
        carry_ = data.substr(carryStart);
        data.resize(carryStart);
    }

    return Utf8ToWide(data);
}
