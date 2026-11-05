/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#pragma once

#include "rpl/event_stream.h"
#include "rpl/producer.h"

namespace MakuGram {

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

private:
	Settings();
	~Settings() = default;

	Settings(const Settings &) = delete;
	Settings &operator=(const Settings &) = delete;

	bool _hideMessagesFromBlocked = true;
	bool _quickBanInRequests = true;
	bool _alwaysZoomMediaToScreen = true;

	rpl::event_stream<bool> _hideMessagesFromBlockedChanges;
	rpl::event_stream<bool> _quickBanInRequestsChanges;
	rpl::event_stream<bool> _alwaysZoomMediaToScreenChanges;
};

} // namespace MakuGram
