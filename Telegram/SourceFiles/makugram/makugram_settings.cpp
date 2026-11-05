/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/makugram_settings.h"

#include <QtCore/QSettings>

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

} // namespace MakuGram
