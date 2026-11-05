/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#pragma once

class HistoryItem;

namespace MakuGram {

[[nodiscard]] bool IsBlockedMessageHidden(const HistoryItem *item);
[[nodiscard]] bool IsSpamKeywordMessageHidden(const HistoryItem *item);

} // namespace MakuGram
