/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#pragma once

#include <QtCore/QString>

namespace Window {
class SessionController;
} // namespace Window

namespace MakuGram {

inline constexpr auto kVersion = "1.0.0";
inline constexpr auto kGitHubRepo = "MarkAdderly/MakuGram";

void CheckForUpdates(Window::SessionController *controller, bool silent = false);

} // namespace MakuGram
