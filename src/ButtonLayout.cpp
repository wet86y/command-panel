#include "ButtonLayout.h"

#include "UiTheme.h"

#include <algorithm>

ButtonLayoutResult CalculateButtonLayout(int clientWidth, int clientHeight, UINT dpi,
                                         std::size_t cardCount, int scrollPosition)
{
    ButtonLayoutResult result;
    const int margin = Ui::Scale(12, dpi);
    const int gap = Ui::Scale(14, dpi);
    const int height = Ui::CompactScale(64, dpi);
    const int widthLimit = std::max(100, clientWidth - margin * 2);
    result.columns = widthLimit >= Ui::Scale(760, dpi)
        ? 4 : std::max(1, (widthLimit + gap) / (Ui::Scale(180, dpi) + gap));
    const int slotWidth = std::max(100, (widthLimit - gap * (result.columns - 1)) / result.columns);
    const int cardInset = std::min(Ui::Scale(10, dpi), std::max(0, (slotWidth - 100) / 2));
    const int cardWidth = std::max(100, slotWidth - cardInset * 2);
    const int rows = cardCount == 0 ? 0 :
        (static_cast<int>(cardCount) + result.columns - 1) / result.columns;
    result.contentHeight = rows == 0 ? margin * 2 :
        margin * 2 + rows * height + (rows - 1) * gap;
    result.maximumScroll = std::max(0, result.contentHeight - std::max(0, clientHeight));
    const int scroll = std::clamp(scrollPosition, 0, result.maximumScroll);

    result.cards.reserve(cardCount);
    for (std::size_t index = 0; index < cardCount; ++index) {
        const int column = static_cast<int>(index % static_cast<std::size_t>(result.columns));
        const int row = static_cast<int>(index / static_cast<std::size_t>(result.columns));
        const int x = margin + column * (slotWidth + gap) + (slotWidth - cardWidth) / 2;
        const int y = margin + row * (height + gap) - scroll;
        result.cards.push_back(RECT{x, y, x + cardWidth, y + height});
    }
    return result;
}
