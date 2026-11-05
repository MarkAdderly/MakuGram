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

private:
	Settings();
	~Settings() = default;

	Settings(const Settings &) = delete;
	Settings &operator=(const Settings &) = delete;

	bool _hideMessagesFromBlocked = true;
	bool _quickBanInRequests = true;

	rpl::event_stream<bool> _hideMessagesFromBlockedChanges;
	rpl::event_stream<bool> _quickBanInRequestsChanges;
};

} // namespace MakuGram
