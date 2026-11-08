/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/currency_converter.h"

#include "makugram/makugram_settings.h"
#include "core/click_handler_types.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_menu_icons.h"
#include "styles/style_widgets.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "mainwidget.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtGui/QCursor>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <algorithm>

namespace MakuGram {
namespace {

QString FlagForCurrency(const QString &code) {
	const auto c = code.toUpper();
	if (c == u"USD"_q) return QString::fromUtf8("🇺🇸");
	if (c == u"RUB"_q) return QString::fromUtf8("🇷🇺");
	if (c == u"UAH"_q) return QString::fromUtf8("🇺🇦");
	if (c == u"EUR"_q) return QString::fromUtf8("🇪🇺");
	if (c == u"KZT"_q) return QString::fromUtf8("🇰🇿");
	if (c == u"BYN"_q) return QString::fromUtf8("🇧🇾");
	if (c == u"TRY"_q) return QString::fromUtf8("🇹🇷");
	if (c == u"CNY"_q) return QString::fromUtf8("🇨🇳");
	if (c == u"JPY"_q) return QString::fromUtf8("🇯🇵");
	if (c == u"IDR"_q) return QString::fromUtf8("🇮🇩");
	if (c == u"GBP"_q) return QString::fromUtf8("🇬🇧");
	if (c == u"PLN"_q) return QString::fromUtf8("🇵🇱");
	if (c == u"THB"_q) return QString::fromUtf8("🇹🇭");
	if (c == u"AED"_q) return QString::fromUtf8("🇦🇪");
	if (c == u"GEL"_q) return QString::fromUtf8("🇬🇪");
	if (c == u"AMD"_q) return QString::fromUtf8("🇦🇲");
	if (c == u"CAD"_q) return QString::fromUtf8("🇨🇦");
	if (c == u"AUD"_q) return QString::fromUtf8("🇦🇺");
	if (c == u"BRL"_q) return QString::fromUtf8("🇧🇷");
	if (c == u"CHF"_q) return QString::fromUtf8("🇨🇭");
	if (c == u"INR"_q) return QString::fromUtf8("🇮🇳");
	if (c == u"KRW"_q) return QString::fromUtf8("🇰🇷");
	if (c == u"SGD"_q) return QString::fromUtf8("🇸🇬");
	if (c == u"ILS"_q) return QString::fromUtf8("🇮🇱");
	if (c == u"UZS"_q) return QString::fromUtf8("🇺🇿");
	if (c == u"USDT"_q) return QString::fromUtf8("🪙");
	return QString();
}

const QRegularExpression &SuffixRegex() {
	static const auto expression = QRegularExpression(
		u"(?<![\\w\\d])"
		"(\\d{1,3}(?:[., ]\\d{3})+(?:[.,]\\d+)?|\\d+(?:[.,]\\d+)?)"
		"\\s*"
		"(k|к|тыс(?:\\.|яч[а-я]*)?|kk|кк|m|м|млн(?:\\.)?|миллион[а-я]*|b|млрд(?:\\.)?|миллиард[а-я]*)?"
		"\\s*"
		"(руб(?:лей|ля|ли|ль|\\.)?|₽|rur|rub|idr|rp|рупи[йяи]|usd|\\$|доллар(?:ов|а|ы)?|долл(?:\\.)?|бакс(?:ов|а|ы)?|eur|€|евро|гривен|гривн[ыа]|грн(?:\\.)?|₴|uah|тенге|тг(?:\\.)?|₸|kzt|бел\\.?\\s*руб(?:лей|ля|ли|\\.)?|белруб|byn|турецких\\s+лир|лир[ыа]?|₺|try|фунт(?:ов|а|ы)?|£|gbp|юан(?:ей|я|и|ь)|¥|cny|йен(?:ей|а|ы|у|ах)?|иен(?:ей|а|ы|у|ах)?|jpy|злотых|злоты[ей]|zł|zl|pln|бат(?:ов|а|ы)?|฿|thb|дирхам(?:ов|а|ы)?|aed|лари|₾|gel|драм(?:ов|а|ы)?|֏|amd|usdt)"
		"(?![\\w\\d])"_q,
		QRegularExpression::CaseInsensitiveOption);
	return expression;
}

const QRegularExpression &PrefixRegex() {
	static const auto expression = QRegularExpression(
		u"(?<![\\w\\d])"
		"(\\$|€|£|₽|₴|₸|₺|¥|฿|₾|֏|Rp\\.?\\s*)"
		"\\s*"
		"(\\d{1,3}(?:[., ]\\d{3})+(?:[.,]\\d+)?|\\d+(?:[.,]\\d+)?)"
		"\\s*"
		"(k|к|тыс(?:\\.|яч[а-я]*)?|kk|кк|m|м|млн(?:\\.)?|миллион[а-я]*|b|млрд(?:\\.)?|миллиард[а-я]*)?"
		"(?![\\w\\d])"_q,
		QRegularExpression::CaseInsensitiveOption);
	return expression;
}

QString MapCurrency(QString cur) {
	cur = cur.toLower().trimmed();
	if (cur.endsWith('.')) {
		cur.chop(1);
	}
	if (cur == u"₽"_q || cur == u"rub"_q || cur == u"rur"_q || cur.startsWith(u"руб"_q)) {
		return u"RUB"_q;
	} else if (cur == u"$"_q || cur == u"usd"_q || cur.startsWith(u"долл"_q) || cur.startsWith(u"бакс"_q)) {
		return u"USD"_q;
	} else if (cur == u"€"_q || cur == u"eur"_q || cur.startsWith(u"евро"_q)) {
		return u"EUR"_q;
	} else if (cur == u"idr"_q || cur == u"rp"_q || cur.startsWith(u"рупи"_q)) {
		return u"IDR"_q;
	} else if (cur == u"₴"_q || cur == u"uah"_q || cur.startsWith(u"грн"_q) || cur.startsWith(u"грив"_q)) {
		return u"UAH"_q;
	} else if (cur == u"₸"_q || cur == u"kzt"_q || cur.startsWith(u"тенге"_q) || cur == u"тг"_q) {
		return u"KZT"_q;
	} else if (cur == u"byn"_q || cur.contains(u"бел"_q)) {
		return u"BYN"_q;
	} else if (cur == u"₺"_q || cur == u"try"_q || cur.contains(u"лир"_q)) {
		return u"TRY"_q;
	} else if (cur == u"£"_q || cur == u"gbp"_q || cur.startsWith(u"фунт"_q)) {
		return u"GBP"_q;
	} else if (cur == u"¥"_q || cur == u"cny"_q || cur.startsWith(u"юан"_q)) {
		return u"CNY"_q;
	} else if (cur == u"jpy"_q || cur.startsWith(u"йен"_q) || cur.startsWith(u"иен"_q)) {
		return u"JPY"_q;
	} else if (cur == u"zł"_q || cur == u"zl"_q || cur == u"pln"_q || cur.startsWith(u"злот"_q)) {
		return u"PLN"_q;
	} else if (cur == u"฿"_q || cur == u"thb"_q || cur.startsWith(u"бат"_q)) {
		return u"THB"_q;
	} else if (cur == u"aed"_q || cur.startsWith(u"дирхам"_q)) {
		return u"AED"_q;
	} else if (cur == u"₾"_q || cur == u"gel"_q || cur.startsWith(u"лари"_q)) {
		return u"GEL"_q;
	} else if (cur == u"֏"_q || cur == u"amd"_q || cur.startsWith(u"драм"_q)) {
		return u"AMD"_q;
	} else if (cur == u"usdt"_q) {
		return u"USDT"_q;
	}
	return QString();
}

double ParseAmount(QString numStr, const QString &multStr) {
	numStr = numStr.trimmed();
	numStr.remove(' ');
	numStr.remove(QChar(0x00A0)); // non-breaking space
	numStr.remove(QChar(0x202F)); // narrow non-breaking space

	const auto dotCount = numStr.count('.');
	const auto commaCount = numStr.count(',');

	if (dotCount > 1) {
		numStr.remove('.');
	} else if (commaCount > 1) {
		numStr.remove(',');
	} else if (dotCount == 1 && commaCount == 1) {
		const auto dotIndex = numStr.indexOf('.');
		const auto commaIndex = numStr.indexOf(',');
		if (dotIndex < commaIndex) {
			numStr.remove('.');
			numStr.replace(',', '.');
		} else {
			numStr.remove(',');
		}
	} else if (commaCount == 1) {
		const auto commaIndex = numStr.indexOf(',');
		const auto fracLen = numStr.size() - commaIndex - 1;
		if (fracLen == 3 && numStr.size() > 4) {
			numStr.remove(',');
		} else {
			numStr.replace(',', '.');
		}
	} else if (dotCount == 1) {
		const auto dotIndex = numStr.indexOf('.');
		const auto fracLen = numStr.size() - dotIndex - 1;
		if (fracLen == 3 && numStr.size() > 4) {
			numStr.remove('.');
		}
	}

	bool ok = false;
	double val = numStr.toDouble(&ok);
	if (!ok || val <= 0.0) {
		return 0.0;
	}

	const auto m = multStr.toLower().trimmed();
	if (m == u"k"_q || m == u"к"_q || m.startsWith(u"тыс"_q)) {
		val *= 1'000.0;
	} else if (m == u"kk"_q || m == u"кк"_q || m == u"m"_q || m == u"м"_q || m.startsWith(u"млн"_q) || m.startsWith(u"мил"_q)) {
		val *= 1'000'000.0;
	} else if (m == u"b"_q || m.startsWith(u"млрд"_q)) {
		val *= 1'000'000'000.0;
	}

	return val;
}

QString FormatNumber(double amount) {
	if (!std::isfinite(amount) || amount <= 0.0) {
		return u"0"_q;
	}
	if (amount >= 1'000'000'000.0) {
		return QString::number(amount / 1'000'000'000.0, 'f', 2) + u"B"_q;
	} else if (amount >= 10'000'000.0) {
		return QString::number(amount / 1'000'000.0, 'f', 2) + u"M"_q;
	} else if (amount >= 1'000.0) {
		const auto rounded = qRound64(amount);
		auto s = QString::number(rounded);
		for (int i = s.size() - 3; i > 0; i -= 3) {
			s.insert(i, ' ');
		}
		return s;
	} else if (amount >= 10.0) {
		return QString::number(amount, 'f', 2);
	} else if (amount >= 0.01) {
		return QString::number(amount, 'f', 2);
	} else if (amount > 0.0) {
		return QString::number(amount, 'f', 4);
	}
	return u"0"_q;
}

struct MatchCandidate {
	int from = 0;
	int till = 0;
	double amount = 0.0;
	QString currency;
};

class CurrencyClickHandler final : public ClickHandler {
public:
	CurrencyClickHandler(QString data, QString text);

	void onClick(ClickContext context) const override;
	QString tooltip() const override;
	TextEntity getTextEntity() const override;

private:
	QString _data;
	QString _text;
	double _amount = 0.0;
	QString _currency;
};

CurrencyClickHandler::CurrencyClickHandler(QString data, QString text)
: _data(std::move(data))
, _text(std::move(text)) {
	const auto prefix = u"internal:makugram-currency:"_q;
	if (_data.startsWith(prefix)) {
		const auto rest = _data.mid(prefix.size());
		const auto parts = rest.split(':');
		if (parts.size() >= 2) {
			_amount = parts[0].toDouble();
			_currency = parts[1].toUpper();
		}
	}
}

QString CurrencyClickHandler::tooltip() const {
	if (_amount <= 0.0 || _currency.isEmpty()) {
		return QString();
	}
	const auto targets = Settings::instance().currencyConverterTargets()
		.split(',', Qt::SkipEmptyParts);
	QStringList lines;
	for (const auto &rawTarget : targets) {
		const auto target = rawTarget.trimmed().toUpper();
		if (target == _currency) {
			continue;
		}
		const auto conv = CurrencyRatesManager::instance().convert(
			_amount,
			_currency,
			target);
		if (conv > 0.0) {
			lines.push_back(u"• "_q + CurrencyRatesManager::instance().formatConverted(conv, target));
		}
	}
	if (lines.isEmpty()) {
		return QString();
	}
	return _text + u" ≈\n"_q + lines.join(u"\n"_q);
}

void CurrencyClickHandler::onClick(ClickContext context) const {
	if (context.button != Qt::LeftButton) {
		return;
	}
	const auto my = context.other.value<ClickHandlerContext>();
	const auto controller = my.sessionWindow.get();
	if (!controller) {
		return;
	}
	const auto menu = Ui::CreateChild<Ui::PopupMenu>(
		controller->content(),
		st::popupMenuWithIcons);

	const auto pos = QCursor::pos();

	const auto origFlag = FlagForCurrency(_currency);
	const auto title = (origFlag.isEmpty() ? QString() : (origFlag + ' '))
		+ _text + u" ("_q + _currency + u")"_q;
	const auto copyOriginal = [val = _text, show = controller->uiShow()] {
		TextUtilities::SetClipboardText(TextForMimeData::Simple(val));
		show->showToast(QString::fromUtf8("Скопировано: ") + val);
	};
	menu->addAction(title, copyOriginal, &st::menuIconCopy);

	menu->addSeparator();

	const auto targets = Settings::instance().currencyConverterTargets()
		.split(',', Qt::SkipEmptyParts);
	for (const auto &rawTarget : targets) {
		const auto target = rawTarget.trimmed().toUpper();
		if (target == _currency) {
			continue;
		}
		const auto conv = CurrencyRatesManager::instance().convert(
			_amount,
			_currency,
			target);
		if (conv > 0.0) {
			const auto formatted = CurrencyRatesManager::instance().formatConverted(conv, target);
			const auto copyThis = [val = formatted, show = controller->uiShow()] {
				TextUtilities::SetClipboardText(TextForMimeData::Simple(val));
				show->showToast(QString::fromUtf8("Скопировано: ") + val);
			};
			menu->addAction(formatted, copyThis, &st::menuIconCopy);
		}
	}

	menu->addSeparator();

	menu->addAction(
		QString::fromUtf8("+ Настроить валюты..."),
		[controller] { ShowCurrencyTargetsBox(controller); },
		&st::menuIconSettings);

	menu->popup(pos);
}

ClickHandler::TextEntity CurrencyClickHandler::getTextEntity() const {
	return { EntityType::CustomUrl, _data };
}

} // namespace

CurrencyRatesManager &CurrencyRatesManager::instance() {
	static CurrencyRatesManager manager;
	return manager;
}

CurrencyRatesManager::CurrencyRatesManager() {
	initFallbacks();
	loadCachedRates();
	checkAndUpdateRates();
}

void CurrencyRatesManager::initFallbacks() {
	const auto lock = std::lock_guard(_mutex);
	_ratesFromUSD[u"USD"_q] = 1.0;
	_ratesFromUSD[u"USDT"_q] = 1.0;
	_ratesFromUSD[u"EUR"_q] = 0.92;
	_ratesFromUSD[u"RUB"_q] = 82.0;
	_ratesFromUSD[u"IDR"_q] = 18000.0;
	_ratesFromUSD[u"UAH"_q] = 45.0;
	_ratesFromUSD[u"KZT"_q] = 450.0;
	_ratesFromUSD[u"BYN"_q] = 3.0;
	_ratesFromUSD[u"TRY"_q] = 50.0;
	_ratesFromUSD[u"GBP"_q] = 0.75;
	_ratesFromUSD[u"CNY"_q] = 6.7;
	_ratesFromUSD[u"JPY"_q] = 155.0;
	_ratesFromUSD[u"PLN"_q] = 3.9;
	_ratesFromUSD[u"THB"_q] = 33.5;
	_ratesFromUSD[u"AED"_q] = 3.67;
	_ratesFromUSD[u"GEL"_q] = 2.6;
	_ratesFromUSD[u"AMD"_q] = 360.0;
}

void CurrencyRatesManager::loadCachedRates() {
	const auto lock = std::lock_guard(_mutex);
	QSettings s("MakuGram", "MakuGram");
	_lastUpdateTime = s.value("currencyRatesLastUpdate", 0).toLongLong();
	const auto cached = s.value("currencyRatesCache", QString()).toString();
	if (!cached.isEmpty()) {
		const auto pairs = cached.split(';', Qt::SkipEmptyParts);
		for (const auto &pair : pairs) {
			const auto idx = pair.indexOf(':');
			if (idx > 0) {
				const auto cur = pair.left(idx).trimmed().toUpper();
				const auto rate = pair.mid(idx + 1).toDouble();
				if (rate > 0.0) {
					_ratesFromUSD[cur] = rate;
				}
			}
		}
	}
}

void CurrencyRatesManager::saveCachedRates() {
	const auto lock = std::lock_guard(_mutex);
	QSettings s("MakuGram", "MakuGram");
	s.setValue("currencyRatesLastUpdate", _lastUpdateTime);
	QStringList pairs;
	pairs.reserve(_ratesFromUSD.size());
	for (const auto &[cur, rate] : _ratesFromUSD) {
		pairs.push_back(cur + ':' + QString::number(rate, 'g', 10));
	}
	s.setValue("currencyRatesCache", pairs.join(';'));
}

void CurrencyRatesManager::checkAndUpdateRates() {
	const auto now = QDateTime::currentSecsSinceEpoch();
	if (_lastUpdateTime > 0 && (now - _lastUpdateTime) < 43200) { // 12 hours
		return;
	}
	if (!_network) {
		_network = std::make_unique<QNetworkAccessManager>(this);
	}
	const auto url = QUrl(u"https://open.er-api.com/v6/latest/USD"_q);
	auto req = QNetworkRequest(url);
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	const auto reply = _network->get(req);
	connect(reply, &QNetworkReply::finished, this, [=] {
		reply->deleteLater();
		if (reply->error() == QNetworkReply::NoError) {
			parseRatesJson(reply->readAll());
		}
	});
}

void CurrencyRatesManager::parseRatesJson(const QByteArray &json) {
	QJsonParseError err;
	const auto doc = QJsonDocument::fromJson(json, &err);
	if (err.error == QJsonParseError::NoError && doc.isObject()) {
		const auto lock = std::lock_guard(_mutex);
		const auto obj = doc.object();
		const auto ratesObj = obj.value(u"rates"_q).toObject();
		for (auto it = ratesObj.begin(); it != ratesObj.end(); ++it) {
			const auto val = it.value().toDouble(0.0);
			if (val > 0.0) {
				_ratesFromUSD[it.key().toUpper()] = val;
			}
		}
		_ratesFromUSD[u"USD"_q] = 1.0;
		_ratesFromUSD[u"USDT"_q] = 1.0;
		_lastUpdateTime = QDateTime::currentSecsSinceEpoch();
		saveCachedRates();
	}
}

double CurrencyRatesManager::convert(
		double amount,
		const QString &from,
		const QString &to) const {
	if (!std::isfinite(amount) || amount <= 0.0) {
		return 0.0;
	}
	const auto fromCode = from.toUpper();
	const auto toCode = to.toUpper();
	if (fromCode == toCode) {
		return amount;
	}
	const auto lock = std::lock_guard(_mutex);
	const auto fromIt = _ratesFromUSD.find(fromCode);
	const auto toIt = _ratesFromUSD.find(toCode);
	if (fromIt == _ratesFromUSD.end() || toIt == _ratesFromUSD.end()) {
		return 0.0;
	}
	const auto fromRate = fromIt->second;
	const auto toRate = toIt->second;
	if (fromRate <= 0.0 || toRate <= 0.0) {
		return 0.0;
	}
	const auto res = (amount / fromRate) * toRate;
	return std::isfinite(res) ? res : 0.0;
}

QString CurrencyRatesManager::formatConverted(
		double amount,
		const QString &targetCurrency) const {
	const auto num = FormatNumber(amount);
	const auto code = targetCurrency.toUpper();
	const auto flag = FlagForCurrency(code);
	const auto prefix = flag.isEmpty() ? QString() : (flag + ' ');

	if (code == u"USD"_q) {
		return prefix + u"$"_q + num + u" USD"_q;
	} else if (code == u"EUR"_q) {
		return prefix + u"€"_q + num + u" EUR"_q;
	} else if (code == u"RUB"_q) {
		return prefix + num + u" ₽"_q;
	} else if (code == u"IDR"_q) {
		return prefix + num + u" IDR"_q;
	} else if (code == u"UAH"_q) {
		return prefix + num + u" ₴"_q;
	} else if (code == u"KZT"_q) {
		return prefix + num + u" ₸"_q;
	} else if (code == u"BYN"_q) {
		return prefix + num + u" бел. руб"_q;
	} else if (code == u"TRY"_q) {
		return prefix + num + u" ₺"_q;
	} else if (code == u"GBP"_q) {
		return prefix + u"£"_q + num + u" GBP"_q;
	} else if (code == u"CNY"_q) {
		return prefix + u"¥"_q + num + u" CNY"_q;
	} else if (code == u"JPY"_q) {
		return prefix + u"¥"_q + num + u" JPY"_q;
	} else if (code == u"PLN"_q) {
		return prefix + num + u" zł"_q;
	} else if (code == u"THB"_q) {
		return prefix + num + u" ฿"_q;
	} else if (code == u"AED"_q) {
		return prefix + num + u" AED"_q;
	} else if (code == u"GEL"_q) {
		return prefix + num + u" ₾"_q;
	} else if (code == u"AMD"_q) {
		return prefix + num + u" ֏"_q;
	} else if (code == u"USDT"_q) {
		return prefix + num + u" USDT"_q;
	}
	return prefix + num + u" "_q + code;
}

TextWithEntities AddCurrencyLinks(TextWithEntities text) {
	if (text.empty()) {
		return text;
	}

	std::vector<MatchCandidate> candidates;

	// Suffix matching
	auto i = SuffixRegex().globalMatch(text.text);
	while (i.hasNext()) {
		const auto m = i.next();
		const auto numStr = m.captured(1);
		const auto multStr = m.captured(2);
		const auto curStr = m.captured(3);
		const auto cur = MapCurrency(curStr);
		if (cur.isEmpty()) {
			continue;
		}
		const auto amount = ParseAmount(numStr, multStr);
		if (amount <= 0.0) {
			continue;
		}
		candidates.push_back({
			.from = int(m.capturedStart()),
			.till = int(m.capturedEnd()),
			.amount = amount,
			.currency = cur,
		});
	}

	// Prefix matching
	auto j = PrefixRegex().globalMatch(text.text);
	while (j.hasNext()) {
		const auto m = j.next();
		const auto curStr = m.captured(1);
		const auto numStr = m.captured(2);
		const auto multStr = m.captured(3);
		const auto cur = MapCurrency(curStr);
		if (cur.isEmpty()) {
			continue;
		}
		const auto amount = ParseAmount(numStr, multStr);
		if (amount <= 0.0) {
			continue;
		}
		candidates.push_back({
			.from = int(m.capturedStart()),
			.till = int(m.capturedEnd()),
			.amount = amount,
			.currency = cur,
		});
	}

	if (candidates.empty()) {
		return text;
	}

	std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
		return a.from < b.from;
	});

	std::vector<MatchCandidate> nonOverlapping;
	int lastTill = -1;
	for (const auto &c : candidates) {
		if (c.from >= lastTill) {
			nonOverlapping.push_back(c);
			lastTill = c.till;
		}
	}

	auto &entities = text.entities;
	for (const auto &c : nonOverlapping) {
		bool overlaps = false;
		for (const auto &existing : entities) {
			const auto existingEnd = existing.offset() + existing.length();
			if (existing.offset() < c.till && existingEnd > c.from) {
				if ((existing.type() == EntityType::Spoiler || existing.type() == EntityType::Blockquote)
					&& existing.offset() <= c.from && existingEnd >= c.till) {
					continue;
				}
				overlaps = true;
				break;
			}
		}
		if (overlaps) {
			continue;
		}

		const auto url = u"internal:makugram-currency:"_q
			+ QString::number(c.amount, 'f', 4)
			+ u":"_q
			+ c.currency;

		entities.push_back(
			EntityInText(
				EntityType::CustomUrl,
				c.from,
				c.till - c.from,
				url));
	}

	ranges::sort(entities, std::less<>(), &EntityInText::offset);

	return text;
}

ClickHandlerPtr CreateCurrencyClickHandler(
		const QString &data,
		const QString &text) {
	return std::make_shared<CurrencyClickHandler>(data, text);
}

void ShowCurrencyTargetsBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(540);
		box->setMaxHeight(690);
		box->setTitle(rpl::single(QString::fromUtf8("Выбор валют для конвертации")));

		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(QString::fromUtf8("Отметьте валюты, в которые переводить найденные в чате суммы:")),
				st::boxDividerLabel),
			style::margins(
				st::boxRowPadding.left(),
				5,
				st::boxRowPadding.right(),
				5));

		struct CurrencyOption {
			QString code;
			QString name;
		};
		const auto options = std::vector<CurrencyOption>{
			{ u"USD"_q, QString::fromUtf8("🇺🇸 Доллары (USD)") },
			{ u"RUB"_q, QString::fromUtf8("🇷🇺 Рубли (RUB)") },
			{ u"UAH"_q, QString::fromUtf8("🇺🇦 Гривны (UAH)") },
			{ u"EUR"_q, QString::fromUtf8("🇪🇺 Евро (EUR)") },
			{ u"KZT"_q, QString::fromUtf8("🇰🇿 Казахские тенге (KZT)") },
			{ u"BYN"_q, QString::fromUtf8("🇧🇾 Белорусские рубли (BYN)") },
			{ u"TRY"_q, QString::fromUtf8("🇹🇷 Турецкие лиры (TRY)") },
			{ u"CNY"_q, QString::fromUtf8("🇨🇳 Китайские юани (CNY)") },
			{ u"JPY"_q, QString::fromUtf8("🇯🇵 Японские йены (JPY)") },
			{ u"IDR"_q, QString::fromUtf8("🇮🇩 Индонезийские рупии (IDR)") },
		};

		const auto currentTargets = Settings::instance().currencyConverterTargets()
			.split(',', Qt::SkipEmptyParts);
		auto currentSet = base::flat_set<QString>();
		for (const auto &t : currentTargets) {
			currentSet.insert(t.trimmed().toUpper());
		}

		auto checkBoxes = std::vector<std::pair<QString, Ui::Checkbox*>>();
		for (const auto &opt : options) {
			const auto isChecked = currentSet.contains(opt.code);
			currentSet.erase(opt.code);
			const auto cb = box->addRow(
				object_ptr<Ui::Checkbox>(
					box,
					opt.name,
					isChecked,
					st::defaultBoxCheckbox),
				style::margins(
					st::boxRowPadding.left(),
					2,
					st::boxRowPadding.right(),
					2));
			cb->setCursor(style::cur_pointer);
			checkBoxes.push_back({ opt.code, cb });
		}

		QStringList extraList;
		for (const auto &extra : currentSet) {
			extraList.push_back(extra);
		}

		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(QString::fromUtf8("Другие валюты (через запятую, например: GBP, PLN, THB, AED):")),
				st::boxDividerLabel),
			style::margins(
				st::boxRowPadding.left(),
				6,
				st::boxRowPadding.right(),
				2));

		const auto extraField = box->addRow(
			object_ptr<Ui::InputField>(
				box,
				st::defaultInputField,
				rpl::single(QString::fromUtf8("Коды валют")),
				extraList.join(u", "_q)),
			style::margins(
				st::boxRowPadding.left(),
				0,
				st::boxRowPadding.right(),
				6));

		box->addButton(tr::lng_settings_save(), [=] {
			QStringList selected;
			for (const auto &[code, cb] : checkBoxes) {
				if (cb->checked()) {
					selected.push_back(code);
				}
			}
			const auto customExtra = extraField->getLastText().split(',', Qt::SkipEmptyParts);
			for (const auto &c : customExtra) {
				const auto trimmed = c.trimmed().toUpper();
				if (!trimmed.isEmpty() && !selected.contains(trimmed)) {
					selected.push_back(trimmed);
				}
			}
			if (selected.isEmpty()) {
				selected.push_back(u"USD"_q);
			}
			Settings::instance().setCurrencyConverterTargets(selected.join(','));
			box->closeBox();
			controller->showToast(QString::fromUtf8("Настройки валют сохранены"));
		});

		box->addButton(tr::lng_cancel(), [=] {
			box->closeBox();
		});
	}));
}

} // namespace MakuGram
