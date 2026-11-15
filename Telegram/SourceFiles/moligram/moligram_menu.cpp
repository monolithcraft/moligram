/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_menu.h"

#include "api/api_common.h"
#include "apiwrap.h"
#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_document.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "data/data_peer_values.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "history/view/history_view_item_preview.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "moligram/moligram_history.h"
#include "moligram/moligram_log.h"
#include "moligram/moligram_settings.h"
#include "ui/boxes/choose_date_time.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/show.h"
#include "ui/painter.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_dialogs.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

namespace Moligram {

QString PeerIdText(not_null<PeerData*> peer) {
	constexpr auto kChannelShift = int64(1000000000000);
	if (const auto user = peerToUser(peer->id)) {
		return QString::number(user.bare);
	} else if (const auto chat = peerToChat(peer->id)) {
		return QString::number(-int64(chat.bare));
	} else if (const auto channel = peerToChannel(peer->id)) {
		return QString::number(-kChannelShift - int64(channel.bare));
	}
	return QString::number(peer->id.value);
}

namespace {

void Remind(
		std::shared_ptr<Ui::Show> show,
		not_null<HistoryItem*> item,
		TimeId when) {
	const auto session = &item->history()->session();
	const auto self = session->data().history(session->user());
	auto action = Api::SendAction(self, Api::SendOptions{ .scheduled = when });
	const auto time = QLocale().toString(
		base::unixtime::parse(when),
		QLocale::ShortFormat);
	session->api().forwardMessages(
		Data::ResolvedForwardDraft{ .items = { item } },
		action,
		[=] {
			show->showToast(tr::lng_moligram_remind_done(tr::now, lt_time, time));
		});
}

[[nodiscard]] TimeId At(QDate date, int hour) {
	return TimeId(QDateTime(date, QTime(hour, 0)).toSecsSinceEpoch());
}

void RemindBox(
		not_null<Ui::GenericBox*> box,
		std::shared_ptr<Ui::Show> show,
		not_null<HistoryItem*> item) {
	box->setTitle(tr::lng_moligram_remind_title());
	box->setWidth(st::boxWideWidth);
	const auto now = QDateTime::currentDateTime();
	const auto today = now.date();
	auto presets = std::vector<std::pair<QString, TimeId>>();
	const auto hour = TimeId(now.toSecsSinceEpoch() + 3600);
	presets.emplace_back(tr::lng_moligram_remind_hour(tr::now), hour);
	if (now.time().hour() < 18) {
		presets.emplace_back(
			tr::lng_moligram_remind_evening(tr::now),
			At(today, 19));
	}
	presets.emplace_back(
		tr::lng_moligram_remind_tomorrow(tr::now),
		At(today.addDays(1), 9));
	presets.emplace_back(
		tr::lng_moligram_remind_week(tr::now),
		At(today.addDays(7), 9));
	const auto weak = base::make_weak(box);
	const auto history = item->history();
	const auto id = item->fullId();
	const auto send = [=](TimeId when) {
		if (const auto current = history->owner().message(id)) {
			Remind(show, current, when);
		}
		if (const auto strong = weak.get()) {
			strong->closeBox();
		}
	};
	for (const auto &[title, when] : presets) {
		const auto label = QLocale().toString(
			base::unixtime::parse(when),
			QLocale::ShortFormat);
		const auto button = box->addRow(
			object_ptr<Ui::SettingsButton>(
				box,
				rpl::single(title),
				st::settingsButtonNoIcon),
			style::margins());
		const auto right = Ui::CreateChild<Ui::FlatLabel>(
			button,
			label,
			st::defaultFlatLabel);
		right->setAttribute(Qt::WA_TransparentForMouseEvents);
		right->setTextColorOverride(st::windowSubTextFg->c);
		button->widthValue() | rpl::on_next([=](int width) {
			right->moveToRight(
				st::settingsButtonNoIcon.padding.right(),
				(button->height() - right->height()) / 2,
				width);
		}, right->lifetime());
		const auto value = when;
		button->setClickedCallback([=] { send(value); });
	}
	const auto custom = box->addRow(
		object_ptr<Ui::SettingsButton>(
			box,
			tr::lng_moligram_remind_custom(),
			st::settingsButtonNoIcon),
		style::margins());
	custom->setClickedCallback([=] {
		show->showBox(Box([=](not_null<Ui::GenericBox*> inner) {
			const auto close = base::make_weak(inner);
			Ui::ChooseDateTimeBox(inner, {
				.title = tr::lng_moligram_remind_title(),
				.submit = tr::lng_settings_save(),
				.done = [=](TimeId when) {
					send(when);
					if (const auto strong = close.get()) {
						strong->closeBox();
					}
				},
				.min = [] { return base::unixtime::now() + 60; },
				.time = base::unixtime::now() + 3600,
				.max = [] { return base::unixtime::now() + 365 * 86400; },
			});
		}));
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace

void AddMessageMenuActions(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller) {
	const auto show = controller->uiShow();
	if (!Revisions(item).empty()) {
		const auto weak = base::make_weak(controller);
		const auto id = item->fullId();
		const auto owner = &item->history()->owner();
		menu->addAction(tr::lng_moligram_edited_history(tr::now), [=] {
			const auto strong = weak.get();
			if (const auto current = owner->message(id); strong && current) {
				ShowEditLog(strong, current);
			}
		}, &st::menuIconEdit);
	}
	if (!Enabled(Option::SendReadMessages)
		&& item->isRegular()
		&& !item->out()) {
		const auto history = item->history();
		const auto id = item->id;
		menu->addAction(tr::lng_moligram_mark_read(tr::now), [=] {
			history->owner().histories().readInboxTillOnServer(history, id);
			show->showToast(tr::lng_moligram_marked_read(tr::now));
		}, &st::menuIconMarkRead);
	}
	if (item->isRegular() && item->allowsForward()) {
		menu->addAction(tr::lng_moligram_remind(tr::now), [=] {
			show->showBox(Box(RemindBox, show, item));
		}, &st::menuIconSchedule);
	}
	if (Enabled(Option::ShowPeerIds) && item->isRegular()) {
		const auto id = QString::number(item->id.bare);
		menu->addAction(tr::lng_moligram_copy_message_id(tr::now), [=] {
			QGuiApplication::clipboard()->setText(id);
			show->showToast(tr::lng_moligram_id_copied(tr::now));
		}, &st::menuIconCopy);
	}
}

bool ConfirmDocumentSending(
		std::shared_ptr<Ui::Show> show,
		not_null<DocumentData*> document,
		Fn<void()> send) {
	if (!Enabled(Option::ConfirmStickerSending)) {
		return false;
	}
	const auto sticker = (document->sticker() != nullptr);
	if (!sticker && !document->isGifv() && !document->isAnimation()) {
		return false;
	}
	show->showBox(Ui::MakeConfirmBox({
		.text = (sticker
			? tr::lng_moligram_send_sticker_confirm()
			: tr::lng_moligram_send_gif_confirm()),
		.confirmed = [=](Fn<void()> close) {
			send();
			close();
		},
		.confirmText = tr::lng_send_button(),
	}));
	return true;
}

void FilterWordsBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_moligram_filter_title());
	box->setWidth(st::boxWideWidth);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_moligram_filter_about(),
		st::boxDividerLabel));
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		Ui::InputField::Mode::MultiLine,
		tr::lng_moligram_filter_placeholder(),
		FilterWords().join('\n')));
	box->setFocusCallback([=] { field->setFocusFast(); });
	box->addButton(tr::lng_settings_save(), [=] {
		SetFilterWords(field->getLastText().split('\n'));
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void FillPeerMenu(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		const Ui::Menu::MenuCallback &addAction) {
	const auto history = peer->owner().historyLoaded(peer);
	if (!history) {
		return;
	}
	const auto count = int(DeletedIn(history).size());
	addAction(
		tr::lng_moligram_deleted_show(tr::now, lt_count, float64(count)),
		[=] { ShowDeletedLog(controller, history); },
		&st::menuIconArchive);
	if (count > 0) {
		addAction({
			.text = tr::lng_moligram_deleted_clear(tr::now),
			.handler = [=] { ClearDeleted(history); },
			.icon = &st::menuIconDeleteAttention,
			.isAttention = true,
		});
	}
	if (!SendsReads(peer->id)) {
		addAction(tr::lng_moligram_read_on_server(tr::now), [=] {
			history->owner().histories().readInboxOnServer(history);
		}, &st::menuIconMarkRead);
	}
	if (GhostMode() || !Enabled(Option::SendReadMessages)) {
		const auto excepted = InList(PeerList::GhostExceptions, peer->id);
		const auto id = peer->id;
		addAction(
			(excepted
				? tr::lng_moligram_ghost_exception_remove(tr::now)
				: tr::lng_moligram_ghost_exception_add(tr::now)),
			[=] {
				SetInList(PeerList::GhostExceptions, id, !excepted);
				controller->showToast(excepted
					? tr::lng_moligram_ghost_exception_removed(tr::now)
					: tr::lng_moligram_ghost_exception_added(tr::now));
			},
			&st::menuIconStealth);
	}
	AddOnlineAlertAction(controller, peer, addAction);
}

void AddOnlineAlertAction(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		const Ui::Menu::MenuCallback &addAction) {
	const auto user = peer->asUser();
	if (!user || user->isSelf() || user->isBot()) {
		return;
	}
	const auto watched = InList(PeerList::OnlineAlerts, user->id);
	const auto id = user->id;
	addAction(
		(watched
			? tr::lng_moligram_online_alert_off(tr::now)
			: tr::lng_moligram_online_alert_on(tr::now)),
		[=] {
			SetInList(PeerList::OnlineAlerts, id, !watched);
			controller->showToast(watched
				? tr::lng_moligram_online_alert_disabled(tr::now)
				: tr::lng_moligram_online_alert_enabled(tr::now));
		},
		&st::menuIconNotifications);
}

void OnlineAlertsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::lng_moligram_online_alerts_title());
	box->setWidth(st::boxWideWidth);
	const auto session = &controller->session();
	const auto list = box->addRow(
		object_ptr<Ui::VerticalLayout>(box),
		QMargins());
	const auto empty = box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_moligram_online_alerts_empty(),
		st::boxDividerLabel));

	const auto addRow = [=](not_null<UserData*> user) {
		const auto height = st::defaultPeerListItem.height;
		const auto size = st::defaultPeerListItem.photoSize;
		const auto row = list->add(object_ptr<Ui::AbstractButton>(list));
		row->resize(row->width(), height);
		const auto userpic = row->lifetime().make_state<Ui::PeerUserpicView>();
		const auto remove = Ui::CreateChild<Ui::IconButton>(
			row,
			st::stickersRemove);
		row->widthValue(
		) | rpl::on_next([=](int width) {
			remove->moveToRight(
				st::boxRowPadding.right() / 2,
				(height - remove->height()) / 2,
				width);
		}, row->lifetime());
		row->paintRequest(
		) | rpl::on_next([=] {
			auto p = Painter(row);
			if (row->isOver()) {
				p.fillRect(row->rect(), st::windowBgOver);
			}
			const auto left = st::boxRowPadding.left();
			user->paintUserpic(p, *userpic, left, (height - size) / 2, size);
			const auto textLeft = left + size + st::defaultPeerListItem.namePosition.x() - st::defaultPeerListItem.photoPosition.x();
			const auto available = remove->x() - textLeft;
			p.setPen(st::contactsNameFg);
			p.setFont(st::semiboldFont);
			p.drawTextLeft(
				textLeft,
				st::defaultPeerListItem.namePosition.y(),
				row->width(),
				st::semiboldFont->elided(user->name(), available));
			const auto now = base::unixtime::now();
			const auto online = Data::OnlineTextActive(user, now);
			p.setPen(online ? st::windowActiveTextFg : st::windowSubTextFg);
			p.setFont(st::normalFont);
			p.drawTextLeft(
				textLeft,
				st::defaultPeerListItem.statusPosition.y(),
				row->width(),
				st::normalFont->elided(Data::OnlineText(user, now), available));
		}, row->lifetime());
		session->changes().peerUpdates(
			user,
			Data::PeerUpdate::Flag::OnlineStatus | Data::PeerUpdate::Flag::Photo
		) | rpl::on_next([=] {
			row->update();
		}, row->lifetime());
		session->downloaderTaskFinished(
		) | rpl::on_next([=] {
			row->update();
		}, row->lifetime());
		row->setClickedCallback([=] {
			box->closeBox();
			controller->showPeerHistory(
				user,
				Window::SectionShow::Way::ClearStack);
		});
		remove->setClickedCallback([=] {
			SetInList(PeerList::OnlineAlerts, user->id, false);
		});
	};

	const auto refresh = [=] {
		list->clear();
		auto count = 0;
		for (const auto &id : ListPeers(PeerList::OnlineAlerts)) {
			if (const auto user = session->data().userLoaded(peerToUser(id))) {
				addRow(user);
				++count;
			}
		}
		empty->setVisible(!count);
		list->resizeToWidth(box->width());
	};
	refresh();
	Changes() | rpl::on_next(refresh, box->lifetime());

	box->addButton(tr::lng_moligram_online_alerts_add(), [=] {
		Window::ShowChooseRecipientBox(
			controller,
			[=](not_null<Data::Thread*> thread) {
				const auto user = thread->peer()->asUser();
				if (!user || user->isSelf() || user->isBot()) {
					controller->showToast(
						tr::lng_moligram_online_alerts_users_only(tr::now));
					return false;
				}
				SetInList(PeerList::OnlineAlerts, user->id, true);
				return true;
			},
			tr::lng_moligram_online_alerts_add());
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace Moligram
