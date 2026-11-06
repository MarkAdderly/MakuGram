/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/settings_makugram.h"

#include "makugram/makugram_settings.h"
#include "lang/lang_keys.h"
#include "settings/sections/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
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

[[nodiscard]] [[maybe_unused]] int CheckboxTextSkip() {
	return st::defaultBoxCheckbox.checkPosition.x()
		+ st::defaultCheck.diameter
		+ st::defaultBoxCheckbox.textPosition.x();
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
		rpl::producer<QString> about = nullptr) {
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
			about ? 0 : st::boxMediumSkip));
	cb->setAllowTextLines(0);
	cb->setCheckAlignment(style::al_topleft);
	cb->checkedChanges(
	) | rpl::on_next([=](bool checked) {
		onChange(checked);
	}, cb->lifetime());

	if (about) {
		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				std::move(about),
				st::boxDividerLabel),
			style::margins(
				st::boxRowPadding.left(),
				3,
				st::boxRowPadding.right(),
				st::boxMediumSkip));
	}
}

void ShowSpamFilterBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
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
				2));
		excludeSearch->setAllowTextLines(0);
		excludeSearch->setCheckAlignment(style::al_topleft);

		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				QString::fromUtf8("Сообщения, содержащие скрываемые слова, не будут отображаться в результатах глобального поиска и поиска по чату."),
				st::boxDividerLabel),
			style::margins(
				st::boxRowPadding.left(),
				3,
				st::boxRowPadding.right(),
				st::boxMediumSkip));

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

void ShowPrivacyBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setTitle(tr::lng_settings_makugram_section_privacy());

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
			tr::lng_settings_makugram_spam_filter(),
			MakuGram::Settings::instance().spamFilterEnabled(),
			[=](bool checked) {
				MakuGram::Settings::instance().setSpamFilterEnabled(checked);
			},
			tr::lng_settings_makugram_spam_filter_about());

		const auto spamSetupBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				tr::lng_settings_makugram_spam_setup(),
				st::defaultBoxButton),
			style::margins(
				st::boxRowPadding.left(),
				0,
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

void ShowModerationBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
		box->setTitle(tr::lng_settings_makugram_section_moderation());

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
			tr::lng_settings_makugram_quick_ban(),
			MakuGram::Settings::instance().quickBanInRequests(),
			[=](bool checked) {
				MakuGram::Settings::instance().setQuickBanInRequests(checked);
			},
			tr::lng_settings_makugram_quick_ban_about());

		AddCheckboxRow(
			box,
			tr::lng_settings_makugram_quick_delete_ban(),
			MakuGram::Settings::instance().quickDeleteAndBan(),
			[=](bool checked) {
				MakuGram::Settings::instance().setQuickDeleteAndBan(checked);
			},
			tr::lng_settings_makugram_quick_delete_ban_about());

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
		box->setTitle(tr::lng_settings_makugram_section_profile());

		AddCheckboxRow(
			box,
			tr::lng_settings_makugram_profile_details(),
			MakuGram::Settings::instance().showProfileDetails(),
			[=](bool checked) {
				MakuGram::Settings::instance().setShowProfileDetails(checked);
			},
			tr::lng_settings_makugram_profile_details_about());

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowMediaBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
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
			tr::lng_settings_makugram_fast_download(),
			MakuGram::Settings::instance().fastDownload(),
			[=](bool checked) {
				MakuGram::Settings::instance().setFastDownload(checked);
			},
			tr::lng_settings_makugram_fast_download_about());

		box->addButton(tr::lng_close(), [=] {
			box->closeBox();
		});
	}));
}

void ShowAboutBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(kBoxContentWidth);
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
				QString::fromUtf8(
					"• Безлимитный мультиаккаунт (до 100 аккаунтов) активен по умолчанию.\n"
					"• Приватность и сообщения: фильтр скрываемых слов со звёздочками (*), точное время с секундами, пересылка без автора.\n"
					"• Модерация и безопасность: быстрый бан в заявках, удаление и бан из меню сообщений, бейдж (Новичок), скрытие сообщений заблокированных.\n"
					"• Медиа и сеть: открытие во весь экран, ускоренная параллельная загрузка файлов.\n\n"
					"Версия клиента: MakuGram 7.4.0"),
				st::boxDividerLabel),
			st::boxRowPadding);

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

	// 1. Приватность и сообщения
	builder.addButton({
		.title = tr::lng_settings_makugram_section_privacy(),
		.icon = { &st::menuIconLock },
		.onClick = [=] { ShowPrivacyBox(controller); },
		.keywords = { u"time"_q, u"seconds"_q, u"exact"_q, u"forward"_q, u"spam"_q, u"words"_q, u"время"_q, u"секунды"_q, u"пересылка"_q, u"спам"_q, u"слова"_q, u"скрыть"_q },
	});

	// 2. Модерация и безопасность
	builder.addButton({
		.title = tr::lng_settings_makugram_section_moderation(),
		.icon = { &st::menuIconAdmin },
		.onClick = [=] { ShowModerationBox(controller); },
		.keywords = { u"ban"_q, u"badge"_q, u"requests"_q, u"бан"_q, u"новичок"_q, u"заявки"_q, u"блок"_q },
	});

	// 3. Профиль и информация
	builder.addButton({
		.title = tr::lng_settings_makugram_section_profile(),
		.icon = { &st::menuIconProfile },
		.onClick = [=] { ShowProfileBox(controller); },
		.keywords = { u"id"_q, u"profile"_q, u"registration"_q, u"профиль"_q, u"возраст"_q, u"регистрация"_q },
	});

	// 4. Медиа и сеть
	builder.addButton({
		.title = tr::lng_settings_makugram_section_media(),
		.icon = { &st::menuIconPhoto },
		.onClick = [=] { ShowMediaBox(controller); },
		.keywords = { u"media"_q, u"zoom"_q, u"download"_q, u"медиа"_q, u"масштаб"_q, u"скорость"_q, u"экран"_q },
	});

	// 5. О клиенте
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
