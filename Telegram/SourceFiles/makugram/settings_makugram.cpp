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
#include "ui/widgets/buttons.h"
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

void BuildMakuGramSection(SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(QString::fromUtf8("Сообщения и чаты")));

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

	builder.addSubsectionTitle(rpl::single(QString::fromUtf8("Модерация и заявки")));

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

	builder.addSubsectionTitle(rpl::single(QString::fromUtf8("О клиенте")));
	builder.addDividerText(tr::lng_settings_makugram_about());
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
