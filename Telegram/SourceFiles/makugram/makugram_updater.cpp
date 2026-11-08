/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/makugram_updater.h"

#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/buttons.h"
#include "ui/basic_click_handlers.h"
#include "lang/lang_keys.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QRegularExpression>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

namespace MakuGram {
namespace {

struct VersionParts {
	int major = 0;
	int minor = 0;
	int patch = 0;

	bool operator<(const VersionParts &other) const {
		if (major != other.major) return major < other.major;
		if (minor != other.minor) return minor < other.minor;
		return patch < other.patch;
	}
};

VersionParts ParseVersion(QString tag) {
	tag = tag.trimmed();
	if (tag.startsWith('v', Qt::CaseInsensitive)) {
		tag = tag.mid(1);
	}
	const auto parts = tag.split('.');
	VersionParts result;
	if (!parts.isEmpty()) result.major = parts[0].toInt();
	if (parts.size() > 1) result.minor = parts[1].toInt();
	if (parts.size() > 2) result.patch = parts[2].toInt();
	return result;
}

class UpdateManager final : public QObject {
public:
	static UpdateManager &instance() {
		static UpdateManager mgr;
		return mgr;
	}

	void check(base::weak_ptr<Window::SessionController> weakController, bool silent) {
		if (_inProgress) {
			if (!silent) {
				if (const auto controller = weakController.get()) {
					controller->showToast(QString::fromUtf8("Проверка обновлений уже выполняется..."));
				}
			}
			return;
		}
		_inProgress = true;

		if (!_network) {
			_network = std::make_unique<QNetworkAccessManager>(this);
		}

		const auto url = QUrl(u"https://api.github.com/repos/MarkAdderly/MakuGram/releases/latest"_q);
		auto request = QNetworkRequest(url);
		request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
		request.setRawHeader("User-Agent", "MakuGram-App");
		request.setRawHeader("Accept", "application/vnd.github.v3+json");

		const auto reply = _network->get(request);
		connect(reply, &QNetworkReply::finished, this, [=]() {
			_inProgress = false;
			reply->deleteLater();

			const auto controller = weakController.get();

			if (reply->error() != QNetworkReply::NoError) {
				if (!silent && controller) {
					controller->showToast(QString::fromUtf8("Не удалось проверить обновления. Проверьте соединение с интернетом."));
				}
				return;
			}

			const auto data = reply->readAll();
			QJsonParseError parseError;
			const auto doc = QJsonDocument::fromJson(data, &parseError);
			if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
				if (!silent && controller) {
					controller->showToast(QString::fromUtf8("Ошибка обработки ответа от GitHub."));
				}
				return;
			}

			const auto obj = doc.object();
			const auto tagName = obj.value(u"tag_name"_q).toString();
			const auto releaseName = obj.value(u"name"_q).toString();
			const auto htmlUrl = obj.value(u"html_url"_q).toString();
			const auto body = obj.value(u"body"_q).toString();

			const auto currentVer = ParseVersion(kVersion);
			const auto latestVer = ParseVersion(tagName);

			if (currentVer < latestVer) {
				if (controller) {
					showUpdateBox(controller, tagName, releaseName, htmlUrl, body);
				}
			} else {
				if (!silent && controller) {
					controller->showToast(QString::fromUtf8("У вас установлена последняя версия MakuGram (v%1)").arg(kVersion));
				}
			}
		});
	}

private:
	void showUpdateBox(
			not_null<Window::SessionController*> controller,
			const QString &tag,
			const QString &name,
			const QString &url,
			const QString &body) {
		controller->show(Box([=](not_null<Ui::GenericBox*> box) {
			box->setWidth(500);
			box->setTitle(rpl::single(QString::fromUtf8("Доступно обновление MakuGram!")));

			box->addRow(
				object_ptr<Ui::FlatLabel>(
					box,
					QString::fromUtf8("Найдена новая версия: %1 (текущая: v%2)%3")
						.arg(tag.startsWith('v') ? tag : ("v" + tag))
						.arg(kVersion)
						.arg(name.isEmpty() ? QString() : ("\n" + name)),
					st::boxLabel),
				st::boxRowPadding);

			if (!body.trimmed().isEmpty()) {
				const auto cleanBody = body.left(300) + (body.length() > 300 ? u"..."_q : QString());
				box->addRow(
					object_ptr<Ui::FlatLabel>(
						box,
						cleanBody,
						st::boxDividerLabel),
					st::boxRowPadding);
			}

			box->addButton(rpl::single(QString::fromUtf8("Скачать обновление")), [=] {
				UrlClickHandler::Open(url.isEmpty()
					? u"https://github.com/MarkAdderly/MakuGram/releases/latest"_q
					: url);
				box->closeBox();
			});

			box->addButton(tr::lng_cancel(), [=] {
				box->closeBox();
			});
		}));
	}

	std::unique_ptr<QNetworkAccessManager> _network;
	bool _inProgress = false;
};

} // namespace

void CheckForUpdates(Window::SessionController *controller, bool silent) {
	if (!controller) {
		return;
	}
	UpdateManager::instance().check(base::make_weak(controller), silent);
}

} // namespace MakuGram
