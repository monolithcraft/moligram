/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "settings/sections/settings_moligram.h"

#include "lang/lang_keys.h"
#include "moligram/moligram_club.h"
#include "moligram/moligram_menu.h"
#include "moligram/moligram_palette.h"
#include "moligram/moligram_settings.h"
#include "ui/layers/generic_box.h"
#include "settings/sections/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/color_editor.h"
#include "ui/widgets/discrete_sliders.h"
#include "ui/image/image_prepare.h"
#include "core/file_utilities.h"
#include "main/main_session.h"
#include "mainwindow.h"
#include "ui/widgets/checkbox.h"
#include "ui/boxes/confirm_box.h"
#include "core/application.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Builder;
using Moligram::Option;

void AddToggle(
		SectionBuilder &builder,
		QString id,
		rpl::producer<QString> title,
		Option option,
		const style::icon *icon = nullptr) {
	const auto button = builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.st = icon ? &st::settingsButton : &st::settingsButtonNoIcon,
		.icon = { icon },
		.toggled = Moligram::Value(option),
		.highlight = { .rippleShape = true },
	});
	if (button) {
		button->toggledChanges(
		) | rpl::on_next([=](bool enabled) {
			Moligram::Set(option, enabled);
		}, button->lifetime());
	}
}

void AddPicker(
		SectionBuilder &builder,
		QString id,
		rpl::producer<QString> title,
		Moligram::Highlight which) {
	builder.addControl({
		.factory = [=](not_null<Ui::VerticalLayout*> container) {
			return object_ptr<Moligram::HighlightPicker>(
				container,
				which,
				rpl::duplicate(title));
		},
		.id = std::move(id),
		.title = rpl::duplicate(title),
		.keywords = { u"color"_q, u"highlight"_q },
	});
}

[[nodiscard]] std::vector<std::pair<int, QString>> RadiusOptions() {
	return {
		{ -1, tr::lng_moligram_radius_default(tr::now) },
		{ 4, tr::lng_moligram_radius_sharp(tr::now) },
		{ 12, tr::lng_moligram_radius_soft(tr::now) },
		{ 20, tr::lng_moligram_radius_round(tr::now) },
	};
}

[[nodiscard]] QString RadiusName(int radius) {
	for (const auto &[value, name] : RadiusOptions()) {
		if (value == radius) {
			return name;
		}
	}
	return QString::number(radius);
}

void RadiusBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::lng_moligram_radius());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		Moligram::BubbleRadius());
	for (const auto &[value, name] : RadiusOptions()) {
		box->addRow(
			object_ptr<Ui::Radiobutton>(
				box,
				group,
				value,
				name,
				st::defaultBoxCheckbox),
			st::boxRowPadding + style::margins(0, 0, 0, st::boxOptionListSkip));
	}
	box->addButton(tr::lng_settings_save(), [=] {
		const auto value = group->current();
		box->closeBox();
		if (value == Moligram::BubbleRadius()) {
			return;
		}
		Moligram::SetBubbleRadius(value);
		controller->show(Ui::MakeConfirmBox({
			.text = tr::lng_settings_need_restart(),
			.confirmed = [] { Core::Restart(); },
			.confirmText = tr::lng_settings_restart_now(),
		}));
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

[[nodiscard]] std::vector<std::pair<int, QString>> SizeOptions() {
	return {
		{ 160, tr::lng_moligram_sticker_small(tr::now) },
		{ -1, tr::lng_moligram_radius_default(tr::now) },
		{ 280, tr::lng_moligram_sticker_large(tr::now) },
		{ 340, tr::lng_moligram_sticker_huge(tr::now) },
	};
}

[[nodiscard]] QString SizeName(int size) {
	for (const auto &[value, name] : SizeOptions()) {
		if (value == size) {
			return name;
		}
	}
	return QString::number(size);
}

void SizeBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::lng_moligram_sticker_size());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		Moligram::StickerSize());
	for (const auto &[value, name] : SizeOptions()) {
		box->addRow(
			object_ptr<Ui::Radiobutton>(
				box,
				group,
				value,
				name,
				st::defaultBoxCheckbox),
			st::boxRowPadding + style::margins(0, 0, 0, st::boxOptionListSkip));
	}
	box->addButton(tr::lng_settings_save(), [=] {
		const auto value = group->current();
		box->closeBox();
		if (value == Moligram::StickerSize()) {
			return;
		}
		Moligram::SetStickerSize(value);
		controller->show(Ui::MakeConfirmBox({
			.text = tr::lng_settings_need_restart(),
			.confirmed = [] { Core::Restart(); },
			.confirmText = tr::lng_settings_restart_now(),
		}));
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void BuildGhostSection(SectionBuilder &builder) {
	builder.addSkip();
	builder.addSubsectionTitle({
		.id = u"moligram/ghost"_q,
		.title = tr::lng_moligram_ghost_title(),
		.keywords = { u"ghost"_q, u"invisible"_q, u"stealth"_q },
	});

	const auto ghost = builder.addButton({
		.id = u"moligram/ghost-toggle"_q,
		.title = tr::lng_moligram_ghost_toggle(),
		.st = &st::settingsButton,
		.icon = { &st::menuIconStealth },
		.toggled = Moligram::GhostModeValue(),
		.keywords = { u"ghost"_q, u"invisible"_q, u"offline"_q },
		.highlight = { .rippleShape = true },
	});
	if (ghost) {
		ghost->toggledChanges(
		) | rpl::filter([](bool enabled) {
			return enabled != Moligram::GhostMode();
		}) | rpl::on_next([](bool enabled) {
			Moligram::SetGhostMode(enabled);
		}, ghost->lifetime());
	}

	AddToggle(
		builder,
		u"moligram/send-read"_q,
		tr::lng_moligram_send_read(),
		Option::SendReadMessages);
	AddToggle(
		builder,
		u"moligram/send-read-stories"_q,
		tr::lng_moligram_send_read_stories(),
		Option::SendReadStories);
	AddToggle(
		builder,
		u"moligram/send-online"_q,
		tr::lng_moligram_send_online(),
		Option::SendOnlineStatus);
	AddToggle(
		builder,
		u"moligram/send-typing"_q,
		tr::lng_moligram_send_typing(),
		Option::SendTyping);
	AddToggle(
		builder,
		u"moligram/send-upload"_q,
		tr::lng_moligram_send_upload(),
		Option::SendUploadProgress);
	AddToggle(
		builder,
		u"moligram/send-listened"_q,
		tr::lng_moligram_send_listened(),
		Option::SendListened);
	builder.addSkip();
	builder.addDividerText(tr::lng_moligram_ghost_about());

	builder.addSkip();
	AddToggle(
		builder,
		u"moligram/read-after-send"_q,
		tr::lng_moligram_read_after_send(),
		Option::ReadAfterSending);
	AddToggle(
		builder,
		u"moligram/ghost-in-menu"_q,
		tr::lng_moligram_ghost_in_menu(),
		Option::GhostInMainMenu);
	builder.addSkip();
	builder.addDividerText(tr::lng_moligram_read_after_send_about());
}

void BuildHistorySection(SectionBuilder &builder) {
	builder.addSkip();
	builder.addSubsectionTitle({
		.id = u"moligram/history"_q,
		.title = tr::lng_moligram_history_title(),
		.keywords = { u"deleted"_q, u"edited"_q, u"history"_q },
	});
	AddToggle(
		builder,
		u"moligram/keep-deleted"_q,
		tr::lng_moligram_keep_deleted(),
		Option::KeepDeletedMessages,
		&st::menuIconDelete);
	AddToggle(
		builder,
		u"moligram/keep-edits"_q,
		tr::lng_moligram_keep_edits(),
		Option::KeepEditHistory,
		&st::menuIconEdit);
	AddToggle(
		builder,
		u"moligram/deletion-alerts"_q,
		tr::lng_moligram_deletion_alerts(),
		Option::DeletionAlerts,
		&st::menuIconNotifications);
	AddToggle(
		builder,
		u"moligram/highlight-edited"_q,
		tr::lng_moligram_highlight_edited(),
		Option::HighlightEdited,
		&st::menuIconPalette);
	AddPicker(
		builder,
		u"moligram/deleted-color"_q,
		tr::lng_moligram_deleted_color(),
		Moligram::Highlight::Deleted);
	AddPicker(
		builder,
		u"moligram/edited-color"_q,
		tr::lng_moligram_edited_color(),
		Moligram::Highlight::Edited);
	builder.addSkip();
	builder.addDividerText(tr::lng_moligram_history_about());
}

void BuildOnlineAlertsSection(SectionBuilder &builder) {
	builder.addSkip();
	builder.addSubsectionTitle({
		.id = u"moligram/online-alerts"_q,
		.title = tr::lng_moligram_online_alerts_title(),
		.keywords = { u"online"_q, u"alert"_q, u"notify"_q },
	});
	builder.addButton({
		.id = u"moligram/online-alerts-list"_q,
		.title = tr::lng_moligram_online_alerts_button(),
		.st = &st::settingsButton,
		.icon = { &st::menuIconNotifications },
		.label = rpl::single(
			rpl::empty
		) | rpl::then(Moligram::Changes()) | rpl::map([] {
			const auto count = int(Moligram::ListPeers(
				Moligram::PeerList::OnlineAlerts).size());
			return count ? QString::number(count) : QString();
		}),
		.onClick = [controller = builder.controller()] {
			controller->show(Box(Moligram::OnlineAlertsBox, controller));
		},
		.keywords = { u"online"_q, u"alert"_q, u"watch"_q },
	});
	builder.addSkip();
	builder.addDividerText(tr::lng_moligram_online_alerts_about());
}

void BuildPrivacySection(SectionBuilder &builder) {
	builder.addSkip();
	builder.addSubsectionTitle({
		.id = u"moligram/privacy"_q,
		.title = tr::lng_moligram_privacy_title(),
		.keywords = { u"streamer"_q, u"privacy"_q },
	});
	AddToggle(
		builder,
		u"moligram/streamer"_q,
		tr::lng_moligram_streamer(),
		Option::StreamerMode,
		&st::menuIconUserHide);
	builder.addSkip();
	builder.addDividerText(tr::lng_moligram_streamer_about());
	builder.addSkip();
	AddToggle(
		builder,
		u"moligram/clean-links"_q,
		tr::lng_moligram_clean_links(),
		Option::CleanLinks,
		&st::menuIconLink);
	AddToggle(
		builder,
		u"moligram/panic-key"_q,
		tr::lng_moligram_panic_key(),
		Option::PanicKey,
		&st::menuIconBlock);
	AddToggle(
		builder,
		u"moligram/lock-minimize"_q,
		tr::lng_moligram_lock_minimize(),
		Option::LockOnMinimize,
		&st::menuIconLock);
	builder.addSkip();
	builder.addDividerText(tr::lng_moligram_privacy_about());
}

void BuildInterfaceSection(SectionBuilder &builder) {
	builder.addSkip();
	builder.addSubsectionTitle({
		.id = u"moligram/interface"_q,
		.title = tr::lng_moligram_interface_title(),
		.keywords = { u"interface"_q, u"appearance"_q },
	});
	AddToggle(
		builder,
		u"moligram/seconds"_q,
		tr::lng_moligram_seconds(),
		Option::ShowMessageSeconds);
	AddToggle(
		builder,
		u"moligram/peer-ids"_q,
		tr::lng_moligram_peer_ids(),
		Option::ShowPeerIds);
	AddToggle(
		builder,
		u"moligram/hide-stories"_q,
		tr::lng_moligram_hide_stories(),
		Option::HideStories);
	AddToggle(
		builder,
		u"moligram/confirm-stickers"_q,
		tr::lng_moligram_confirm_stickers(),
		Option::ConfirmStickerSending);
	AddToggle(
		builder,
		u"moligram/silent-sending"_q,
		tr::lng_moligram_silent_sending(),
		Option::SilentSending);
	AddToggle(
		builder,
		u"moligram/forward-no-author"_q,
		tr::lng_moligram_forward_no_author(),
		Option::ForwardNoAuthor);
	AddToggle(
		builder,
		u"moligram/club-badges"_q,
		tr::lng_moligram_club_badges(),
		Option::ClubBadges);
	AddToggle(
		builder,
		u"moligram/hover-profiles"_q,
		tr::lng_moligram_hover_profiles(),
		Option::HoverProfiles);
	const auto compact = builder.addButton({
		.id = u"moligram/compact-dialogs"_q,
		.title = tr::lng_moligram_compact_dialogs(),
		.st = &st::settingsButtonNoIcon,
		.toggled = Moligram::Value(Option::CompactDialogs),
		.highlight = { .rippleShape = true },
	});
	if (compact) {
		const auto controller = builder.controller();
		compact->toggledChanges(
		) | rpl::filter([](bool enabled) {
			return enabled != Moligram::Enabled(Option::CompactDialogs);
		}) | rpl::on_next([=](bool enabled) {
			Moligram::Set(Option::CompactDialogs, enabled);
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_settings_need_restart(),
				.confirmed = [] { Core::Restart(); },
				.confirmText = tr::lng_settings_restart_now(),
			}));
		}, compact->lifetime());
	}
	builder.addButton({
		.id = u"moligram/sticker-size"_q,
		.title = tr::lng_moligram_sticker_size(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(SizeName(Moligram::StickerSize())),
		.onClick = [controller = builder.controller()] {
			controller->show(Box(SizeBox, controller));
		},
		.keywords = { u"sticker"_q, u"size"_q },
	});
	builder.addButton({
		.id = u"moligram/bubble-radius"_q,
		.title = tr::lng_moligram_radius(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(RadiusName(Moligram::BubbleRadius())),
		.onClick = [controller = builder.controller()] {
			controller->show(Box(RadiusBox, controller));
		},
		.keywords = { u"bubble"_q, u"radius"_q, u"corners"_q },
	});
	builder.addButton({
		.id = u"moligram/filter-words"_q,
		.title = tr::lng_moligram_filter_button(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(
			rpl::empty
		) | rpl::then(Moligram::Changes()) | rpl::map([] {
			const auto count = int(Moligram::FilterWords().size());
			return count ? QString::number(count) : QString();
		}),
		.onClick = [controller = builder.controller()] {
			controller->show(Box(Moligram::FilterWordsBox));
		},
		.keywords = { u"filter"_q, u"words"_q, u"hide"_q },
	});
	builder.addSkip();
}

void BuildQuickPanelSection(SectionBuilder &builder) {
	builder.addSkip();
	builder.addSubsectionTitle({
		.id = u"moligram/quick"_q,
		.title = tr::lng_moligram_quick_title(),
		.keywords = { u"quick"_q, u"panel"_q, u"menu"_q },
	});
	AddToggle(
		builder,
		u"moligram/quick-show"_q,
		tr::lng_moligram_quick_show(),
		Option::QuickPanel,
		&st::menuIconShowAll);
	AddToggle(
		builder,
		u"moligram/quick-ghost"_q,
		tr::lng_moligram_ghost_toggle(),
		Option::QuickGhost);
	AddToggle(
		builder,
		u"moligram/quick-streamer"_q,
		tr::lng_moligram_streamer(),
		Option::QuickStreamer);
	AddToggle(
		builder,
		u"moligram/quick-stories"_q,
		tr::lng_moligram_hide_stories(),
		Option::QuickHideStories);
	AddToggle(
		builder,
		u"moligram/quick-deleted"_q,
		tr::lng_moligram_keep_deleted(),
		Option::QuickKeepDeleted);
	AddToggle(
		builder,
		u"moligram/quick-edits"_q,
		tr::lng_moligram_keep_edits(),
		Option::QuickKeepEdits);
	AddToggle(
		builder,
		u"moligram/quick-seconds"_q,
		tr::lng_moligram_seconds(),
		Option::QuickSeconds);
	AddToggle(
		builder,
		u"moligram/quick-ids"_q,
		tr::lng_moligram_peer_ids(),
		Option::QuickPeerIds);
	AddToggle(
		builder,
		u"moligram/quick-dnd"_q,
		tr::lng_moligram_dnd(),
		Option::QuickDnd);
	AddToggle(
		builder,
		u"moligram/quick-silent"_q,
		tr::lng_moligram_silent_sending(),
		Option::QuickSilent);
	builder.addSkip();
	builder.addDividerText(tr::lng_moligram_quick_about());
}

void ClubColorsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::lng_moligram_club_colors());
	const auto session = &controller->session();
	const auto [from, to] = Moligram::ClubColors();
	struct State {
		rpl::variable<Moligram::ClubCard> card;
		int side = 0;
	};
	const auto state = box->lifetime().make_state<State>();
	state->card = Moligram::ClubCard{
		.banner = Moligram::kBannerColors,
		.from = from,
		.to = to,
		.owner = session->userId(),
	};
	box->addRow(
		object_ptr<Moligram::BannerCard>(
			box,
			session,
			state->card.value()),
		style::margins());
	const auto slider = box->addRow(
		object_ptr<Ui::SettingsSlider>(box, st::defaultTabsSlider),
		st::boxRowPadding + style::margins(0, st::boxOptionListSkip, 0, 0));
	slider->setSections({
		tr::lng_moligram_club_color_start(tr::now),
		tr::lng_moligram_club_color_end(tr::now),
	});
	const auto editor = box->addRow(object_ptr<ColorEditor>(
		box,
		ColorEditor::Mode::HSL,
		from));
	slider->sectionActivated() | rpl::on_next([=](int side) {
		state->side = side;
		const auto card = state->card.current();
		editor->showColor(side ? card.to : card.from);
	}, slider->lifetime());
	editor->colorValue() | rpl::on_next([=](QColor color) {
		auto card = state->card.current();
		(state->side ? card.to : card.from) = color;
		state->card = card;
	}, editor->lifetime());
	const auto save = [=] {
		const auto card = state->card.current();
		Moligram::SetClubColors(card.from, card.to);
		Moligram::SetClubBanner(Moligram::kBannerColors);
		box->closeBox();
	};
	editor->submitRequests() | rpl::on_next(save, editor->lifetime());
	box->setFocusCallback([=] { editor->setInnerFocus(); });
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	box->setWidth(editor->width());
}

void ChooseClubImage(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller);
	FileDialog::GetOpenPath(
		controller->widget().get(),
		tr::lng_choose_image(tr::now),
		FileDialog::ImagesFilter(),
		[=](const FileDialog::OpenResult &result) {
			const auto strong = weak.get();
			if (!strong) {
				return;
			}
			auto image = Images::Read({
				.path = result.paths.isEmpty()
					? QString()
					: result.paths.front(),
				.content = result.remoteContent,
				.forceOpaque = true,
			}).image;
			if (image.isNull()) {
				return;
			}
			strong->showToast(tr::lng_moligram_club_uploading(tr::now));
			Moligram::UploadClubImage(
				&strong->session(),
				std::move(image),
				[=](bool success) {
					if (const auto strong = weak.get()) {
						strong->showToast(success
							? tr::lng_moligram_club_uploaded(tr::now)
							: tr::lng_moligram_club_upload_failed(tr::now));
					}
				});
		});
}

void BuildClubSection(SectionBuilder &builder) {
	builder.addSkip();
	builder.addSubsectionTitle({
		.id = u"moligram/club"_q,
		.title = tr::lng_moligram_club_title(),
		.keywords = { u"club"_q, u"badge"_q, u"banner"_q },
	});
	const auto controller = builder.controller();
	builder.addControl({
		.factory = [=](not_null<Ui::VerticalLayout*> container) {
			return object_ptr<Moligram::BannerCard>(
				container,
				&controller->session(),
				Moligram::OwnClubCardValue(&controller->session()));
		},
		.id = u"moligram/club-preview"_q,
		.title = tr::lng_moligram_club_title(),
		.keywords = { u"club"_q, u"banner"_q },
	});
	AddToggle(
		builder,
		u"moligram/club-member"_q,
		tr::lng_moligram_club_toggle(),
		Option::ClubMember,
		&st::menuIconProfile);
	builder.addControl({
		.factory = [=](not_null<Ui::VerticalLayout*> container) {
			return object_ptr<Moligram::BannerPicker>(
				container,
				[=] { controller->show(Box(ClubColorsBox, controller)); },
				[=] { ChooseClubImage(controller); });
		},
		.id = u"moligram/club-banner"_q,
		.title = tr::lng_moligram_club_banner(),
		.keywords = { u"banner"_q, u"color"_q },
	});
	builder.addSkip();
	builder.addDividerText(tr::lng_moligram_club_about());
}

void BuildMoligramSectionContent(SectionBuilder &builder) {
	BuildClubSection(builder);
	builder.addDivider();
	BuildQuickPanelSection(builder);
	builder.addDivider();
	BuildGhostSection(builder);
	builder.addDivider();
	BuildHistorySection(builder);
	builder.addDivider();
	BuildOnlineAlertsSection(builder);
	builder.addDivider();
	BuildPrivacySection(builder);
	builder.addDivider();
	BuildInterfaceSection(builder);
}

class MoligramSection : public Section<MoligramSection> {
public:
	MoligramSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

MoligramSection::MoligramSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> MoligramSection::title() {
	return tr::lng_moligram_settings();
}

void MoligramSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	const SectionBuildMethod buildMethod = [](
			not_null<Ui::VerticalLayout*> container,
			not_null<Window::SessionController*> controller,
			Fn<void(Type)> showOther,
			rpl::producer<> showFinished) {
		const auto isPaused = Window::PausedIn(
			controller,
			Window::GifPauseReason::Layer);
		auto builder = SectionBuilder(WidgetContext{
			.container = container,
			.controller = controller,
			.showOther = std::move(showOther),
			.isPaused = isPaused,
		});
		BuildMoligramSectionContent(builder);
	};

	build(content, buildMethod);
	Ui::ResizeFitChild(this, content);
}

const auto kMeta = BuildHelper({
	.id = MoligramSection::Id(),
	.parentId = MainId(),
	.title = &tr::lng_moligram_settings,
	.icon = &st::menuIconStealth,
}, [](SectionBuilder &builder) {
	BuildMoligramSectionContent(builder);
});

} // namespace

Type MoligramId() {
	return MoligramSection::Id();
}

namespace Builder {

SectionBuildMethod MoligramSectionBuild = kMeta.build;

} // namespace Builder
} // namespace Settings
