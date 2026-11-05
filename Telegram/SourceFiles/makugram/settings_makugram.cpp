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
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {
namespace {

using namespace Builder;

class MakuGramSection : public Section<MakuGramSection> {
public:
	MakuGramSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();
};

void ShowSpamFilterBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_settings_makugram_spam_filter());

		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::settingsBio,
			rpl::single(QString::fromUtf8("Слова через запятую (напр.: казино, 1xbet, крипта)")),
			MakuGram::Settings::instance().spamKeywords()));

		box->addButton(tr::lng_settings_save(), [=] {
			MakuGram::Settings::instance().setSpamKeywords(field->getLastText());
			box->closeBox();
			controller->showToast(QString::fromUtf8("Список ключевых слов сохранен"));
		});
		box->addButton(tr::lng_cancel(), [=] {
			box->closeBox();
		});
	}));
}

void BuildMakuGramSection(SectionBuilder &builder) {
	const auto controller = builder.controller();

	// 1. Приватность и сообщения
	builder.addSubsectionTitle(tr::lng_settings_makugram_section_privacy());

	const auto exactTimeToggle = builder.addButton({
		.title = tr::lng_settings_makugram_exact_time(),
		.toggled = rpl::single(MakuGram::Settings::instance().showExactTime()),
		.keywords = { u"time"_q, u"seconds"_q, u"точное"_q, u"время"_q, u"секунды"_q },
	});
	if (exactTimeToggle) {
		exactTimeToggle->toggledValue(
		) | rpl::on_next([=](bool checked) {
			MakuGram::Settings::instance().setShowExactTime(checked);
		}, exactTimeToggle->lifetime());
	}
	builder.addDividerText(tr::lng_settings_makugram_exact_time_about());

	builder.addDividerText(tr::lng_settings_makugram_fwd_no_author_hint());

	// 2. Модерация и безопасность
	builder.addSubsectionTitle(tr::lng_settings_makugram_section_moderation());

	const auto hideBlockedToggle = builder.addButton({
		.title = tr::lng_settings_makugram_hide_blocked(),
		.toggled = rpl::single(MakuGram::Settings::instance().hideMessagesFromBlocked()),
		.keywords = { u"blocked"_q, u"hide"_q, u"messages"_q, u"заблокированные"_q },
	});
	if (hideBlockedToggle) {
		hideBlockedToggle->toggledValue(
		) | rpl::on_next([=](bool checked) {
			MakuGram::Settings::instance().setHideMessagesFromBlocked(checked);
		}, hideBlockedToggle->lifetime());
	}
	builder.addDividerText(tr::lng_settings_makugram_hide_blocked_about());

	const auto quickBanToggle = builder.addButton({
		.title = tr::lng_settings_makugram_quick_ban(),
		.toggled = rpl::single(MakuGram::Settings::instance().quickBanInRequests()),
		.keywords = { u"ban"_q, u"requests"_q, u"moderation"_q, u"бан"_q, u"заявки"_q },
	});
	if (quickBanToggle) {
		quickBanToggle->toggledValue(
		) | rpl::on_next([=](bool checked) {
			MakuGram::Settings::instance().setQuickBanInRequests(checked);
		}, quickBanToggle->lifetime());
	}
	builder.addDividerText(tr::lng_settings_makugram_quick_ban_about());

	const auto quickDeleteBanToggle = builder.addButton({
		.title = tr::lng_settings_makugram_quick_delete_ban(),
		.toggled = rpl::single(MakuGram::Settings::instance().quickDeleteAndBan()),
		.keywords = { u"delete"_q, u"ban"_q, u"удалить"_q, u"забанить"_q },
	});
	if (quickDeleteBanToggle) {
		quickDeleteBanToggle->toggledValue(
		) | rpl::on_next([=](bool checked) {
			MakuGram::Settings::instance().setQuickDeleteAndBan(checked);
		}, quickDeleteBanToggle->lifetime());
	}
	builder.addDividerText(tr::lng_settings_makugram_quick_delete_ban_about());

	const auto newbieBadgeToggle = builder.addButton({
		.title = tr::lng_settings_makugram_newbie_badge(),
		.toggled = rpl::single(MakuGram::Settings::instance().showNewbieBadge()),
		.keywords = { u"newbie"_q, u"badge"_q, u"новичок"_q, u"бейдж"_q },
	});
	if (newbieBadgeToggle) {
		newbieBadgeToggle->toggledValue(
		) | rpl::on_next([=](bool checked) {
			MakuGram::Settings::instance().setShowNewbieBadge(checked);
		}, newbieBadgeToggle->lifetime());
	}
	builder.addDividerText(tr::lng_settings_makugram_newbie_badge_about());

	const auto spamFilterToggle = builder.addButton({
		.title = tr::lng_settings_makugram_spam_filter(),
		.toggled = rpl::single(MakuGram::Settings::instance().spamFilterEnabled()),
		.keywords = { u"spam"_q, u"filter"_q, u"спам"_q, u"фильтр"_q },
	});
	if (spamFilterToggle) {
		spamFilterToggle->toggledValue(
		) | rpl::on_next([=](bool checked) {
			MakuGram::Settings::instance().setSpamFilterEnabled(checked);
		}, spamFilterToggle->lifetime());
	}
	builder.addDividerText(tr::lng_settings_makugram_spam_filter_about());

	if (controller) {
		builder.addButton({
			.title = tr::lng_settings_makugram_spam_setup(),
			.onClick = [=] { ShowSpamFilterBox(controller); },
		});
	}

	// 3. Профиль и информация
	builder.addSubsectionTitle(tr::lng_settings_makugram_section_profile());

	const auto profileDetailsToggle = builder.addButton({
		.title = tr::lng_settings_makugram_profile_details(),
		.toggled = rpl::single(MakuGram::Settings::instance().showProfileDetails()),
		.keywords = { u"id"_q, u"profile"_q, u"профиль"_q, u"возраст"_q },
	});
	if (profileDetailsToggle) {
		profileDetailsToggle->toggledValue(
		) | rpl::on_next([=](bool checked) {
			MakuGram::Settings::instance().setShowProfileDetails(checked);
		}, profileDetailsToggle->lifetime());
	}
	builder.addDividerText(tr::lng_settings_makugram_profile_details_about());

	// 4. Медиа и сеть
	builder.addSubsectionTitle(tr::lng_settings_makugram_section_media());

	const auto alwaysZoomToggle = builder.addButton({
		.title = tr::lng_settings_makugram_always_zoom(),
		.toggled = rpl::single(MakuGram::Settings::instance().alwaysZoomMediaToScreen()),
		.keywords = { u"media"_q, u"zoom"_q, u"fullscreen"_q, u"медиа"_q, u"масштаб"_q, u"экран"_q },
	});
	if (alwaysZoomToggle) {
		alwaysZoomToggle->toggledValue(
		) | rpl::on_next([=](bool checked) {
			MakuGram::Settings::instance().setAlwaysZoomMediaToScreen(checked);
		}, alwaysZoomToggle->lifetime());
	}
	builder.addDividerText(tr::lng_settings_makugram_always_zoom_about());

	const auto fastDownloadToggle = builder.addButton({
		.title = tr::lng_settings_makugram_fast_download(),
		.toggled = rpl::single(MakuGram::Settings::instance().fastDownload()),
		.keywords = { u"download"_q, u"speed"_q, u"скачивание"_q, u"скорость"_q },
	});
	if (fastDownloadToggle) {
		fastDownloadToggle->toggledValue(
		) | rpl::on_next([=](bool checked) {
			MakuGram::Settings::instance().setFastDownload(checked);
		}, fastDownloadToggle->lifetime());
	}
	builder.addDividerText(tr::lng_settings_makugram_fast_download_about());

	// 5. О клиенте
	builder.addSubsectionTitle(rpl::single(QString::fromUtf8("О клиенте")));
	builder.addDividerText(tr::lng_settings_makugram_about());
	builder.addDividerText(rpl::single(QString::fromUtf8("• Безлимитный мультиаккаунт (до 100 аккаунтов) активен по умолчанию.")));
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
