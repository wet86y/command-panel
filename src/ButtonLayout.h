#pragma once

#include <windows.h>

#include <cstddef>
#include <vector>

struct ButtonLayoutResult
{
    std::vector<RECT> cards;
    int columns = 1;
    int contentHeight = 0;
    int maximumScroll = 0;
};

ButtonLayoutResult CalculateButtonLayout(int clientWidth, int clientHeight, UINT dpi,
                                         std::size_t cardCount, int scrollPosition);
