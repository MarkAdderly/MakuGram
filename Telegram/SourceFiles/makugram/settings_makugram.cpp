/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/settings_makugram.h"
#include "makugram/currency_converter.h"
#include "makugram/makugram_updater.h"
#include "makugram/makugram_lang.h"
#include "boxes/connection_box.h"

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
using namespace MakuGram;

constexpr auto kBoxContentWidth = 540;
constexpr auto kBoxMaxHeight = 690;

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
		box->setTitle(Tr::Value(
			QString::fromUtf8("Локальный спам-фильтр"),
			u"Local Spam Filter"_q));

		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				Tr::Value(
					QString::fromUtf8(
						"Введите скрываемые слова через запятую или с новой строки.\n"
						"Поддерживается звёздочка * (например: кошк* скроет \"кошка\", \"кошку\", \"кошки\" и т.д.):"),
					u"Enter filtered words comma-separated or one per line.\n"
					"Asterisk * wildcard is supported (e.g.: cat* will hide \"cat\", \"cats\", etc.):"_q),
				st::boxLabel),
			st::boxRowPadding);

		const auto field = box->addRow(
			object_ptr<Ui::InputField>(
				box,
				st::supportInfoField,
				Ui::InputField::Mode::MultiLine,
				Tr::Value(
					QString::fromUtf8("Введите слова или шаблоны (*)..."),
					u"Enter words or patterns (*)..."_q),
				MakuGram::Settings::instance().spamKeywords()),
			st::boxRowPadding);
		field->setFocus();
		field->setMinHeight(130);

		const auto excludeSearch = box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				Tr::Value(
					QString::fromUtf8("Исключить скрываемые слова из поиска"),
					u"Exclude hidden words from search"_q),
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
				Tr::Value(
					QString::fromUtf8("Сообщения, содержащие скрываемые слова, не будут отображаться в результатах глобального поиска и поиска по чату."),
					u"Messages containing hidden words will not appear in global and chat search results."_q),
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
			controller->showToast(Tr::Get(
				QString::fromUtf8("Настройки сохранены"),
				u"Settings saved"_q));
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
		box->setTitle(Tr::Value(
			QString::fromUtf8("Сообщения"),
			u"Messages"_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Кнопка \"Только непрочитанные\" над чатами"),
				u"\"Unread only\" button above chats"_q),
			MakuGram::Settings::instance().showUnreadFilterButton(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowUnreadFilterButton(checked);
			},
			Tr::Value(
				QString::fromUtf8("Отображает кнопку над списком чатов для быстрого показа только непрочитанных диалогов в один клик."),
				u"Displays a button above the chat list to quickly show only unread dialogs in one click."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Переслать без автора"),
				u"Forward without author"_q),
			MakuGram::Settings::instance().forwardWithoutAuthor(),
			[=](bool checked) {
				MakuGram::Settings::instance().setForwardWithoutAuthor(checked);
			},
			Tr::Value(
				QString::fromUtf8("Добавляет пункт \"Переслать без автора\" в контекстное меню сообщений."),
				u"Adds a \"Forward without author\" item to the message context menu."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Увеличить лимит выделяемых сообщений с 100 до 1000"),
				u"Increase message selection limit from 100 to 1000"_q),
			MakuGram::Settings::instance().increaseMessageSelectLimit(),
			[=](bool checked) {
				MakuGram::Settings::instance().setIncreaseMessageSelectLimit(checked);
			},
			Tr::Value(
				QString::fromUtf8("Позволяет одновременно выделять до 1000 сообщений в чатах и медиа для их пересылки или удаления."),
				u"Allows selecting up to 1000 messages in chats and media at once for forwarding or deleting."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Точное время сообщений (с секундами)"),
				u"Exact message time (with seconds)"_q),
			MakuGram::Settings::instance().showExactTime(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowExactTime(checked);
			},
			Tr::Value(
				QString::fromUtf8("Отображать время отправленных сообщений с точностью до секунды (ЧЧ:ММ:СС)."),
				u"Display message sent time with second precision (HH:MM:SS)."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Локальный спам-фильтр по ключевым словам"),
				u"Local keyword spam filter"_q),
			MakuGram::Settings::instance().spamFilterEnabled(),
			[=](bool checked) {
				MakuGram::Settings::instance().setSpamFilterEnabled(checked);
			},
			Tr::Value(
				QString::fromUtf8("Скрывать сообщения со скрываемыми словами в чатах и в личных сообщениях."),
				u"Hide messages containing filtered words in chats and direct messages."_q),
			0);

		const auto spamSetupBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				Tr::Value(
					QString::fromUtf8("Настроить скрываемые слова..."),
					u"Configure hidden words..."_q),
				st::defaultBoxButton),
			style::margins(
				st::boxRowPadding.left(),
				9,
				st::boxRowPadding.right(),
				st::boxMediumSkip));
		spamSetupBtn->setClickedCallback([=] {
			ShowSpamFilterBox(controller);
		});

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Конвертер валют"),
				u"Currency converter"_q),
			MakuGram::Settings::instance().currencyConverter(),
			[=](bool checked) {
				MakuGram::Settings::instance().setCurrencyConverter(checked);
			},
			Tr::Value(
				QString::fromUtf8("Автоматически подсвечивает суммы в сообщениях (например, \"15 рублей\", \"10M IDR\", \"100$\") и показывает их конвертацию при наведении или клике."),
				u"Automatically detects amounts in messages (e.g. \"15 rubles\", \"10M IDR\", \"100$\") and shows conversion on hover or click."_q),
			0);

		const auto curSetupBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				Tr::Value(
					QString::fromUtf8("Выбрать целевые валюты..."),
					u"Select target currencies..."_q),
				st::defaultBoxButton),
			style::margins(
				st::boxRowPadding.left(),
				9,
				st::boxRowPadding.right(),
				st::boxMediumSkip));
		curSetupBtn->setClickedCallback([=] {
			MakuGram::ShowCurrencyTargetsBox(controller);
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
		box->setTitle(Tr::Value(
			QString::fromUtf8("Приватность и безопасность"),
			u"Privacy & Security"_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Не передавать статус набора текста"),
				u"Do not send typing status"_q),
			MakuGram::Settings::instance().hideTypingStatus(),
			[=](bool checked) {
				MakuGram::Settings::instance().setHideTypingStatus(checked);
			},
			Tr::Value(
				QString::fromUtf8("Собеседники не будут видеть, когда вы печатаете текст или записываете голосовое/видеосообщение."),
				u"Others will not see when you are typing or recording voice/video messages."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Разрешить копирование и сохранение контента"),
				u"Allow copying and saving protected content"_q),
			MakuGram::Settings::instance().bypassRestrictedContent(),
			[=](bool checked) {
				MakuGram::Settings::instance().setBypassRestrictedContent(checked);
			},
			Tr::Value(
				QString::fromUtf8("Снимает ограничения на пересылку, сохранение медиа, копирование текста и скриншоты в защищённых каналах и группах (при пересылке защищённых сообщений они копируются с сохранением текста, медиа и указанием автора в конце)."),
				u"Removes restrictions on forwarding, saving media, copying text, and screenshots in protected channels and groups."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Массовое удаление сообщений за период"),
				u"Mass delete messages by period"_q),
			MakuGram::Settings::instance().quickMassDeleteMyMessages(),
			[=](bool checked) {
				MakuGram::Settings::instance().setQuickMassDeleteMyMessages(checked);
			},
			Tr::Value(
				QString::fromUtf8("Добавляет в меню диалога пункт для быстрого удаления сообщений за сегодня, неделю, месяц или всё время."),
				u"Adds an option to chat menu for quickly deleting messages for today, week, month, or all time."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Скрывать сообщения от заблокированных"),
				u"Hide messages from blocked users"_q),
			MakuGram::Settings::instance().hideMessagesFromBlocked(),
			[=](bool checked) {
				MakuGram::Settings::instance().setHideMessagesFromBlocked(checked);
			},
			Tr::Value(
				QString::fromUtf8("Не отображать сообщения заблокированных пользователей в общих чатах и группах."),
				u"Do not display messages from blocked users in public and group chats."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Спрятать номер телефона из профиля и настроек"),
				u"Hide phone number in profile and settings"_q),
			MakuGram::Settings::instance().hidePhoneNumberInSettings(),
			[=](bool checked) {
				MakuGram::Settings::instance().setHidePhoneNumberInSettings(checked);
			},
			Tr::Value(
				QString::fromUtf8("Скрывает номер телефона в вашем профиле, в шапке настроек и в меню редактирования для защиты от случайной демонстрации."),
				u"Hides your phone number in profile, settings header, and edit menu to protect against accidental exposure."_q));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowModerationBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(Tr::Value(
			QString::fromUtf8("Модерация чатов"),
			u"Chat Moderation"_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Поиск сообщений пользователя в чате"),
				u"Search user messages in chat"_q),
			MakuGram::Settings::instance().searchUserMessagesInChat(),
			[=](bool checked) {
				MakuGram::Settings::instance().setSearchUserMessagesInChat(checked);
			},
			Tr::Value(
				QString::fromUtf8("Добавляет пункт в контекстное меню сообщения для быстрого поиска всех сообщений автора в текущей группе."),
				u"Adds an item to message context menu to quickly find all messages from this author in current group."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Кнопка \"Забанить\" в заявках"),
				u"\"Ban\" button in join requests"_q),
			MakuGram::Settings::instance().quickBanInRequests(),
			[=](bool checked) {
				MakuGram::Settings::instance().setQuickBanInRequests(checked);
			},
			Tr::Value(
				QString::fromUtf8("Добавляет кнопку быстрого бана прямо рядом с кнопками одобрения и отклонения заявок на вступление."),
				u"Adds a quick ban button directly next to approve and decline join requests."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Бейдж \"Новичок\" в чатах"),
				u"\"Newbie\" badge in chats"_q),
			MakuGram::Settings::instance().showNewbieBadge(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowNewbieBadge(checked);
			},
			Tr::Value(
				QString::fromUtf8("Отображать бейдж \"Новичок\" рядом с именем участников, состоящих в чате меньше 3 месяцев."),
				u"Show \"Newbie\" badge next to members who joined the chat less than 3 months ago."_q));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowProfileBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(Tr::Value(
			QString::fromUtf8("Профиль и информация"),
			u"Profile & Info"_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Показывать ID и дату регистрации аккаунта"),
				u"Show account ID and registration date"_q),
			MakuGram::Settings::instance().showProfileDetails(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowProfileDetails(checked);
			},
			Tr::Value(
				QString::fromUtf8("Отображать числовой ID (с копированием в буфер по клику) и расчетный период регистрации в профиле пользователя."),
				u"Display numeric ID (clickable to copy) and estimated registration period in user profile."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Отключить сторис"),
				u"Disable Stories"_q),
			MakuGram::Settings::instance().disableStories(),
			[=](bool checked) {
				MakuGram::Settings::instance().setDisableStories(checked);
			},
			Tr::Value(
				QString::fromUtf8("Полностью скрывает истории пользователей и каналов: ободки на аватарках, верхнюю панель историй, раздел в профиле и в архиве."),
				u"Completely hides user and channel stories: rings on avatars, top stories bar, profile and archive sections."_q));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowMediaBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(Tr::Value(
			QString::fromUtf8("Медиа"),
			u"Media"_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Всегда открывать медиа по размеру экрана"),
				u"Always open media fit to window"_q),
			MakuGram::Settings::instance().alwaysZoomMediaToScreen(),
			[=](bool checked) {
				MakuGram::Settings::instance().setAlwaysZoomMediaToScreen(checked);
			},
			Tr::Value(
				QString::fromUtf8("Автоматически растягивать изображения и медиафайлы до границ окна по соотношению сторон при открытии (аналогично нажатию на колёсико мыши)."),
				u"Automatically fit images and media to window borders on open (like clicking mouse wheel)."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Регулировка громкости колесиком мыши"),
				u"Mouse wheel volume control"_q),
			MakuGram::Settings::instance().videoVolumeWheel(),
			[=](bool checked) {
				MakuGram::Settings::instance().setVideoVolumeWheel(checked);
			},
			Tr::Value(
				QString::fromUtf8("При просмотре видео в полноэкранном режиме колесико мыши регулирует громкость звука вместо переключения на следующее или предыдущее медиа."),
				u"When viewing video in fullscreen, mouse wheel adjusts volume instead of switching to next/previous media."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Отправлять медиа в высоком качестве"),
				u"Send media in high quality"_q),
			MakuGram::Settings::instance().sendMediaHighQuality(),
			[=](bool checked) {
				MakuGram::Settings::instance().setSendMediaHighQuality(checked);
			},
			Tr::Value(
				QString::fromUtf8("Автоматически включает отправку фото и медиа в максимальном HD-качестве (до 2560px) по умолчанию без необходимости каждый раз переключать режим."),
				u"Automatically enables sending photos in max HD quality (up to 2560px) by default without switching every time."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Показывать разрешение"),
				u"Show media resolution"_q),
			MakuGram::Settings::instance().showMediaResolution(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowMediaResolution(checked);
			},
			Tr::Value(
				QString::fromUtf8("Отображает разрешение изображения или видео в полноэкранном режиме просмотра медиа."),
				u"Displays image or video resolution in fullscreen media viewer."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Добавить выделение медиа в полноэкранном режиме"),
				u"Enable media selection in fullscreen"_q),
			MakuGram::Settings::instance().fullscreenMediaSelection(),
			[=](bool checked) {
				MakuGram::Settings::instance().setFullscreenMediaSelection(checked);
			},
			Tr::Value(
				QString::fromUtf8("Позволяет выделять и пересылать медиа при полноэкранном просмотре, сохраняя выделение при возврате в чат."),
				u"Allows selecting and forwarding media in fullscreen, preserving selection when returning to chat."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Расширить окно отправки медиа"),
				u"Expand media send dialog"_q),
			MakuGram::Settings::instance().expandMediaSendDialog(),
			[=](bool checked) {
				MakuGram::Settings::instance().setExpandMediaSendDialog(checked);
			},
			Tr::Value(
				QString::fromUtf8("Увеличивает ширину окна предпросмотра при отправке фото и видео, а также расширяет поле ввода подписи по высоте для комфортного написания длинного текста."),
				u"Increases preview dialog width when sending media and expands caption input height for writing long text."_q));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowAppearanceBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(Tr::Value(
			QString::fromUtf8("Внешний вид"),
			u"Appearance"_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Компактный список чатов"),
				u"Compact chat list"_q),
			MakuGram::Settings::instance().compactChatList(),
			[=](bool checked) {
				MakuGram::Settings::instance().setCompactChatList(checked);
			},
			Tr::Value(
				QString::fromUtf8("Скрывает строку предпросмотра последнего сообщения и уменьшает высоту строк чатов, умещая в два раза больше диалогов на экране."),
				u"Hides last message preview line and reduces chat row height, fitting twice as many chats on screen."_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Отключить анимацию реакций и конфетти"),
				u"Disable reaction animations and confetti"_q),
			MakuGram::Settings::instance().disableReactionAnimations(),
			[=](bool checked) {
				MakuGram::Settings::instance().setDisableReactionAnimations(checked);
			},
			Tr::Value(
				QString::fromUtf8("Отключает летающие большие эмодзи, эффекты и конфетти при установке реакций для максимальной плавности."),
				u"Disables floating emoji effects and confetti when reacting for maximum smoothness."_q));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowStreamerModeBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(Tr::Value(
			QString::fromUtf8("Режим стримера"),
			u"Streamer Mode"_q));

		AddCheckboxRow(
			box,
			Tr::Value(
				QString::fromUtf8("Включить режим стримера"),
				u"Enable Streamer Mode"_q),
			MakuGram::Settings::instance().streamerMode(),
			[=](bool checked) {
				MakuGram::Settings::instance().setStreamerMode(checked);
			},
			Tr::Value(
				QString::fromUtf8("Скрывает номера телефонов, расчетную дату регистрации и числовой ID профилей, а также размывает все аватарки пользователей и каналов во всем приложении для защиты личных данных при записи видео, демонстрации экрана или стриминге."),
				u"Hides phone numbers, estimated registration date, and profile IDs, and blurs all user and channel avatars across the app to protect personal data during streaming or screen recording."_q));

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowAboutBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setMaxHeight(kBoxMaxHeight);
		box->setTitle(Tr::Value(
			QString::fromUtf8("О клиенте MakuGram"),
			u"About MakuGram"_q));

		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				Tr::Value(
					QString::fromUtf8("MakuGram - форк Telegram с расширенными возможностями модерации, приватности и удобства."),
					u"MakuGram is a Telegram fork with advanced moderation, privacy, and convenience features."_q),
				st::boxLabel),
			st::boxRowPadding);

		box->addSkip(st::boxMediumSkip);

		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				Tr::Value(
					QString::fromUtf8("Версия: %1\nАвтор - Марк Аддерли").arg(MakuGram::kVersion),
					QString::fromUtf8("Version: %1\nAuthor - Mark Adderly").arg(MakuGram::kVersion)),
				st::boxLabel),
			st::boxRowPadding);

		box->addSkip(st::boxMediumSkip);

		const auto buttonWidth = kBoxContentWidth
			- st::boxRowPadding.left()
			- st::boxRowPadding.right();

		const auto checkUpdateBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				Tr::Value(
					QString::fromUtf8("Проверить обновления"),
					u"Check for updates"_q),
				st::defaultBoxButton),
			style::margins(
				st::boxRowPadding.left(),
				4,
				st::boxRowPadding.right(),
				4));
		checkUpdateBtn->setFullWidth(buttonWidth);
		checkUpdateBtn->setFullRadius(true);
		checkUpdateBtn->setClickedCallback([=] {
			MakuGram::CheckForUpdates(controller, /*silent=*/false);
		});

		const auto ghBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				Tr::Value(
					QString::fromUtf8("GitHub репозиторий проекта"),
					u"Project GitHub Repository"_q),
				st::defaultBoxButton),
			style::margins(
				st::boxRowPadding.left(),
				4,
				st::boxRowPadding.right(),
				4));
		ghBtn->setFullWidth(buttonWidth);
		ghBtn->setFullRadius(true);
		ghBtn->setClickedCallback([=] {
			UrlClickHandler::Open(u"https://github.com/MarkAdderly/MakuGram"_q);
		});

		const auto tgBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				Tr::Value(
					QString::fromUtf8("Telegram канал автора"),
					u"Author's Telegram Channel"_q),
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
				Tr::Value(
					QString::fromUtf8("YouTube канал автора"),
					u"Author's YouTube Channel"_q),
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
				Tr::Value(
					QString::fromUtf8("Поддержать разработку"),
					u"Support Development"_q),
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

	static bool updateChecked = false;
	if (!updateChecked) {
		updateChecked = true;
		MakuGram::CheckForUpdates(controller, /*silent=*/true);
	}

	builder.addSubsectionTitle(Tr::Value(
		QString::fromUtf8("Категории настроек"),
		u"Setting Categories"_q));

	builder.addButton({
		.title = Tr::Value(QString::fromUtf8("Сообщения"), u"Messages"_q),
		.icon = { &st::menuIconChats },
		.onClick = [=] { ShowMessagesBox(controller); },
		.keywords = { u"currency"_q, u"messages"_q, u"unread"_q, u"spam"_q, u"time"_q, u"seconds"_q, u"валюта"_q, u"сообщения"_q, u"спам"_q, u"время"_q, u"секунды"_q },
	});

	builder.addButton({
		.title = Tr::Value(QString::fromUtf8("Приватность и безопасность"), u"Privacy & Security"_q),
		.icon = { &st::menuIconLock },
		.onClick = [=] { ShowPrivacyBox(controller); },
		.keywords = { u"privacy"_q, u"typing"_q, u"copy"_q, u"delete"_q, u"phone"_q, u"blocked"_q, u"приватность"_q, u"тайпинг"_q, u"копирование"_q, u"удаление"_q, u"телефон"_q, u"номер"_q, u"блок"_q },
	});

	builder.addButton({
		.title = Tr::Value(QString::fromUtf8("Модерация чатов"), u"Chat Moderation"_q),
		.icon = { &st::menuIconAdmin },
		.onClick = [=] { ShowModerationBox(controller); },
		.keywords = { u"moderation"_q, u"ban"_q, u"badge"_q, u"requests"_q, u"search"_q, u"бан"_q, u"новичок"_q, u"заявки"_q, u"модерация"_q, u"поиск"_q },
	});

	builder.addButton({
		.title = Tr::Value(QString::fromUtf8("Профиль и информация"), u"Profile & Info"_q),
		.icon = { &st::menuIconProfile },
		.onClick = [=] { ShowProfileBox(controller); },
		.keywords = { u"id"_q, u"profile"_q, u"registration"_q, u"stories"_q, u"профиль"_q, u"возраст"_q, u"регистрация"_q, u"сторис"_q, u"истории"_q },
	});

	builder.addButton({
		.title = Tr::Value(QString::fromUtf8("Внешний вид"), u"Appearance"_q),
		.icon = { &st::menuIconChangeColors },
		.onClick = [=] { ShowAppearanceBox(controller); },
		.keywords = { u"appearance"_q, u"compact"_q, u"reactions"_q, u"вид"_q, u"компактный"_q, u"реакции"_q },
	});

	builder.addButton({
		.title = Tr::Value(QString::fromUtf8("Медиа"), u"Media"_q),
		.icon = { &st::menuIconPhoto },
		.onClick = [=] { ShowMediaBox(controller); },
		.keywords = { u"media"_q, u"zoom"_q, u"volume"_q, u"медиа"_q, u"масштаб"_q, u"экран"_q },
	});

	builder.addButton({
		.title = Tr::Value(QString::fromUtf8("Настройки прокси"), u"Proxy Settings"_q),
		.icon = { &st::menuIconNetwork },
		.onClick = [=] { ProxiesBoxController::Show(controller); },
		.keywords = { u"proxy"_q, u"прокси"_q, u"vpn"_q, u"впн"_q, u"network"_q, u"сеть"_q },
	});

	builder.addButton({
		.title = Tr::Value(QString::fromUtf8("Режим стримера"), u"Streamer Mode"_q),
		.icon = { &st::menuIconPermissions },
		.onClick = [=] { ShowStreamerModeBox(controller); },
		.keywords = { u"stream"_q, u"streamer"_q, u"blur"_q, u"стрим"_q, u"стример"_q, u"блюр"_q, u"аватарки"_q, u"приватность"_q },
	});

	builder.addButton({
		.title = Tr::Value(QString::fromUtf8("О клиенте"), u"About MakuGram"_q),
		.icon = { &st::menuIconInfo },
		.onClick = [=] { ShowAboutBox(controller); },
		.keywords = { u"about"_q, u"makugram"_q, u"version"_q, u"версия"_q, u"информация"_q },
	});

	builder.addDivider();
	builder.addDividerText(Tr::Value(
		QString::fromUtf8("Нажмите на категорию для настройки параметров MakuGram."),
		u"Click a category to configure MakuGram settings."_q));
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
	return Tr::Value(QString::fromUtf8("Функции MakuGram"), u"MakuGram Features"_q);
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
