#pragma once

#include <string_view>

// Release builds compile this to a no-op.  Local RelWithDebInfo builds may opt
// in through COMMAND_PANEL_LOCAL_UPDATE_DIAGNOSTICS while diagnosing update
// hand-offs on a user's actual install path.
void RecordLocalUpdateDiagnostic(std::wstring_view message) noexcept;
[[nodiscard]] bool LocalUpdateDiagnosticsEnabled() noexcept;
