/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/makugram_settings.h"

#include <QtCore/QSettings>
#include <QtCore/QStringList>

namespace MakuGram {

Settings &Settings::instance() {
	static Settings instance;
	return instance;
}

Settings::Settings() {
	QSettings s("MakuGram", "MakuGram");
	_hideMessagesFromBlocked = s.value("hideMessagesFromBlocked", true).toBool();
	_quickBanInRequests = s.value("quickBanInRequests", true).toBool();
	_alwaysZoomMediaToScreen = s.value("alwaysZoomMediaToScreen", true).toBool();
	_showExactTime = s.value("showExactTime", false).toBool();
	_fastDownload = s.value("fastDownload", true).toBool();
	_spamFilterEnabled = s.value("spamFilterEnabled", true).toBool();
	_filterSpamInSearch = s.value("filterSpamInSearch", true).toBool();
	_spamKeywords = s.value("spamKeywords", QString()).toString();
	_showNewbieBadge = s.value("showNewbieBadge", true).toBool();
	_showProfileDetails = s.value("showProfileDetails", true).toBool();
	_quickDeleteAndBan = s.value("quickDeleteAndBan", true).toBool();

	parseSpamKeywords();
}

void Settings::parseSpamKeywords() {
	_spamKeywordsList.clear();
	_spamPatterns.clear();
	if (_spamKeywords.trimmed().isEmpty()) {
		return;
	}
	const auto parts = _spamKeywords.split(QRegularExpression(u"[,;\r\n]+"_q), Qt::SkipEmptyParts);
	for (const auto &part : parts) {
		const auto trimmed = part.trimmed().toLower();
		if (trimmed.isEmpty()) {
			continue;
		}
		_spamKeywordsList.push_back(trimmed);

		SpamPattern p;
		p.raw = trimmed;
		if (trimmed.contains('*') || trimmed.contains('?')) {
			p.isWildcard = true;
			const auto startsWithStar = trimmed.startsWith('*');
			const auto endsWithStar = trimmed.endsWith('*');
			auto clean = trimmed;
			while (clean.startsWith('*')) clean.remove(0, 1);
			while (clean.endsWith('*')) clean.chop(1);
			const auto escaped = QRegularExpression::escape(clean);

			QString pattern;
			if (startsWithStar && endsWithStar) {
				pattern = escaped;
			} else if (startsWithStar) {
				pattern = u"[\\p{L}\\p{N}_]*"_q + escaped + u"(?:$|[^\\p{L}\\p{N}_])"_q;
			} else if (endsWithStar) {
				pattern = u"(?:^|[^\\p{L}\\p{N}_])"_q + escaped + u"[\\p{L}\\p{N}_]*"_q;
			} else {
				pattern = escaped;
			}
			p.regex = QRegularExpression(
				pattern,
				QRegularExpression::CaseInsensitiveOption
				| QRegularExpression::UseUnicodePropertiesOption);
		}
		_spamPatterns.push_back(std::move(p));
	}
}

bool Settings::hideMessagesFromBlocked() const {
	return _hideMessagesFromBlocked;
}

void Settings::setHideMessagesFromBlocked(bool hide) {
	if (_hideMessagesFromBlocked != hide) {
		_hideMessagesFromBlocked = hide;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("hideMessagesFromBlocked", hide);
		_hideMessagesFromBlockedChanges.fire_copy(hide);
	}
}

rpl::producer<bool> Settings::hideMessagesFromBlockedChanges() const {
	return _hideMessagesFromBlockedChanges.events();
}

bool Settings::quickBanInRequests() const {
	return _quickBanInRequests;
}

void Settings::setQuickBanInRequests(bool enabled) {
	if (_quickBanInRequests != enabled) {
		_quickBanInRequests = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("quickBanInRequests", enabled);
		_quickBanInRequestsChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::quickBanInRequestsChanges() const {
	return _quickBanInRequestsChanges.events();
}

bool Settings::alwaysZoomMediaToScreen() const {
	return _alwaysZoomMediaToScreen;
}

void Settings::setAlwaysZoomMediaToScreen(bool enabled) {
	if (_alwaysZoomMediaToScreen != enabled) {
		_alwaysZoomMediaToScreen = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("alwaysZoomMediaToScreen", enabled);
		_alwaysZoomMediaToScreenChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::alwaysZoomMediaToScreenChanges() const {
	return _alwaysZoomMediaToScreenChanges.events();
}

bool Settings::showExactTime() const {
	return _showExactTime;
}

void Settings::setShowExactTime(bool enabled) {
	if (_showExactTime != enabled) {
		_showExactTime = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("showExactTime", enabled);
		_showExactTimeChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::showExactTimeChanges() const {
	return _showExactTimeChanges.events();
}

bool Settings::fastDownload() const {
	return _fastDownload;
}

void Settings::setFastDownload(bool enabled) {
	if (_fastDownload != enabled) {
		_fastDownload = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("fastDownload", enabled);
		_fastDownloadChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::fastDownloadChanges() const {
	return _fastDownloadChanges.events();
}

bool Settings::spamFilterEnabled() const {
	return _spamFilterEnabled;
}

void Settings::setSpamFilterEnabled(bool enabled) {
	if (_spamFilterEnabled != enabled) {
		_spamFilterEnabled = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("spamFilterEnabled", enabled);
		_spamFilterEnabledChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::spamFilterEnabledChanges() const {
	return _spamFilterEnabledChanges.events();
}

bool Settings::filterSpamInSearch() const {
	return _filterSpamInSearch;
}

void Settings::setFilterSpamInSearch(bool enabled) {
	if (_filterSpamInSearch != enabled) {
		_filterSpamInSearch = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("filterSpamInSearch", enabled);
		_filterSpamInSearchChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::filterSpamInSearchChanges() const {
	return _filterSpamInSearchChanges.events();
}

QString Settings::spamKeywords() const {
	return _spamKeywords;
}

void Settings::setSpamKeywords(const QString &keywords) {
	if (_spamKeywords != keywords) {
		_spamKeywords = keywords;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("spamKeywords", keywords);
		parseSpamKeywords();
		_spamKeywordsChanges.fire_copy(keywords);
	}
}

rpl::producer<QString> Settings::spamKeywordsChanges() const {
	return _spamKeywordsChanges.events();
}

const std::vector<QString> &Settings::spamKeywordsList() const {
	return _spamKeywordsList;
}

const std::vector<SpamPattern> &Settings::spamPatterns() const {
	return _spamPatterns;
}

bool Settings::showNewbieBadge() const {
	return _showNewbieBadge;
}

void Settings::setShowNewbieBadge(bool enabled) {
	if (_showNewbieBadge != enabled) {
		_showNewbieBadge = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("showNewbieBadge", enabled);
		_showNewbieBadgeChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::showNewbieBadgeChanges() const {
	return _showNewbieBadgeChanges.events();
}

bool Settings::showProfileDetails() const {
	return _showProfileDetails;
}

void Settings::setShowProfileDetails(bool enabled) {
	if (_showProfileDetails != enabled) {
		_showProfileDetails = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("showProfileDetails", enabled);
		_showProfileDetailsChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::showProfileDetailsChanges() const {
	return _showProfileDetailsChanges.events();
}

bool Settings::quickDeleteAndBan() const {
	return _quickDeleteAndBan;
}

void Settings::setQuickDeleteAndBan(bool enabled) {
	if (_quickDeleteAndBan != enabled) {
		_quickDeleteAndBan = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("quickDeleteAndBan", enabled);
		_quickDeleteAndBanChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::quickDeleteAndBanChanges() const {
	return _quickDeleteAndBanChanges.events();
}

} // namespace MakuGram
