/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/makugram_updater.h"
#include "makugram/makugram_lang.h"
#include "settings.h"
#include "core/application.h"

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
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QProcess>
#include <QtCore/QPointer>
#include <QtCore/QCoreApplication>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

namespace MakuGram {

void CleanOldUpdateFiles() {
	const auto oldExe = cExeDir() + cExeName() + u".old"_q;
	if (QFile::exists(oldExe)) {
		QFile::remove(oldExe);
	}
	const auto updateDir = cWorkingDir() + u"tupdates/makugram"_q;
	if (QDir(updateDir).exists()) {
		QDir(updateDir).removeRecursively();
	}
}

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

struct ReleaseAsset {
	QString name;
	QString url;
	qint64 size = 0;
};

struct ReleaseInfo {
	QString tag;
	QString name;
	QString body;
	QString htmlUrl;
	ReleaseAsset asset;
};

bool ApplyUpdateAndRestart(const QString &newExePath, QString *outError = nullptr) {
	const auto currentExe = cExeDir() + cExeName();
	const auto oldExe = currentExe + u".old"_q;

	// 1. Remove previous .old if exists
	if (QFile::exists(oldExe)) {
		if (!QFile::remove(oldExe)) {
			if (outError) {
				*outError = Tr::Get(
					u"Не удалось удалить старую резервную копию MakuGram.exe.old."_q,
					u"Failed to delete old backup file MakuGram.exe.old."_q);
			}
			return false;
		}
	}

	// 2. Rename running exe to .old (allowed on Windows)
	if (!QFile::rename(currentExe, oldExe)) {
		if (outError) {
			*outError = Tr::Get(
				u"Не удалось переименовать текущий MakuGram.exe (нет прав доступа)."_q,
				u"Failed to rename current MakuGram.exe (access denied)."_q);
		}
		return false;
	}

	// 3. Move/Copy new exe to original spot
	bool placed = false;
	if (QFile::rename(newExePath, currentExe)) {
		placed = true;
	} else if (QFile::copy(newExePath, currentExe)) {
		placed = true;
	}

	if (!placed) {
		// Rollback immediately!
		QFile::rename(oldExe, currentExe);
		if (outError) {
			*outError = Tr::Get(
				u"Не удалось разместить новый MakuGram.exe. Исходный файл восстановлен."_q,
				u"Failed to place new MakuGram.exe. Original file restored."_q);
		}
		return false;
	}

	// 4. Clean update directory
	const auto updateDir = cWorkingDir() + u"tupdates/makugram"_q;
	QDir(updateDir).removeRecursively();

#ifdef Q_OS_WIN
	// 5. Schedule background retry loop to delete .old file as soon as this process exits and releases lock
	const auto nativeOld = QDir::toNativeSeparators(oldExe);
	QProcess::startDetached(
		u"powershell.exe"_q,
		{
			u"-NonInteractive"_q,
			u"-WindowStyle"_q,
			u"Hidden"_q,
			u"-Command"_q,
			QString("for ($i=0; $i -lt 30; $i++) { Start-Sleep -Seconds 1; if (!(Test-Path -LiteralPath '%1')) { break }; Remove-Item -LiteralPath '%1' -Force -ErrorAction SilentlyContinue; if (!(Test-Path -LiteralPath '%1')) { break } }").arg(nativeOld),
		});
#endif

	// 6. Trigger restart cleanly!
	cSetRestarting(true);
	cSetRestartingToSettings(false);
	Core::Quit();
	return true;
}

struct DownloadSession {
	QPointer<QNetworkReply> reply;
	std::unique_ptr<QFile> file;
	QString downloadPath;
	QString extractedExePath;
	bool isZip = false;
	bool readyToRestart = false;
};

class UpdateManager final : public QObject {
public:
	static UpdateManager &instance() {
		static UpdateManager mgr;
		return mgr;
	}

	void check(base::weak_ptr<Window::SessionController> weakController, bool silent) {
		CleanOldUpdateFiles();

		if (_inProgress) {
			if (!silent) {
				if (const auto controller = weakController.get()) {
					controller->showToast(Tr::Get(
						u"Проверка обновлений уже выполняется..."_q,
						u"Update check is already in progress..."_q));
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
					controller->showToast(Tr::Get(
						u"Не удалось проверить обновления. Проверьте соединение с интернетом."_q,
						u"Failed to check for updates. Check your internet connection."_q));
				}
				return;
			}

			const auto data = reply->readAll();
			QJsonParseError parseError;
			const auto doc = QJsonDocument::fromJson(data, &parseError);
			if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
				if (!silent && controller) {
					controller->showToast(Tr::Get(
						u"Ошибка обработки ответа от GitHub."_q,
						u"Error parsing response from GitHub."_q));
				}
				return;
			}

			const auto obj = doc.object();
			ReleaseInfo info;
			info.tag = obj.value(u"tag_name"_q).toString();
			info.name = obj.value(u"name"_q).toString();
			info.htmlUrl = obj.value(u"html_url"_q).toString();
			info.body = obj.value(u"body"_q).toString();

			// Find best asset: .exe first, then .zip
			const auto assets = obj.value(u"assets"_q).toArray();
			for (const auto &val : assets) {
				const auto a = val.toObject();
				const auto aName = a.value(u"name"_q).toString();
				if (aName.endsWith(u".exe"_q, Qt::CaseInsensitive)) {
					info.asset.name = aName;
					info.asset.url = a.value(u"browser_download_url"_q).toString();
					info.asset.size = a.value(u"size"_q).toVariant().toLongLong();
					break;
				}
			}
			if (info.asset.url.isEmpty()) {
				for (const auto &val : assets) {
					const auto a = val.toObject();
					const auto aName = a.value(u"name"_q).toString();
					if (aName.endsWith(u".zip"_q, Qt::CaseInsensitive)) {
						info.asset.name = aName;
						info.asset.url = a.value(u"browser_download_url"_q).toString();
						info.asset.size = a.value(u"size"_q).toVariant().toLongLong();
						break;
					}
				}
			}

			const auto currentVer = ParseVersion(kVersion);
			const auto latestVer = ParseVersion(info.tag);

			if (currentVer < latestVer) {
				if (controller) {
					showUpdateBox(controller, info);
				}
			} else {
				if (!silent && controller) {
					controller->showToast(Tr::Get(
						QString::fromUtf8("У вас установлена последняя версия MakuGram (v%1)").arg(kVersion),
						QString::fromUtf8("You have the latest version of MakuGram (v%1)").arg(kVersion)));
				}
			}
		});
	}

private:
	void showUpdateBox(
			not_null<Window::SessionController*> controller,
			const ReleaseInfo &release) {
		controller->show(Box([=](not_null<Ui::GenericBox*> box) {
			box->setWidth(500);
			box->setTitle(Tr::Value(
				u"Доступно обновление MakuGram!"_q,
				u"MakuGram update available!"_q));

			QString versionInfo = Tr::Get(
				u"Найдена новая версия: %1 (текущая: v%2)"_q,
				u"New version available: %1 (current: v%2)"_q
			).arg(release.tag.startsWith('v') ? release.tag : ("v" + release.tag)).arg(kVersion);

			if (!release.name.isEmpty() && release.name != release.tag) {
				versionInfo += u"\n"_q + release.name;
			}
			if (release.asset.size > 0) {
				const auto mb = double(release.asset.size) / (1024.0 * 1024.0);
				versionInfo += u"\n"_q + Tr::Get(
					QString::fromUtf8("Размер обновления: ~%1 МБ").arg(QString::number(mb, 'f', 1)),
					QString::fromUtf8("Update size: ~%1 MB").arg(QString::number(mb, 'f', 1)));
			}

			box->addRow(
				object_ptr<Ui::FlatLabel>(
					box,
					versionInfo,
					st::boxLabel),
				st::boxRowPadding);

			if (!release.body.trimmed().isEmpty()) {
				const auto cleanBody = release.body.left(350)
					+ (release.body.length() > 350 ? u"..."_q : QString());
				box->addRow(
					object_ptr<Ui::FlatLabel>(
						box,
						cleanBody,
						st::boxDividerLabel),
					st::boxRowPadding);
			}

			const auto statusLabel = box->addRow(
				object_ptr<Ui::FlatLabel>(
					box,
					QString(),
					st::boxDividerLabel),
				st::boxRowPadding);
			statusLabel->hide();

			const auto weakStatus = QPointer<Ui::FlatLabel>(statusLabel);
			const auto session = std::make_shared<DownloadSession>();

			auto weakActionBtn = std::make_shared<QPointer<Ui::RoundButton>>();

			const auto actionBtn = box->addButton(
				Tr::Value(u"Скачать и обновить"_q, u"Download & Update"_q),
				[=]() {
					const auto btn = weakActionBtn ? weakActionBtn->data() : nullptr;
					const auto status = weakStatus.data();

					// If already downloaded and extracted, restart!
					if (session->readyToRestart) {
						if (btn) btn->setEnabled(false);
						if (status) {
							status->setText(Tr::Get(
								u"Применение обновления и перезапуск..."_q,
								u"Applying update and restarting..."_q));
							status->show();
						}
						QString errorMsg;
						if (!ApplyUpdateAndRestart(session->extractedExePath, &errorMsg)) {
							if (status) {
								status->setText(errorMsg);
								status->show();
							}
							if (btn) {
								btn->setEnabled(true);
								btn->setText(Tr::Value(u"Попробовать снова"_q, u"Try again"_q));
							}
						}
						return;
					}

					// Fallback to browser if no direct download asset found
					if (release.asset.url.isEmpty()) {
						UrlClickHandler::Open(release.htmlUrl.isEmpty()
							? u"https://github.com/MarkAdderly/MakuGram/releases/latest"_q
							: release.htmlUrl);
						box->closeBox();
						return;
					}

					// Start in-app download!
					if (btn) {
						btn->setEnabled(false);
						btn->setText(Tr::Value(u"Загрузка..."_q, u"Downloading..."_q));
					}
					if (status) {
						status->setText(Tr::Get(
							u"Подключение к серверу обновлений..."_q,
							u"Connecting to update server..."_q));
						status->show();
					}

					const auto updateDir = cWorkingDir() + u"tupdates/makugram/"_q;
					QDir().mkpath(updateDir);

					session->isZip = release.asset.name.endsWith(u".zip"_q, Qt::CaseInsensitive);
					session->downloadPath = updateDir + (session->isZip ? u"update.zip"_q : u"MakuGram_new.exe"_q);

					QFile::remove(session->downloadPath);
					session->file = std::make_unique<QFile>(session->downloadPath);
					if (!session->file->open(QIODevice::WriteOnly)) {
						if (status) {
							status->setText(Tr::Get(
								u"Ошибка: не удалось создать временный файл."_q,
								u"Error: failed to create temporary file."_q));
						}
						if (btn) {
							btn->setEnabled(true);
							btn->setText(Tr::Value(u"Попробовать снова"_q, u"Try again"_q));
						}
						return;
					}

					auto request = QNetworkRequest(QUrl(release.asset.url));
					request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
					request.setRawHeader("User-Agent", "MakuGram-App");

					const auto reply = _network->get(request);
					session->reply = reply;

					connect(reply, &QNetworkReply::readyRead, this, [=]() {
						if (session->file && session->file->isOpen()) {
							session->file->write(reply->readAll());
						}
					});

					connect(reply, &QNetworkReply::downloadProgress, this, [=](qint64 received, qint64 total) {
						if (const auto lbl = weakStatus.data()) {
							if (total > 0) {
								const auto mbRec = double(received) / (1024.0 * 1024.0);
								const auto mbTot = double(total) / (1024.0 * 1024.0);
								const auto pct = int((received * 100) / total);
								lbl->setText(Tr::Get(
									QString::fromUtf8("Загрузка: %1 / %2 МБ (%3%)")
										.arg(QString::number(mbRec, 'f', 1))
										.arg(QString::number(mbTot, 'f', 1))
										.arg(pct),
									QString::fromUtf8("Downloading: %1 / %2 MB (%3%)")
										.arg(QString::number(mbRec, 'f', 1))
										.arg(QString::number(mbTot, 'f', 1))
										.arg(pct)));
							} else {
								const auto mbRec = double(received) / (1024.0 * 1024.0);
								lbl->setText(Tr::Get(
									QString::fromUtf8("Загружено: %1 МБ...").arg(QString::number(mbRec, 'f', 1)),
									QString::fromUtf8("Downloaded: %1 MB...").arg(QString::number(mbRec, 'f', 1))));
							}
						}
					});

					connect(reply, &QNetworkReply::finished, this, [=]() {
						reply->deleteLater();
						if (session->file && session->file->isOpen()) {
							session->file->flush();
							session->file->close();
						}

						const auto b = weakActionBtn ? weakActionBtn->data() : nullptr;
						const auto lbl = weakStatus.data();

						if (reply->error() != QNetworkReply::NoError) {
							if (reply->error() == QNetworkReply::OperationCanceledError) {
								QFile::remove(session->downloadPath);
								return;
							}
							QFile::remove(session->downloadPath);
							if (lbl) {
								lbl->setText(Tr::Get(
									QString::fromUtf8("Ошибка загрузки: %1").arg(reply->errorString()),
									QString::fromUtf8("Download error: %1").arg(reply->errorString())));
							}
							if (b) {
								b->setEnabled(true);
								b->setText(Tr::Value(u"Попробовать снова"_q, u"Try again"_q));
							}
							return;
						}

						const auto fi = QFileInfo(session->downloadPath);
						if (!fi.exists() || fi.size() < 10 * 1024 * 1024) {
							QFile::remove(session->downloadPath);
							if (lbl) {
								lbl->setText(Tr::Get(
									u"Ошибка: загруженный файл повреждён или не полон."_q,
									u"Error: downloaded file is damaged or incomplete."_q));
							}
							if (b) {
								b->setEnabled(true);
								b->setText(Tr::Value(u"Попробовать снова"_q, u"Try again"_q));
							}
							return;
						}

						// Unpack if zip archive
						QString newExePath;
						if (session->isZip) {
							if (lbl) {
								lbl->setText(Tr::Get(
									u"Распаковка обновления..."_q,
									u"Extracting update..."_q));
							}
							QCoreApplication::processEvents();

							const auto extractDir = updateDir + u"extracted/"_q;
							QDir(extractDir).removeRecursively();
							QDir().mkpath(extractDir);

							bool extracted = false;
							{
								QProcess tar;
								tar.start(u"tar"_q, { u"-xf"_q, session->downloadPath, u"-C"_q, extractDir });
								if (tar.waitForFinished(60000) && tar.exitCode() == 0) {
									extracted = true;
								}
							}
							if (!extracted) {
								QProcess ps;
								ps.start(u"powershell"_q, {
									u"-NoProfile"_q,
									u"-NonInteractive"_q,
									u"-Command"_q,
									QString::fromUtf8("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
										.arg(session->downloadPath)
										.arg(extractDir)
								});
								if (ps.waitForFinished(60000) && ps.exitCode() == 0) {
									extracted = true;
								}
							}

							QFile::remove(session->downloadPath);

							if (!extracted) {
								if (lbl) {
									lbl->setText(Tr::Get(
										u"Ошибка распаковки архива обновления."_q,
										u"Failed to extract update archive."_q));
								}
								if (b) {
									b->setEnabled(true);
									b->setText(Tr::Value(u"Попробовать снова"_q, u"Try again"_q));
								}
								return;
							}

							// Search for MakuGram.exe or any .exe
							QDirIterator it(extractDir, { u"*.exe"_q }, QDir::Files, QDirIterator::Subdirectories);
							while (it.hasNext()) {
								const auto found = it.next();
								if (found.endsWith(u"MakuGram.exe"_q, Qt::CaseInsensitive)) {
									newExePath = found;
									break;
								} else if (newExePath.isEmpty()) {
									newExePath = found;
								}
							}

							if (newExePath.isEmpty() || QFileInfo(newExePath).size() < 10 * 1024 * 1024) {
								if (lbl) {
									lbl->setText(Tr::Get(
										u"Ошибка: MakuGram.exe не найден в распакованном архиве."_q,
										u"Error: MakuGram.exe not found in extracted archive."_q));
								}
								if (b) {
									b->setEnabled(true);
									b->setText(Tr::Value(u"Попробовать снова"_q, u"Try again"_q));
								}
								return;
							}
						} else {
							newExePath = session->downloadPath;
						}

						session->extractedExePath = newExePath;
						session->readyToRestart = true;

						if (lbl) {
							lbl->setText(Tr::Get(
								u"Обновление загружено и готово к установке!\nНажмите кнопку ниже для перезапуска MakuGram."_q,
								u"Update downloaded and ready to install!\nClick the button below to restart MakuGram."_q));
						}
						if (b) {
							b->setEnabled(true);
							b->setText(Tr::Value(u"Перезапустить и обновить"_q, u"Restart and Update"_q));
						}
					});
				});

			*weakActionBtn = actionBtn;

			box->addButton(tr::lng_cancel(), [=] {
				if (session->reply) {
					session->reply->abort();
				}
				box->closeBox();
			});

			box->addLeftButton(Tr::Value(u"Открыть в GitHub"_q, u"Open on GitHub"_q), [=] {
				UrlClickHandler::Open(release.htmlUrl.isEmpty()
					? u"https://github.com/MarkAdderly/MakuGram/releases/latest"_q
					: release.htmlUrl);
			});

			box->lifetime().add([=] {
				if (session->reply) {
					session->reply->abort();
				}
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
