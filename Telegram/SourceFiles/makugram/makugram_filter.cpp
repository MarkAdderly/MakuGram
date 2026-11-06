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
#include <QtCore/QStringView>

namespace MakuGram {
namespace {

bool MatchesPattern(const QString &text, const SpamPattern &p) {
	if (text.isEmpty()) {
		return false;
	}
	if (!p.clean.isEmpty() && !text.contains(p.clean, Qt::CaseInsensitive)) {
		return false;
	}

	if (p.type == PatternType::Contains) {
		return true;
	}

	if (p.type == PatternType::Prefix) {
		const int n = text.size();
		for (int i = 0; i < n;) {
			while (i < n && !text[i].isLetterOrNumber()) {
				++i;
			}
			if (i >= n) break;
			const int start = i;
			while (i < n && text[i].isLetterOrNumber()) {
				++i;
			}
			const auto word = QStringView(text).mid(start, i - start);
			if (word.startsWith(p.clean, Qt::CaseInsensitive)) {
				return true;
			}
		}
		return false;
	}

	if (p.type == PatternType::Suffix) {
		const int n = text.size();
		for (int i = 0; i < n;) {
			while (i < n && !text[i].isLetterOrNumber()) {
				++i;
			}
			if (i >= n) break;
			const int start = i;
			while (i < n && text[i].isLetterOrNumber()) {
				++i;
			}
			const auto word = QStringView(text).mid(start, i - start);
			if (word.endsWith(p.clean, Qt::CaseInsensitive)) {
				return true;
			}
		}
		return false;
	}

	if (p.type == PatternType::Exact) {
		return text.contains(p.clean, Qt::CaseInsensitive);
	}

	if (p.type == PatternType::Regex) {
		if (p.regex.isValid()) {
			return p.regex.match(text).hasMatch();
		}
	}

	return false;
}

} // namespace

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
		if (MatchesPattern(text, p)) {
			return true;
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
	const auto &patterns = settings.spamPatterns();
	if (patterns.empty()) {
		return false;
	}

	const auto checkText = [&](const QString &text) {
		if (text.isEmpty()) {
			return false;
		}
		for (const auto &p : patterns) {
			if (MatchesPattern(text, p)) {
				return true;
			}
		}
		return false;
	};

	if (checkText(item->originalText().text)) {
		return true;
	}
	if (checkText(item->notificationText().text)) {
		return true;
	}
	if (checkText(item->translatedText().text)) {
		return true;
	}
	return false;
}

} // namespace MakuGram
