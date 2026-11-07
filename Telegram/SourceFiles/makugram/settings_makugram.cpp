/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/settings_makugram.h"

#include "makugram/makugram_settings.h"
#include "base/event_filter.h"
#include "core/click_handler_types.h"
#include "lang/lang_keys.h"
#include "settings/sections/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "ui/basic_click_handlers.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {
namespace {

using namespace Builder;

constexpr auto kBoxContentWidth = 540;
constexpr auto kBoxMaxHeight = 660;

[[nodiscard]] [[maybe_unused]] int CheckboxTextSkip() {
	return st::defaultBoxCheckbox.checkPosition.x()
		+ st::defaultCheck.diameter
		+ st::defaultBoxCheckbox.textPosition.x();
}

[[nodiscard]] const style::RoundButton &TelegramButtonStyle() {
	static auto result = [] {
		auto st = st::defaultActiveButton;
		st.height = 46;
		st.textTop = 13;
		st.style = st::semiboldTextStyle;
		return st;
	}();
	return result;
}

[[nodiscard]] const style::RoundButton &YouTubeButtonStyle() {
	static auto ytBg = style::complex_color([] { return QColor(230, 33, 23); });
	static auto ytBgOver = style::complex_color([] { return QColor(200, 25, 16); });
	static auto ripple = style::complex_color([] { return QColor(255, 255, 255, 50); });
	static auto result = [] {
		auto st = st::defaultActiveButton;
		st.height = 46;
		st.textTop = 13;
		st.style = st::semiboldTextStyle;
		st.textFg = st::activeButtonFg;
		st.textFgOver = st::activeButtonFg;
		st.textBg = ytBg.color();
		st.textBgOver = ytBgOver.color();
		st.ripple.color = ripple.color();
		return st;
	}();
	return result;
}

[[nodiscard]] const style::RoundButton &BoostyButtonStyle() {
	static auto boostyBg = style::complex_color([] { return QColor(241, 95, 44); });
	static auto boostyBgOver = style::complex_color([] { return QColor(220, 80, 32); });
	static auto ripple = style::complex_color([] { return QColor(255, 255, 255, 50); });
	static auto result = [] {
		auto st = st::defaultActiveButton;
		st.height = 46;
		st.textTop = 13;
		st.style = st::semiboldTextStyle;
		st.textFg = st::activeButtonFg;
		st.textFgOver = st::activeButtonFg;
		st.textBg = boostyBg.color();
		st.textBgOver = boostyBgOver.color();
		st.ripple.color = ripple.color();
		return st;
	}();
	return result;
}

class MakuGramSection : public Section<MakuGramSection> {
public:
	MakuGramSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();
};

void AddCheckboxRow(
		not_null<Ui::GenericBox*> box,
		rpl::producer<QString> title,
		bool checked,
		Fn<void(bool)> onChange,
		rpl::producer<QString> about = nullptr,
		int bottomSkip = -1) {
	const auto cb = box->addRow(
		object_ptr<Ui::Checkbox>(
			box,
			std::move(title),
			checked,
			st::defaultBoxCheckbox),
		style::margins(
			st::boxRowPadding.left(),
			4,
			st::boxRowPadding.right(),
			about ? 0 : (bottomSkip >= 0 ? bottomSkip : st::boxMediumSkip)),
		style::al_justify);
	cb->setCursor(style::cur_pointer);
	cb->setAllowTextLines(0);
	cb->setCheckAlignment(style::al_topleft);
	cb->checkedChanges(
	) | rpl::on_next([=](bool checked) {
		onChange(checked);
	}, cb->lifetime());

	if (about) {
		const auto label = box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				std::move(about),
				st::boxDividerLabel),
			style::margins(
				st::boxRowPadding.left(),
				9,
				st::boxRowPadding.right(),
				bottomSkip >= 0 ? bottomSkip : st::boxMediumSkip),
			style::al_justify);
		label->setCursor(style::cur_pointer);
		base::install_event_filter(label, [=](not_null<QEvent*> e) {
			if (e->type() == QEvent::MouseButtonPress) {
				cb->setChecked(!cb->checked());
				return base::EventFilterResult::Cancel;
			}
			return base::EventFilterResult::Continue;
		});
	}
}

void ShowSpamFilterBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(QString::fromUtf8("Локальный спам-фильтр"));

		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				QString::fromUtf8(
					"Введите скрываемые слова через запятую или с новой строки.\n"
					"Поддерживается звёздочка * (например: кошк* скроет \"кошка\", \"кошку\", \"кошки\" и т.д.):"),
				st::boxLabel),
			st::boxRowPadding);

		const auto field = box->addRow(
			object_ptr<Ui::InputField>(
				box,
				st::supportInfoField,
				Ui::InputField::Mode::MultiLine,
				rpl::single(QString::fromUtf8("Введите слова или шаблоны (*)...")),
				MakuGram::Settings::instance().spamKeywords()),
			st::boxRowPadding);
		field->setFocus();
		field->setMinHeight(130);

		const auto excludeSearch = box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				QString::fromUtf8("Исключить скрываемые слова из поиска"),
				MakuGram::Settings::instance().filterSpamInSearch(),
				st::defaultBoxCheckbox),
			style::margins(
				st::boxRowPadding.left(),
				st::boxMediumSkip,
				st::boxRowPadding.right(),
				2),
			style::al_justify);
		excludeSearch->setCursor(style::cur_pointer);
		excludeSearch->setAllowTextLines(0);
		excludeSearch->setCheckAlignment(style::al_topleft);

		const auto excludeAbout = box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				QString::fromUtf8("Сообщения, содержащие скрываемые слова, не будут отображаться в результатах глобального поиска и поиска по чату."),
				st::boxDividerLabel),
			style::margins(
				st::boxRowPadding.left(),
				9,
				st::boxRowPadding.right(),
				st::boxMediumSkip),
			style::al_justify);
		excludeAbout->setCursor(style::cur_pointer);
		base::install_event_filter(excludeAbout, [=](not_null<QEvent*> e) {
			if (e->type() == QEvent::MouseButtonPress) {
				excludeSearch->setChecked(!excludeSearch->checked());
				return base::EventFilterResult::Cancel;
			}
			return base::EventFilterResult::Continue;
		});

		box->addButton(tr::lng_settings_save(), [=] {
			MakuGram::Settings::instance().setSpamKeywords(field->getLastText());
			MakuGram::Settings::instance().setFilterSpamInSearch(excludeSearch->checked());
			box->closeBox();
			controller->showToast(QString::fromUtf8("Настройки сохранены"));
		});
		box->addButton(tr::lng_cancel(), [=] {
			box->closeBox();
		});
	}));
}

void ShowMessagesBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(rpl::single(QString::fromUtf8("Сообщения")));

		AddCheckboxRow(
			box,
			tr::lng_settings_makugram_exact_time(),
			MakuGram::Settings::instance().showExactTime(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowExactTime(checked);
			},
			tr::lng_settings_makugram_exact_time_about());

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Кнопка \"Только непрочитанные\" над чатами")),
			MakuGram::Settings::instance().showUnreadFilterButton(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowUnreadFilterButton(checked);
			},
			rpl::single(QString::fromUtf8("Отображает кнопку над списком чатов для быстрого показа только непрочитанных диалогов в один клик.")));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Переслать без автора")),
			MakuGram::Settings::instance().forwardWithoutAuthor(),
			[=](bool checked) {
				MakuGram::Settings::instance().setForwardWithoutAuthor(checked);
			},
			rpl::single(QString::fromUtf8("Добавляет пункт \"Переслать без автора\" в контекстное меню сообщений.")));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Увеличить лимит выделяемых сообщений с 100 до 1000")),
			MakuGram::Settings::instance().increaseMessageSelectLimit(),
			[=](bool checked) {
				MakuGram::Settings::instance().setIncreaseMessageSelectLimit(checked);
			},
			rpl::single(QString::fromUtf8("Позволяет одновременно выделять до 1000 сообщений в чатах и медиа для их пересылки или удаления.")));

		AddCheckboxRow(
			box,
			tr::lng_settings_makugram_spam_filter(),
			MakuGram::Settings::instance().spamFilterEnabled(),
			[=](bool checked) {
				MakuGram::Settings::instance().setSpamFilterEnabled(checked);
			},
			tr::lng_settings_makugram_spam_filter_about(),
			 0);

		const auto spamSetupBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				tr::lng_settings_makugram_spam_setup(),
				st::defaultBoxButton),
			style::margins(
				st::boxRowPadding.left(),
				9,
				st::boxRowPadding.right(),
				st::boxMediumSkip));
		spamSetupBtn->setClickedCallback([=] {
			ShowSpamFilterBox(controller);
		});

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowPrivacyBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(rpl::single(QString::fromUtf8("Приватность и безопасность")));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Не передавать статус набора текста")),
			MakuGram::Settings::instance().hideTypingStatus(),
			[=](bool checked) {
				MakuGram::Settings::instance().setHideTypingStatus(checked);
			},
			rpl::single(QString::fromUtf8("Собеседники не будут видеть, когда вы печатаете текст или записываете голосовое/видеосообщение.")));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Разрешить копирование и сохранение контента")),
			MakuGram::Settings::instance().bypassRestrictedContent(),
			[=](bool checked) {
				MakuGram::Settings::instance().setBypassRestrictedContent(checked);
			},
			rpl::single(QString::fromUtf8("Снимает ограничения на сохранение медиа, копирование текста и скриншоты в защищённых каналах и группах.")));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Массовое удаление сообщений за период")),
			MakuGram::Settings::instance().quickMassDeleteMyMessages(),
			[=](bool checked) {
				MakuGram::Settings::instance().setQuickMassDeleteMyMessages(checked);
			},
			rpl::single(QString::fromUtf8("Добавляет в меню диалога пункт для быстрого удаления сообщений за сегодня, неделю, месяц или всё время.")));

		AddCheckboxRow(
			box,
			tr::lng_settings_makugram_hide_blocked(),
			MakuGram::Settings::instance().hideMessagesFromBlocked(),
			[=](bool checked) {
				MakuGram::Settings::instance().setHideMessagesFromBlocked(checked);
			},
			tr::lng_settings_makugram_hide_blocked_about());

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Спрятать номер телефона из настроек")),
			MakuGram::Settings::instance().hidePhoneNumberInSettings(),
			[=](bool checked) {
				MakuGram::Settings::instance().setHidePhoneNumberInSettings(checked);
			},
			rpl::single(QString::fromUtf8("Скрывает номер телефона в шапке настроек и в меню редактирования профиля для защиты от случайной демонстрации.")));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowModerationBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(rpl::single(QString::fromUtf8("Модерация чатов")));

		AddCheckboxRow(
			box,
			tr::lng_settings_makugram_quick_ban(),
			MakuGram::Settings::instance().quickBanInRequests(),
			[=](bool checked) {
				MakuGram::Settings::instance().setQuickBanInRequests(checked);
			},
			tr::lng_settings_makugram_quick_ban_about());

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Поиск сообщений пользователя в чате")),
			MakuGram::Settings::instance().searchUserMessagesInChat(),
			[=](bool checked) {
				MakuGram::Settings::instance().setSearchUserMessagesInChat(checked);
			},
			rpl::single(QString::fromUtf8("Добавляет пункт в контекстное меню сообщения для быстрого поиска всех сообщений автора в текущей группе.")));

		AddCheckboxRow(
			box,
			tr::lng_settings_makugram_newbie_badge(),
			MakuGram::Settings::instance().showNewbieBadge(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowNewbieBadge(checked);
			},
			tr::lng_settings_makugram_newbie_badge_about());

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowProfileBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(tr::lng_settings_makugram_section_profile());

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Показывать ID и дату регистрации аккаунта")),
			MakuGram::Settings::instance().showProfileDetails(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowProfileDetails(checked);
			},
			rpl::single(QString::fromUtf8("Отображать числовой ID (с копированием в буфер по клику) и расчетный период регистрации в профиле пользователя.")));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Отключить сторис")),
			MakuGram::Settings::instance().disableStories(),
			[=](bool checked) {
				MakuGram::Settings::instance().setDisableStories(checked);
			},
			rpl::single(QString::fromUtf8("Полностью скрывает истории пользователей и каналов: ободки на аватарках, верхнюю панель историй, раздел в профиле и в архиве.")));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowMediaBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(tr::lng_settings_makugram_section_media());

		AddCheckboxRow(
			box,
			tr::lng_settings_makugram_always_zoom(),
			MakuGram::Settings::instance().alwaysZoomMediaToScreen(),
			[=](bool checked) {
				MakuGram::Settings::instance().setAlwaysZoomMediaToScreen(checked);
			},
			tr::lng_settings_makugram_always_zoom_about());

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Показывать разрешение")),
			MakuGram::Settings::instance().showMediaResolution(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowMediaResolution(checked);
			},
			rpl::single(QString::fromUtf8("Отображает разрешение изображения или видео в полноэкранном режиме просмотра медиа.")));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Добавить выделение медиа в полноэкранном режиме")),
			MakuGram::Settings::instance().fullscreenMediaSelection(),
			[=](bool checked) {
				MakuGram::Settings::instance().setFullscreenMediaSelection(checked);
			},
			rpl::single(QString::fromUtf8("Позволяет выделять и пересылать медиа при полноэкранном просмотре, сохраняя выделение при возврате в чат.")));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Расширить окно отправки медиа")),
			MakuGram::Settings::instance().expandMediaSendDialog(),
			[=](bool checked) {
				MakuGram::Settings::instance().setExpandMediaSendDialog(checked);
			},
			rpl::single(QString::fromUtf8("Увеличивает ширину окна предпросмотра при отправке фото и видео, а также расширяет поле ввода подписи по высоте для комфортного написания длинного текста.")));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowAppearanceBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(QString::fromUtf8("Внешний вид"));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Компактный список чатов")),
			MakuGram::Settings::instance().compactChatList(),
			[=](bool checked) {
				MakuGram::Settings::instance().setCompactChatList(checked);
			},
			rpl::single(QString::fromUtf8("Скрывает строку предпросмотра последнего сообщения и уменьшает высоту строк чатов, умещая в два раза больше диалогов на экране.")));

		AddCheckboxRow(
			box,
			rpl::single(QString::fromUtf8("Отключить анимацию реакций и конфетти")),
			MakuGram::Settings::instance().disableReactionAnimations(),
			[=](bool checked) {
				MakuGram::Settings::instance().setDisableReactionAnimations(checked);
			},
			rpl::single(QString::fromUtf8("Отключает летающие большие эмодзи, эффекты и конфетти при установке реакций для максимальной плавности.")));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowAboutBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(QString::fromUtf8("О клиенте MakuGram"));

		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				tr::lng_settings_makugram_about(),
				st::boxLabel),
			st::boxRowPadding);

		box->addSkip(st::boxMediumSkip);

		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				QString::fromUtf8("Автор - Марк Аддерли"),
				st::boxLabel),
			st::boxRowPadding);

		box->addSkip(st::boxMediumSkip);

		const auto buttonWidth = kBoxContentWidth
			- st::boxRowPadding.left()
			- st::boxRowPadding.right();

		const auto tgBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				rpl::single(QString::fromUtf8("Telegram канал автора")),
				TelegramButtonStyle()),
			style::margins(
				st::boxRowPadding.left(),
				4,
				st::boxRowPadding.right(),
				4));
		tgBtn->setFullWidth(buttonWidth);
		tgBtn->setFullRadius(true);
		tgBtn->setClickedCallback([=] {
			UrlClickHandler::Open(
				u"https://t.me/adderly324"_q,
				QVariant::fromValue(ClickHandlerContext{
					.sessionWindow = base::make_weak(controller),
				}));
		});

		const auto ytBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				rpl::single(QString::fromUtf8("YouTube канал автора")),
				YouTubeButtonStyle()),
			style::margins(
				st::boxRowPadding.left(),
				4,
				st::boxRowPadding.right(),
				4));
		ytBtn->setFullWidth(buttonWidth);
		ytBtn->setFullRadius(true);
		ytBtn->setClickedCallback([=] {
			UrlClickHandler::Open(u"https://youtube.com/@makuadarii"_q);
		});

		const auto boostyBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				rpl::single(QString::fromUtf8("Поддержать разработку")),
				BoostyButtonStyle()),
			style::margins(
				st::boxRowPadding.left(),
				4,
				st::boxRowPadding.right(),
				8));
		boostyBtn->setFullWidth(buttonWidth);
		boostyBtn->setFullRadius(true);
		boostyBtn->setClickedCallback([=] {
			UrlClickHandler::Open(u"https://boosty.to/adderly"_q);
		});

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void BuildMakuGramSection(SectionBuilder &builder) {
	const auto controller = builder.controller();
	if (!controller) {
		return;
	}

	builder.addSubsectionTitle(rpl::single(QString::fromUtf8("Категории настроек")));

	builder.addButton({
		.title = rpl::single(QString::fromUtf8("Сообщения")),
		.icon = { &st::menuIconChats },
		.onClick = [=] { ShowMessagesBox(controller); },
		.keywords = { u"time"_q, u"seconds"_q, u"unread"_q, u"spam"_q, u"words"_q, u"время"_q, u"секунды"_q, u"непрочитанные"_q, u"спам"_q, u"слова"_q, u"сообщения"_q },
	});

	builder.addButton({
		.title = rpl::single(QString::fromUtf8("Приватность и безопасность")),
		.icon = { &st::menuIconLock },
		.onClick = [=] { ShowPrivacyBox(controller); },
		.keywords = { u"privacy"_q, u"typing"_q, u"copy"_q, u"delete"_q, u"phone"_q, u"blocked"_q, u"приватность"_q, u"тайпинг"_q, u"копирование"_q, u"удаление"_q, u"телефон"_q, u"номер"_q, u"блок"_q },
	});

	builder.addButton({
		.title = rpl::single(QString::fromUtf8("Модерация чатов")),
		.icon = { &st::menuIconAdmin },
		.onClick = [=] { ShowModerationBox(controller); },
		.keywords = { u"ban"_q, u"badge"_q, u"requests"_q, u"search"_q, u"бан"_q, u"новичок"_q, u"заявки"_q, u"модерация"_q, u"поиск"_q },
	});

	builder.addButton({
		.title = tr::lng_settings_makugram_section_profile(),
		.icon = { &st::menuIconProfile },
		.onClick = [=] { ShowProfileBox(controller); },
		.keywords = { u"id"_q, u"profile"_q, u"registration"_q, u"stories"_q, u"профиль"_q, u"возраст"_q, u"регистрация"_q, u"сторис"_q, u"истории"_q },
	});

	builder.addButton({
		.title = rpl::single(QString::fromUtf8("Внешний вид")),
		.icon = { &st::menuIconChangeColors },
		.onClick = [=] { ShowAppearanceBox(controller); },
		.keywords = { u"appearance"_q, u"compact"_q, u"reactions"_q, u"вид"_q, u"компактный"_q, u"реакции"_q },
	});

	builder.addButton({
		.title = tr::lng_settings_makugram_section_media(),
		.icon = { &st::menuIconPhoto },
		.onClick = [=] { ShowMediaBox(controller); },
		.keywords = { u"media"_q, u"zoom"_q, u"медиа"_q, u"масштаб"_q, u"экран"_q },
	});

	builder.addButton({
		.title = rpl::single(QString::fromUtf8("О клиенте")),
		.icon = { &st::menuIconInfo },
		.onClick = [=] { ShowAboutBox(controller); },
		.keywords = { u"about"_q, u"makugram"_q, u"версия"_q, u"информация"_q },
	});

	builder.addDivider();
	builder.addDividerText(rpl::single(QString::fromUtf8("Нажмите на категорию для настройки параметров MakuGram.")));
}

const auto kMeta = BuildHelper({
	.id = MakuGramSection::Id(),
	.parentId = MainId(),
	.title = &tr::lng_settings_makugram,
	.icon = &st::menuIconManage,
}, [](SectionBuilder &builder) {
	BuildMakuGramSection(builder);
});

MakuGramSection::MakuGramSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> MakuGramSection::title() {
	return tr::lng_settings_makugram();
}

void MakuGramSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

} // namespace

Type MakuGramId() {
	return MakuGramSection::Id();
}

} // namespace Settings
