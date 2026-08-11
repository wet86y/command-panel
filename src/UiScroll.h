#pragma once

#include <windows.h>

namespace UiScroll {

struct Metrics
{
    int minimum = 0;
    int maximum = 0;
    UINT page = 0;
    int position = 0;
};

struct Thumb
{
    bool visible = false;
    int top = 0;
    int height = 0;
};

struct WheelAction
{
    int lines = 0;
    int pages = 0;
};

Metrics ReadMetrics(HWND window);
bool IsScrollable(const Metrics& metrics);
Thumb CalculateThumb(const Metrics& metrics, int trackHeight, int minimumThumbHeight);
WheelAction AccumulateWheel(int delta, UINT scrollLines, int& remainder);
bool ScrollTextControl(HWND control, int delta, UINT scrollLines, int& remainder);
UINT SystemWheelScrollLines();

} // namespace UiScroll
