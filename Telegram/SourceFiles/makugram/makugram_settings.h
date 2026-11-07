/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#pragma once

#include "rpl/event_stream.h"
#include "rpl/producer.h"
#include <QtCore/QString>
#include <QtCore/QRegularExpression>
#include <vector>

namespace MakuGram {

enum class PatternType {
	Exact,
	Prefix,
	Suffix,
	Contains,
	Regex,
};

struct SpamPattern {
	QString raw;
	QString clean;
	QString stem;
	std::vector<QString> variants;
	PatternType type = PatternType::Exact;
	QRegularExpression regex;
};

[[nodiscard]] QString NormalizeRussianText(const QString &text);
[[nodiscard]] QString RussianStem(const QString &word);
[[nodiscard]] std::vector<QString> GenerateWordVariants(const QString &clean);

class Settings final {
public:
	static Settings &instance();

	[[nodiscard]] bool hideMessagesFromBlocked() const;
	void setHideMessagesFromBlocked(bool hide);
	[[nodiscard]] rpl::producer<bool> hideMessagesFromBlockedChanges() const;

	[[nodiscard]] bool quickBanInRequests() const;
	void setQuickBanInRequests(bool enabled);
	[[nodiscard]] rpl::producer<bool> quickBanInRequestsChanges() const;

	[[nodiscard]] bool alwaysZoomMediaToScreen() const;
	void setAlwaysZoomMediaToScreen(bool enabled);
	[[nodiscard]] rpl::producer<bool> alwaysZoomMediaToScreenChanges() const;

	[[nodiscard]] bool showExactTime() const;
	void setShowExactTime(bool enabled);
	[[nodiscard]] rpl::producer<bool> showExactTimeChanges() const;

	[[nodiscard]] bool spamFilterEnabled() const;
	void setSpamFilterEnabled(bool enabled);
	[[nodiscard]] rpl::producer<bool> spamFilterEnabledChanges() const;

	[[nodiscard]] bool filterSpamInSearch() const;
	void setFilterSpamInSearch(bool enabled);
	[[nodiscard]] rpl::producer<bool> filterSpamInSearchChanges() const;

	[[nodiscard]] QString spamKeywords() const;
	void setSpamKeywords(const QString &keywords);
	[[nodiscard]] rpl::producer<QString> spamKeywordsChanges() const;
	[[nodiscard]] const std::vector<QString> &spamKeywordsList() const;
	[[nodiscard]] const std::vector<SpamPattern> &spamPatterns() const;

	[[nodiscard]] bool showNewbieBadge() const;
	void setShowNewbieBadge(bool enabled);
	[[nodiscard]] rpl::producer<bool> showNewbieBadgeChanges() const;

	[[nodiscard]] bool showProfileDetails() const;
	void setShowProfileDetails(bool enabled);
	[[nodiscard]] rpl::producer<bool> showProfileDetailsChanges() const;

	[[nodiscard]] bool hideTypingStatus() const;
	void setHideTypingStatus(bool enabled);
	[[nodiscard]] rpl::producer<bool> hideTypingStatusChanges() const;

	[[nodiscard]] bool bypassRestrictedContent() const;
	void setBypassRestrictedContent(bool enabled);
	[[nodiscard]] rpl::producer<bool> bypassRestrictedContentChanges() const;

	[[nodiscard]] bool showUnreadFilterButton() const;
	void setShowUnreadFilterButton(bool enabled);
	[[nodiscard]] rpl::producer<bool> showUnreadFilterButtonChanges() const;

	[[nodiscard]] bool unreadOnlyActive() const;
	void setUnreadOnlyActive(bool active);
	[[nodiscard]] rpl::producer<bool> unreadOnlyActiveChanges() const;

	[[nodiscard]] bool quickMassDeleteMyMessages() const;
	void setQuickMassDeleteMyMessages(bool enabled);
	[[nodiscard]] rpl::producer<bool> quickMassDeleteMyMessagesChanges() const;

	[[nodiscard]] bool searchUserMessagesInChat() const;
	void setSearchUserMessagesInChat(bool enabled);
	[[nodiscard]] rpl::producer<bool> searchUserMessagesInChatChanges() const;

	[[nodiscard]] bool compactChatList() const;
	void setCompactChatList(bool enabled);
	[[nodiscard]] rpl::producer<bool> compactChatListChanges() const;

	[[nodiscard]] bool disableReactionAnimations() const;
	void setDisableReactionAnimations(bool enabled);
	[[nodiscard]] rpl::producer<bool> disableReactionAnimationsChanges() const;

	[[nodiscard]] bool disableStories() const;
	void setDisableStories(bool enabled);
	[[nodiscard]] rpl::producer<bool> disableStoriesChanges() const;

	[[nodiscard]] bool hidePhoneNumberInSettings() const;
	void setHidePhoneNumberInSettings(bool enabled);
	[[nodiscard]] rpl::producer<bool> hidePhoneNumberInSettingsChanges() const;

	[[nodiscard]] bool forwardWithoutAuthor() const;
	void setForwardWithoutAuthor(bool enabled);
	[[nodiscard]] rpl::producer<bool> forwardWithoutAuthorChanges() const;

	[[nodiscard]] bool showMediaResolution() const;
	void setShowMediaResolution(bool enabled);
	[[nodiscard]] rpl::producer<bool> showMediaResolutionChanges() const;

	[[nodiscard]] bool fullscreenMediaSelection() const;
	void setFullscreenMediaSelection(bool enabled);
	[[nodiscard]] rpl::producer<bool> fullscreenMediaSelectionChanges() const;

	[[nodiscard]] bool expandMediaSendDialog() const;
	void setExpandMediaSendDialog(bool enabled);
	[[nodiscard]] rpl::producer<bool> expandMediaSendDialogChanges() const;

	[[nodiscard]] bool increaseMessageSelectLimit() const;
	void setIncreaseMessageSelectLimit(bool enabled);
	[[nodiscard]] rpl::producer<bool> increaseMessageSelectLimitChanges() const;

private:
	Settings();
	~Settings() = default;

	Settings(const Settings &) = delete;
	Settings &operator=(const Settings &) = delete;

	void parseSpamKeywords();

	bool _hideMessagesFromBlocked = true;
	bool _quickBanInRequests = true;
	bool _alwaysZoomMediaToScreen = true;
	bool _showExactTime = false;
	bool _spamFilterEnabled = true;
	bool _filterSpamInSearch = true;
	QString _spamKeywords;
	std::vector<QString> _spamKeywordsList;
	std::vector<SpamPattern> _spamPatterns;
	bool _showNewbieBadge = true;
	bool _showProfileDetails = true;
	bool _hideTypingStatus = false;
	bool _bypassRestrictedContent = true;
	bool _showUnreadFilterButton = true;
	bool _unreadOnlyActive = false;
	bool _quickMassDeleteMyMessages = true;
	bool _searchUserMessagesInChat = true;
	bool _compactChatList = false;
	bool _disableReactionAnimations = false;
	bool _disableStories = false;
	bool _hidePhoneNumberInSettings = false;
	bool _forwardWithoutAuthor = true;
	bool _showMediaResolution = true;
	bool _fullscreenMediaSelection = true;
	bool _expandMediaSendDialog = true;
	bool _increaseMessageSelectLimit = true;

	rpl::event_stream<bool> _hideMessagesFromBlockedChanges;
	rpl::event_stream<bool> _quickBanInRequestsChanges;
	rpl::event_stream<bool> _alwaysZoomMediaToScreenChanges;
	rpl::event_stream<bool> _showExactTimeChanges;
	rpl::event_stream<bool> _spamFilterEnabledChanges;
	rpl::event_stream<bool> _filterSpamInSearchChanges;
	rpl::event_stream<QString> _spamKeywordsChanges;
	rpl::event_stream<bool> _showNewbieBadgeChanges;
	rpl::event_stream<bool> _showProfileDetailsChanges;
	rpl::event_stream<bool> _hideTypingStatusChanges;
	rpl::event_stream<bool> _bypassRestrictedContentChanges;
	rpl::event_stream<bool> _showUnreadFilterButtonChanges;
	rpl::event_stream<bool> _unreadOnlyActiveChanges;
	rpl::event_stream<bool> _quickMassDeleteMyMessagesChanges;
	rpl::event_stream<bool> _searchUserMessagesInChatChanges;
	rpl::event_stream<bool> _compactChatListChanges;
	rpl::event_stream<bool> _disableReactionAnimationsChanges;
	rpl::event_stream<bool> _disableStoriesChanges;
	rpl::event_stream<bool> _hidePhoneNumberInSettingsChanges;
	rpl::event_stream<bool> _forwardWithoutAuthorChanges;
	rpl::event_stream<bool> _showMediaResolutionChanges;
	rpl::event_stream<bool> _fullscreenMediaSelectionChanges;
	rpl::event_stream<bool> _expandMediaSendDialogChanges;
	rpl::event_stream<bool> _increaseMessageSelectLimitChanges;
};

} // namespace MakuGram
