/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/makugram_settings.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "rpl/then.h"
#include "config.h"
#include <QtGui/QImage>

#include <QtCore/QSettings>
#include <QtCore/QStringList>

namespace MakuGram {

namespace {

bool IsRussianConsonant(QChar c) {
	const auto u = c.unicode();
	switch (u) {
	case 0x0431:
	case 0x0432:
	case 0x0433:
	case 0x0434:
	case 0x0436:
	case 0x0437:
	case 0x043A:
	case 0x043B:
	case 0x043C:
	case 0x043D:
	case 0x043F:
	case 0x0440:
	case 0x0441:
	case 0x0442:
	case 0x0444:
	case 0x0445:
	case 0x0446:
	case 0x0447:
	case 0x0448:
	case 0x0449:
		return true;
	default:
		return false;
	}
}

} // namespace

QString NormalizeRussianText(const QString &text) {
	auto res = text.toLower();
	for (auto &ch : res) {
		if (ch == QChar(0x0451) || ch == QChar(0x0401)) {
			ch = QChar(0x0435);
		}
	}
	return res;
}

QString RussianStem(const QString &word) {
	auto res = NormalizeRussianText(word);
	static const QString endings[] = {
		u"ами"_q, u"ями"_q, u"ою"_q, u"ею"_q, u"ейте"_q, u"уйте"_q, u"иями"_q, u"ьями"_q,
		u"ого"_q, u"его"_q, u"ому"_q, u"ему"_q, u"ыми"_q, u"ими"_q, u"ишь"_q, u"ешь"_q, u"ете"_q, u"ите"_q,
		u"ыла"_q, u"ила"_q, u"ыли"_q, u"или"_q, u"ыло"_q, u"ило"_q, u"ных"_q, u"ной"_q, u"ные"_q,
		u"ой"_q, u"ей"_q, u"ем"_q, u"им"_q, u"ом"_q, u"ым"_q, u"ам"_q, u"ям"_q, u"ах"_q, u"ях"_q, u"ов"_q, u"ев"_q,
		u"ая"_q, u"яя"_q, u"ое"_q, u"ее"_q, u"ые"_q, u"ие"_q, u"ый"_q, u"ий"_q, u"ую"_q, u"юю"_q,
		u"ся"_q, u"сь"_q, u"ть"_q, u"ти"_q, u"ла"_q, u"ло"_q, u"ли"_q,
		u"а"_q, u"е"_q, u"и"_q, u"о"_q, u"у"_q, u"ы"_q, u"э"_q, u"ю"_q, u"я"_q
	};
	for (const auto &end : endings) {
		if (res.size() >= end.size() + 3 && res.endsWith(end)) {
			res.chop(end.size());
			break;
		}
	}
	if (res.size() >= 4 && res.endsWith(u'к')) {
		const auto vowel = res[res.size() - 2];
		if (vowel == u'о' || vowel == u'е') {
			const auto c = res[res.size() - 3];
			if (IsRussianConsonant(c)) {
				res.remove(res.size() - 2, 1);
			}
		}
	}
	return res;
}

std::vector<QString> GenerateWordVariants(const QString &clean) {
	auto norm = NormalizeRussianText(clean);
	auto stem = RussianStem(norm);
	std::vector<QString> list;
	const auto addUnique = [&](const QString &s) {
		if (!s.isEmpty() && std::find(list.begin(), list.end(), s) == list.end()) {
			list.push_back(s);
		}
	};
	addUnique(norm);
	addUnique(stem);

	for (const auto &baseWord : { norm, stem }) {
		if (baseWord.size() >= 3 && baseWord.endsWith(u'к') && IsRussianConsonant(baseWord[baseWord.size() - 2])) {
			const auto base = baseWord.left(baseWord.size() - 1);
			addUnique(base + u"ок"_q);
			addUnique(base + u"ек"_q);
			addUnique(base);
		} else if (baseWord.size() >= 4 && baseWord.endsWith(u'к')
				&& (baseWord[baseWord.size() - 2] == u'о' || baseWord[baseWord.size() - 2] == u'е')
				&& IsRussianConsonant(baseWord[baseWord.size() - 3])) {
			const auto base = baseWord.left(baseWord.size() - 2);
			addUnique(base + u"к"_q);
			addUnique(base);
		}
	}
	return list;
}

Settings &Settings::instance() {
	static Settings instance;
	return instance;
}

Settings::Settings() {
	QSettings s("MakuGram", "MakuGram");
	_hideMessagesFromBlocked = s.value("hideMessagesFromBlocked", false).toBool();
	_quickBanInRequests = s.value("quickBanInRequests", false).toBool();
	_alwaysZoomMediaToScreen = s.value("alwaysZoomMediaToScreen", true).toBool();
	_showExactTime = s.value("showExactTime", false).toBool();
	_spamFilterEnabled = s.value("spamFilterEnabled", false).toBool();
	_filterSpamInSearch = s.value("filterSpamInSearch", false).toBool();
	_spamKeywords = s.value("spamKeywords", QString()).toString();
	_showNewbieBadge = s.value("showNewbieBadge", false).toBool();
	_showProfileDetails = s.value("showProfileDetails", true).toBool();
	_hideTypingStatus = s.value("hideTypingStatus", false).toBool();
	_bypassRestrictedContent = s.value("bypassRestrictedContent", false).toBool();
	_showUnreadFilterButton = s.value("showUnreadFilterButton", true).toBool();
	_unreadOnlyActive = false;
	_quickMassDeleteMyMessages = s.value("quickMassDeleteMyMessages", false).toBool();
	_searchUserMessagesInChat = s.value("searchUserMessagesInChat", true).toBool();
	_compactChatList = s.value("compactChatList", false).toBool();
	_disableReactionAnimations = s.value("disableReactionAnimations", false).toBool();
	_disableStories = s.value("disableStories", false).toBool();
	_hidePhoneNumberInSettings = s.value("hidePhoneNumberInSettings", false).toBool();
	_forwardWithoutAuthor = s.value("forwardWithoutAuthor", true).toBool();
	_showMediaResolution = s.value("showMediaResolution", false).toBool();
	_fullscreenMediaSelection = s.value("fullscreenMediaSelection", false).toBool();
	_expandMediaSendDialog = s.value("expandMediaSendDialog", false).toBool();
	_increaseMessageSelectLimit = s.value("increaseMessageSelectLimit", true).toBool();
	_videoVolumeWheel = s.value("videoVolumeWheel", false).toBool();
	_sendMediaHighQuality = s.value("sendMediaHighQuality", false).toBool();
	_streamerMode = s.value("streamerMode", false).toBool();
	_currencyConverter = s.value("currencyConverter", false).toBool();
	_currencyConverterTargets = s.value("currencyConverterTargets", QString("USD,RUB,KZT")).toString();

	parseSpamKeywords();
}

void Settings::parseSpamKeywords() {
	_spamKeywordsList.clear();
	_spamPatterns.clear();
	if (_spamKeywords.trimmed().isEmpty()) {
		return;
	}
	auto parts = QStringList();
	const auto lines = _spamKeywords.split(QRegularExpression(u"[\r\n]+"_q), Qt::SkipEmptyParts);
	for (const auto &line : lines) {
		const auto trimmedLine = line.trimmed();
		if (trimmedLine.isEmpty()) {
			continue;
		}
		if (trimmedLine.contains(',') || trimmedLine.contains(';')) {
			parts.append(trimmedLine.split(QRegularExpression(u"[,;]+"_q), Qt::SkipEmptyParts));
		} else {
			parts.append(trimmedLine.split(QRegularExpression(u"\\s+"_q), Qt::SkipEmptyParts));
		}
	}
	for (const auto &part : parts) {
		const auto trimmed = part.trimmed().toLower();
		if (trimmed.isEmpty()) {
			continue;
		}
		_spamKeywordsList.push_back(trimmed);

		SpamPattern p;
		p.raw = trimmed;
		if (trimmed.startsWith('*') && trimmed.endsWith('*') && trimmed.size() > 2) {
			p.clean = trimmed;
			while (p.clean.startsWith('*')) p.clean.remove(0, 1);
			while (p.clean.endsWith('*')) p.clean.chop(1);
			p.type = PatternType::Contains;
		} else if (trimmed.endsWith('*') && !trimmed.startsWith('*')) {
			p.clean = trimmed;
			while (p.clean.endsWith('*')) p.clean.chop(1);
			p.type = PatternType::Prefix;
		} else if (trimmed.startsWith('*') && !trimmed.endsWith('*')) {
			p.clean = trimmed;
			while (p.clean.startsWith('*')) p.clean.remove(0, 1);
			p.type = PatternType::Suffix;
		} else if (trimmed.contains('*') || trimmed.contains('?')) {
			p.clean = trimmed;
			p.type = PatternType::Regex;
			p.regex = QRegularExpression(
				QRegularExpression::wildcardToRegularExpression(trimmed),
				QRegularExpression::CaseInsensitiveOption);
		} else {
			p.clean = trimmed;
			p.type = PatternType::Exact;
		}
		p.clean = NormalizeRussianText(p.clean);
		p.stem = RussianStem(p.clean);
		p.variants = GenerateWordVariants(p.clean);
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

bool Settings::hideTypingStatus() const {
	return _hideTypingStatus;
}

void Settings::setHideTypingStatus(bool enabled) {
	if (_hideTypingStatus != enabled) {
		_hideTypingStatus = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("hideTypingStatus", enabled);
		_hideTypingStatusChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::hideTypingStatusChanges() const {
	return _hideTypingStatusChanges.events();
}

bool Settings::bypassRestrictedContent() const {
	return _bypassRestrictedContent;
}

void Settings::setBypassRestrictedContent(bool enabled) {
	if (_bypassRestrictedContent != enabled) {
		_bypassRestrictedContent = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("bypassRestrictedContent", enabled);
		_bypassRestrictedContentChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::bypassRestrictedContentChanges() const {
	return _bypassRestrictedContentChanges.events();
}

bool Settings::showUnreadFilterButton() const {
	return _showUnreadFilterButton;
}

void Settings::setShowUnreadFilterButton(bool enabled) {
	if (_showUnreadFilterButton != enabled) {
		_showUnreadFilterButton = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("showUnreadFilterButton", enabled);
		_showUnreadFilterButtonChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::showUnreadFilterButtonChanges() const {
	return _showUnreadFilterButtonChanges.events();
}

bool Settings::unreadOnlyActive() const {
	return _unreadOnlyActive;
}

void Settings::setUnreadOnlyActive(bool active) {
	if (_unreadOnlyActive != active) {
		_unreadOnlyActive = active;
		_unreadOnlyActiveChanges.fire_copy(active);
	}
}

rpl::producer<bool> Settings::unreadOnlyActiveChanges() const {
	return _unreadOnlyActiveChanges.events();
}

bool Settings::quickMassDeleteMyMessages() const {
	return _quickMassDeleteMyMessages;
}

void Settings::setQuickMassDeleteMyMessages(bool enabled) {
	if (_quickMassDeleteMyMessages != enabled) {
		_quickMassDeleteMyMessages = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("quickMassDeleteMyMessages", enabled);
		_quickMassDeleteMyMessagesChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::quickMassDeleteMyMessagesChanges() const {
	return _quickMassDeleteMyMessagesChanges.events();
}

bool Settings::searchUserMessagesInChat() const {
	return _searchUserMessagesInChat;
}

void Settings::setSearchUserMessagesInChat(bool enabled) {
	if (_searchUserMessagesInChat != enabled) {
		_searchUserMessagesInChat = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("searchUserMessagesInChat", enabled);
		_searchUserMessagesInChatChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::searchUserMessagesInChatChanges() const {
	return _searchUserMessagesInChatChanges.events();
}

bool Settings::compactChatList() const {
	return _compactChatList;
}

void Settings::setCompactChatList(bool enabled) {
	if (_compactChatList != enabled) {
		_compactChatList = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("compactChatList", enabled);
		_compactChatListChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::compactChatListChanges() const {
	return _compactChatListChanges.events();
}

bool Settings::disableReactionAnimations() const {
	return _disableReactionAnimations;
}

void Settings::setDisableReactionAnimations(bool enabled) {
	if (_disableReactionAnimations != enabled) {
		_disableReactionAnimations = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("disableReactionAnimations", enabled);
		_disableReactionAnimationsChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::disableReactionAnimationsChanges() const {
	return _disableReactionAnimationsChanges.events();
}

bool Settings::disableStories() const {
	return _disableStories;
}

void Settings::setDisableStories(bool enabled) {
	if (_disableStories != enabled) {
		_disableStories = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("disableStories", enabled);
		_disableStoriesChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::disableStoriesChanges() const {
	return _disableStoriesChanges.events();
}

bool Settings::hidePhoneNumberInSettings() const {
	return _hidePhoneNumberInSettings || _streamerMode;
}

void Settings::setHidePhoneNumberInSettings(bool enabled) {
	if (_hidePhoneNumberInSettings != enabled) {
		_hidePhoneNumberInSettings = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("hidePhoneNumberInSettings", enabled);
		_hidePhoneNumberInSettingsChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::hidePhoneNumberInSettingsChanges() const {
	return _hidePhoneNumberInSettingsChanges.events();
}

rpl::producer<bool> Settings::hidePhoneNumberInSettingsValue() const {
	return rpl::single(hidePhoneNumberInSettings())
		| rpl::then(_hidePhoneNumberInSettingsChanges.events());
}

bool Settings::forwardWithoutAuthor() const {
	return _forwardWithoutAuthor;
}

void Settings::setForwardWithoutAuthor(bool enabled) {
	if (_forwardWithoutAuthor != enabled) {
		_forwardWithoutAuthor = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("forwardWithoutAuthor", enabled);
		_forwardWithoutAuthorChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::forwardWithoutAuthorChanges() const {
	return _forwardWithoutAuthorChanges.events();
}

bool Settings::showMediaResolution() const {
	return _showMediaResolution;
}

void Settings::setShowMediaResolution(bool enabled) {
	if (_showMediaResolution != enabled) {
		_showMediaResolution = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("showMediaResolution", enabled);
		_showMediaResolutionChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::showMediaResolutionChanges() const {
	return _showMediaResolutionChanges.events();
}

bool Settings::fullscreenMediaSelection() const {
	return _fullscreenMediaSelection;
}

void Settings::setFullscreenMediaSelection(bool enabled) {
	if (_fullscreenMediaSelection != enabled) {
		_fullscreenMediaSelection = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("fullscreenMediaSelection", enabled);
		_fullscreenMediaSelectionChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::fullscreenMediaSelectionChanges() const {
	return _fullscreenMediaSelectionChanges.events();
}

bool Settings::expandMediaSendDialog() const {
	return _expandMediaSendDialog;
}

void Settings::setExpandMediaSendDialog(bool enabled) {
	if (_expandMediaSendDialog != enabled) {
		_expandMediaSendDialog = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("expandMediaSendDialog", enabled);
		_expandMediaSendDialogChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::expandMediaSendDialogChanges() const {
	return _expandMediaSendDialogChanges.events();
}

bool Settings::increaseMessageSelectLimit() const {
	return _increaseMessageSelectLimit;
}

void Settings::setIncreaseMessageSelectLimit(bool enabled) {
	if (_increaseMessageSelectLimit != enabled) {
		_increaseMessageSelectLimit = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("increaseMessageSelectLimit", enabled);
		_increaseMessageSelectLimitChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::increaseMessageSelectLimitChanges() const {
	return _increaseMessageSelectLimitChanges.events();
}

bool Settings::videoVolumeWheel() const {
	return _videoVolumeWheel;
}

void Settings::setVideoVolumeWheel(bool enabled) {
	if (_videoVolumeWheel != enabled) {
		_videoVolumeWheel = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("videoVolumeWheel", enabled);
		_videoVolumeWheelChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::videoVolumeWheelChanges() const {
	return _videoVolumeWheelChanges.events();
}

bool Settings::sendMediaHighQuality() const {
	return _sendMediaHighQuality;
}

void Settings::setSendMediaHighQuality(bool enabled) {
	if (_sendMediaHighQuality != enabled) {
		_sendMediaHighQuality = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("sendMediaHighQuality", enabled);
		_sendMediaHighQualityChanges.fire_copy(enabled);
		auto way = Core::App().settings().sendFilesWay();
		way.setSendLargePhotos(enabled);
		Core::App().settings().setSendFilesWay(way);
		Core::App().saveSettingsDelayed();
	}
}

rpl::producer<bool> Settings::sendMediaHighQualityChanges() const {
	return _sendMediaHighQualityChanges.events();
}

bool Settings::streamerMode() const {
	return _streamerMode;
}

void Settings::setStreamerMode(bool enabled) {
	if (_streamerMode != enabled) {
		_streamerMode = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("streamerMode", enabled);
		_streamerModeChanges.fire_copy(enabled);
		_hidePhoneNumberInSettingsChanges.fire_copy(hidePhoneNumberInSettings());
		_showProfileDetailsChanges.fire_copy(showProfileDetails());
	}
}

rpl::producer<bool> Settings::streamerModeChanges() const {
	return _streamerModeChanges.events();
}

rpl::producer<bool> Settings::streamerModeValue() const {
	return rpl::single(_streamerMode) | rpl::then(_streamerModeChanges.events());
}

bool Settings::currencyConverter() const {
	return _currencyConverter;
}

void Settings::setCurrencyConverter(bool enabled) {
	if (_currencyConverter != enabled) {
		_currencyConverter = enabled;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("currencyConverter", enabled);
		_currencyConverterChanges.fire_copy(enabled);
	}
}

rpl::producer<bool> Settings::currencyConverterChanges() const {
	return _currencyConverterChanges.events();
}

QString Settings::currencyConverterTargets() const {
	return _currencyConverterTargets;
}

void Settings::setCurrencyConverterTargets(const QString &targets) {
	if (_currencyConverterTargets != targets) {
		_currencyConverterTargets = targets;
		QSettings s("MakuGram", "MakuGram");
		s.setValue("currencyConverterTargets", targets);
		_currencyConverterTargetsChanges.fire_copy(targets);
	}
}

rpl::producer<QString> Settings::currencyConverterTargetsChanges() const {
	return _currencyConverterTargetsChanges.events();
}

void BlurImageStrong(QImage &image) {
	if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
		return;
	}
	const auto origSize = image.size();
	const auto minDim = std::min(origSize.width(), origSize.height());
	const auto smallDim = std::clamp(minDim / 16, 2, 6);
	const auto midDim = std::clamp(minDim / 4, 8, 20);
	image = image.scaled(smallDim, smallDim, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
		.scaled(midDim, midDim, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
		.scaled(origSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace MakuGram

int MaxSelectedItemsLimit() {
	return MakuGram::Settings::instance().increaseMessageSelectLimit()
		? ExpandedMaxSelectedItems
		: DefaultMaxSelectedItems;
}
