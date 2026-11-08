/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#pragma once

#include "ui/text/text_entity.h"
#include "ui/click_handler.h"
#include "rpl/producer.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtNetwork/QNetworkAccessManager>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace Window {
class SessionController;
} // namespace Window

namespace MakuGram {

// Adds custom currency entities into text.
[[nodiscard]] TextWithEntities AddCurrencyLinks(TextWithEntities text);

// Creates click handler for currency entity.
[[nodiscard]] ClickHandlerPtr CreateCurrencyClickHandler(
	const QString &data,
	const QString &text);

// Dialog to configure target currencies for conversion.
void ShowCurrencyTargetsBox(not_null<Window::SessionController*> controller);

// Rates manager singleton.
class CurrencyRatesManager final : public QObject {
public:
	static CurrencyRatesManager &instance();

	[[nodiscard]] double convert(
		double amount,
		const QString &from,
		const QString &to) const;

	[[nodiscard]] QString formatConverted(
		double amount,
		const QString &targetCurrency) const;

	void checkAndUpdateRates();

private:
	CurrencyRatesManager();
	~CurrencyRatesManager() override = default;

	void initFallbacks();
	void loadCachedRates();
	void saveCachedRates();
	void parseRatesJson(const QByteArray &json);

	mutable std::mutex _mutex;
	std::unique_ptr<QNetworkAccessManager> _network;
	std::unordered_map<QString, double> _ratesFromUSD;
	qint64 _lastUpdateTime = 0;
};

} // namespace MakuGram
