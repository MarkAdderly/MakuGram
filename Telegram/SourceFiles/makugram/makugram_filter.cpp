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
	if (p.type == PatternType::Regex) {
		return p.regex.isValid() && p.regex.match(text).hasMatch();
	}

	const auto normText = NormalizeRussianText(text);

	bool anySubstr = false;
	for (const auto &v : p.variants) {
		if (normText.contains(v)) {
			anySubstr = true;
			break;
		}
	}
	if (!anySubstr) {
		return false;
	}

	if (p.type == PatternType::Contains) {
		return true;
	}

	const int n = normText.size();
	for (int i = 0; i < n;) {
		while (i < n && !normText[i].isLetterOrNumber()) {
			++i;
		}
		if (i >= n) break;
		const int start = i;
		while (i < n && normText[i].isLetterOrNumber()) {
			++i;
		}
		const auto word = normText.mid(start, i - start);
		const auto wordStem = RussianStem(word);

		for (const auto &v : p.variants) {
			if (p.type == PatternType::Prefix) {
				if (word.startsWith(v) || wordStem.startsWith(v)) {
					return true;
				}
			} else if (p.type == PatternType::Suffix) {
				if (word.endsWith(v) || wordStem.endsWith(v)) {
					return true;
				}
			} else if (p.type == PatternType::Exact) {
				if (word == v || wordStem == v || word == p.clean) {
					return true;
				}
			}
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
