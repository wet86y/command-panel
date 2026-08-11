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
    result.minimumWidth = s(500);
    result.minimumHeight = s(480);
    width = std::max(width, result.minimumWidth);
    height = std::max(height, result.minimumHeight);

    const int margin = s(22);
    const int contentWidth = width - margin * 2;
    result.name = Rect(margin, s(18), contentWidth, s(34));
    result.version = Rect(margin, result.name.bottom + s(1), contentWidth, s(20));
    result.developer = Rect(margin, result.version.bottom + s(1), contentWidth, s(20));
    result.repository = Rect(margin, result.developer.bottom + s(1), contentWidth, s(22));

    const int updateTop = result.repository.bottom + s(18);
    result.status = Rect(margin, updateTop, contentWidth, s(36));
    result.progress = Rect(margin, result.status.bottom + s(8), contentWidth, s(12));
    const int buttonHeight = s(34);
    const int buttonGap = s(8);
    const int halfWidth = (contentWidth - buttonGap) / 2;
    const int actionTop = result.progress.bottom + s(14);
    result.check = Rect(margin, actionTop, halfWidth, buttonHeight);
    result.download = result.check;
    result.install = result.check;
    result.acceleration = Rect(margin, actionTop, contentWidth, s(26));
    result.pauseResume = Rect(margin, actionTop, halfWidth, buttonHeight);
    result.nextNode = Rect(result.pauseResume.right + buttonGap, actionTop, halfWidth, buttonHeight);
    result.background = Rect(margin, actionTop + buttonHeight + buttonGap, halfWidth, buttonHeight);
    result.cancel = Rect(result.background.right + buttonGap, actionTop + buttonHeight + buttonGap, halfWidth, buttonHeight);

    const bool transfer = presentation == AboutPresentation::Downloading || presentation == AboutPresentation::Paused;
    const bool hasNotes = presentation == AboutPresentation::Available || transfer ||
                          presentation == AboutPresentation::Completed || presentation == AboutPresentation::Failed ||
                          presentation == AboutPresentation::Cancelled;
    int actionsBottom = actionTop + buttonHeight;
    if (presentation == AboutPresentation::Available) {
        result.download = Rect(margin, actionTop + s(26) + buttonGap, halfWidth, buttonHeight);
        result.nextNode = Rect(result.download.right + buttonGap, result.download.top, halfWidth, buttonHeight);
        actionsBottom = result.download.bottom;
    }
    if (transfer) actionsBottom = result.cancel.bottom;
    if (presentation == AboutPresentation::Idle || presentation == AboutPresentation::Checking) {
        result.progress.bottom = result.progress.top;
        actionsBottom = actionTop + buttonHeight;
    }
    const int notesTop = actionsBottom + s(26);
    result.notes = Rect(margin, notesTop, contentWidth, std::max(s(70), height - notesTop - margin));
    if (!hasNotes) result.notes.bottom = result.notes.top;
    return result;
}
