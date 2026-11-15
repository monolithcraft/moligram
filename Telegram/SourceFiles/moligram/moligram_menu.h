/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

class HistoryItem;
class PeerData;
class DocumentData;

namespace Ui {
class GenericBox;
class PopupMenu;
class Show;
namespace Menu {
struct MenuCallback;
} // namespace Menu
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Moligram {

[[nodiscard]] QString PeerIdText(not_null<PeerData*> peer);

void AddMessageMenuActions(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);

[[nodiscard]] bool ConfirmDocumentSending(
	std::shared_ptr<Ui::Show> show,
	not_null<DocumentData*> document,
	Fn<void()> send);

void FilterWordsBox(not_null<Ui::GenericBox*> box);

void FillPeerMenu(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	const Ui::Menu::MenuCallback &addAction);
void AddOnlineAlertAction(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	const Ui::Menu::MenuCallback &addAction);

void OnlineAlertsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller);

} // namespace Moligram
