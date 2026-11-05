/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/makugram_filter.h"

#include "makugram/makugram_settings.h"
#include "history/history.h"
#include "history/history_item.h"
#include "data/data_peer.h"
#include "data/data_user.h"

namespace MakuGram {

bool IsBlockedMessageHidden(const HistoryItem *item) {
	if (!item) {
		return false;
	}
	if (!Settings::instance().hideMessagesFromBlocked()) {
		return false;
	}
	if (item->out()) {
		return false;
	}
	const auto history = item->history();
	if (!history) {
		return false;
	}
	const auto peer = history->peer;
	if (!peer || peer->isUser()) {
		// Do not hide messages in direct 1-to-1 PM with the blocked user
		return false;
	}

	const auto from = item->from();
	if (from && from->isUser() && from->asUser()->isBlocked()) {
		return true;
	}
	if (const auto bot = item->viaBot()) {
		if (bot->isBlocked()) {
			return true;
		}
	}
	return false;
}

bool IsSpamKeywordMessageHidden(const HistoryItem *item) {
	if (!item) {
		return false;
	}
	const auto &settings = Settings::instance();
	if (!settings.spamFilterEnabled()) {
		return false;
	}
	const auto &patterns = settings.spamPatterns();
	if (patterns.empty()) {
		return false;
	}
	if (item->out()) {
		return false;
	}
	const auto &text = item->originalText().text;
	if (text.isEmpty()) {
		return false;
	}
	for (const auto &p : patterns) {
		if (p.isWildcard) {
			if (p.regex.match(text).hasMatch()) {
				return true;
			}
		} else {
			if (text.contains(p.raw, Qt::CaseInsensitive)) {
				return true;
			}
		}
	}
	return false;
}

bool IsSearchMessageHidden(const HistoryItem *item) {
	if (!item) {
		return false;
	}
	const auto &settings = Settings::instance();
	if (!settings.filterSpamInSearch()) {
		return false;
	}
	return IsSpamKeywordMessageHidden(item);
}

} // namespace MakuGram
