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

struct SpamPattern {
	QString raw;
	bool isWildcard = false;
	QRegularExpression regex;
};

class Settings final {
public:
	static Settings &instance();

	// 1. Blocked messages filter
	[[nodiscard]] bool hideMessagesFromBlocked() const;
	void setHideMessagesFromBlocked(bool hide);
	[[nodiscard]] rpl::producer<bool> hideMessagesFromBlockedChanges() const;

	// 2. Quick ban in join requests
	[[nodiscard]] bool quickBanInRequests() const;
	void setQuickBanInRequests(bool enabled);
	[[nodiscard]] rpl::producer<bool> quickBanInRequestsChanges() const;

	// 3. Always fit media to screen
	[[nodiscard]] bool alwaysZoomMediaToScreen() const;
	void setAlwaysZoomMediaToScreen(bool enabled);
	[[nodiscard]] rpl::producer<bool> alwaysZoomMediaToScreenChanges() const;

	// 4. Exact message time (with seconds)
	[[nodiscard]] bool showExactTime() const;
	void setShowExactTime(bool enabled);
	[[nodiscard]] rpl::producer<bool> showExactTimeChanges() const;

	// 5. Multi-threaded fast download
	[[nodiscard]] bool fastDownload() const;
	void setFastDownload(bool enabled);
	[[nodiscard]] rpl::producer<bool> fastDownloadChanges() const;

	// 6. Local keyword spam filter
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

	// 7. Newbie chat badge (< 3 months)
	[[nodiscard]] bool showNewbieBadge() const;
	void setShowNewbieBadge(bool enabled);
	[[nodiscard]] rpl::producer<bool> showNewbieBadgeChanges() const;

	// 8. Telegram ID & Registration date in profile
	[[nodiscard]] bool showProfileDetails() const;
	void setShowProfileDetails(bool enabled);
	[[nodiscard]] rpl::producer<bool> showProfileDetailsChanges() const;

	// 9. Quick Delete and Ban in context menu
	[[nodiscard]] bool quickDeleteAndBan() const;
	void setQuickDeleteAndBan(bool enabled);
	[[nodiscard]] rpl::producer<bool> quickDeleteAndBanChanges() const;

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
	bool _fastDownload = true;
	bool _spamFilterEnabled = true;
	bool _filterSpamInSearch = true;
	QString _spamKeywords;
	std::vector<QString> _spamKeywordsList;
	std::vector<SpamPattern> _spamPatterns;
	bool _showNewbieBadge = true;
	bool _showProfileDetails = true;
	bool _quickDeleteAndBan = true;

	rpl::event_stream<bool> _hideMessagesFromBlockedChanges;
	rpl::event_stream<bool> _quickBanInRequestsChanges;
	rpl::event_stream<bool> _alwaysZoomMediaToScreenChanges;
	rpl::event_stream<bool> _showExactTimeChanges;
	rpl::event_stream<bool> _fastDownloadChanges;
	rpl::event_stream<bool> _spamFilterEnabledChanges;
	rpl::event_stream<bool> _filterSpamInSearchChanges;
	rpl::event_stream<QString> _spamKeywordsChanges;
	rpl::event_stream<bool> _showNewbieBadgeChanges;
	rpl::event_stream<bool> _showProfileDetailsChanges;
	rpl::event_stream<bool> _quickDeleteAndBanChanges;
};

} // namespace MakuGram
