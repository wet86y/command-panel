#include "UiScroll.h"

#include <algorithm>
#include <cstdint>

namespace UiScroll {

Metrics ReadMetrics(HWND window)
{
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    if (window == nullptr || !GetScrollInfo(window, SB_VERT, &info)) return {};
    return Metrics{info.nMin, info.nMax, info.nPage, info.nPos};
}

bool IsScrollable(const Metrics& metrics)
{
    if (metrics.maximum < metrics.minimum || metrics.page == 0) return false;
    const std::int64_t range = static_cast<std::int64_t>(metrics.maximum) - metrics.minimum + 1;
    return range > static_cast<std::int64_t>(metrics.page);
}

Thumb CalculateThumb(const Metrics& metrics, int trackHeight, int minimumThumbHeight)
{
    if (!IsScrollable(metrics) || trackHeight <= 0) return {};

    const std::int64_t range = static_cast<std::int64_t>(metrics.maximum) - metrics.minimum + 1;
    const int thumbHeight = std::clamp(
        static_cast<int>(static_cast<std::int64_t>(trackHeight) * metrics.page / range),
        std::min(trackHeight, std::max(1, minimumThumbHeight)), trackHeight);
    const std::int64_t maximumPosition = std::max<std::int64_t>(
        metrics.minimum, static_cast<std::int64_t>(metrics.maximum) - metrics.page + 1);
    const std::int64_t positionRange = maximumPosition - metrics.minimum;
    const std::int64_t clampedPosition = std::clamp<std::int64_t>(
        metrics.position, metrics.minimum, maximumPosition);
    const int top = positionRange == 0 ? 0 : static_cast<int>(
        static_cast<std::int64_t>(trackHeight - thumbHeight) *
        (clampedPosition - metrics.minimum) / positionRange);
    return Thumb{true, top, thumbHeight};
}

WheelAction AccumulateWheel(int delta, UINT scrollLines, int& remainder)
{
    remainder += delta;
    const int detents = remainder / WHEEL_DELTA;
    remainder -= detents * WHEEL_DELTA;
    if (detents == 0) return {};
    if (scrollLines == WHEEL_PAGESCROLL) return WheelAction{0, detents};
    return WheelAction{detents * static_cast<int>(scrollLines), 0};
}

bool ScrollTextControl(HWND control, int delta, UINT scrollLines, int& remainder)
{
    const WheelAction action = AccumulateWheel(delta, scrollLines, remainder);
    bool moved = false;
    if (action.pages != 0) {
        const int count = std::abs(action.pages);
        const WPARAM direction = action.pages > 0 ? SB_PAGEUP : SB_PAGEDOWN;
        for (int index = 0; index < count; ++index)
            SendMessageW(control, EM_SCROLL, direction, 0);
        moved = true;
    } else if (action.lines != 0) {
        SendMessageW(control, EM_LINESCROLL, 0, static_cast<LPARAM>(-action.lines));
        moved = true;
    }
    return moved;
}

UINT SystemWheelScrollLines()
{
    UINT lines = 3;
    if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0)) lines = 3;
    return lines;
}

} // namespace UiScroll
