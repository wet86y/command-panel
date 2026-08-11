#include "AboutLayout.h"

#include "UiTheme.h"

#include <algorithm>

namespace {
RECT Rect(int left, int top, int width, int height)
{
    return RECT{left, top, left + std::max(1, width), top + std::max(1, height)};
}
}

AboutLayout CalculateAboutLayout(int width, int height, UINT dpi,
                                 AboutPresentation presentation) noexcept
{
    const auto s = [dpi](int value) { return Ui::Scale(value, dpi); };
    AboutLayout result{};
    result.minimumWidth = s(520);
    result.minimumHeight = s(510);
    width = std::max(width, result.minimumWidth);
    height = std::max(height, result.minimumHeight);

    const int margin = s(18);
    const int gap = s(12);
    const int cardWidth = width - margin * 2;
    result.productCard = Rect(margin, margin, cardWidth, s(104));
    result.icon = Rect(margin + s(16), margin + s(18), s(62), s(62));
    result.repository = Rect(result.productCard.right - s(128), result.productCard.top + s(60), s(112), s(30));
    const int textLeft = result.icon.right + s(14);
    const int textWidth = std::max(s(80), static_cast<int>(result.repository.left) - textLeft - s(10));
    result.name = Rect(textLeft, margin + s(18), textWidth, s(28));
    result.version = Rect(textLeft, result.name.bottom + s(3), textWidth, s(22));
    result.developer = Rect(textLeft, result.version.bottom + s(2), textWidth, s(22));

    const int updateTop = result.productCard.bottom + gap;
    result.updateCard = Rect(margin, updateTop, cardWidth, height - updateTop - margin);
    const int contentLeft = result.updateCard.left + s(16);
    const int contentWidth = cardWidth - s(32);
    result.currentVersion = Rect(contentLeft, updateTop + s(16), contentWidth, s(22));
    result.status = Rect(contentLeft, result.currentVersion.bottom + s(7), contentWidth, s(38));
    result.progress = Rect(contentLeft, result.status.bottom + s(8), contentWidth, s(12));
    result.notes = Rect(contentLeft, result.progress.bottom + s(12), contentWidth, s(88));

    const int primaryWidth = s(114);
    const int transferWidth = s(98);
    const int cancelWidth = s(72);
    const int nodeWidth = s(92);
    const int buttonHeight = s(34);
    const int actionTop = result.updateCard.bottom - s(16) - buttonHeight * 2 - s(8);
    result.check = Rect(contentLeft, actionTop, primaryWidth, buttonHeight);
    result.download = Rect(result.check.right + s(8), actionTop, primaryWidth, buttonHeight);
    result.install = Rect(result.download.right + s(8), actionTop, primaryWidth, buttonHeight);
    result.acceleration = Rect(contentLeft, actionTop - s(30), s(126), s(24));
    result.pauseResume = Rect(contentLeft, actionTop + buttonHeight + s(8), transferWidth, buttonHeight);
    result.background = Rect(result.pauseResume.right + s(8), actionTop + buttonHeight + s(8), transferWidth, buttonHeight);
    result.cancel = Rect(result.background.right + s(8), actionTop + buttonHeight + s(8), cancelWidth, buttonHeight);
    result.nextNode = Rect(result.cancel.right + s(8), actionTop + buttonHeight + s(8), nodeWidth, buttonHeight);
    if (presentation == AboutPresentation::Idle || presentation == AboutPresentation::Checking) {
        result.notes.bottom = result.notes.top;
        result.progress.bottom = result.progress.top;
    }
    return result;
}
