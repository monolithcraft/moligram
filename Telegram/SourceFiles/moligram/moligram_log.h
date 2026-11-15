/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

class History;
class HistoryItem;

namespace Window {
class SessionController;
} // namespace Window

namespace Moligram {

void ShowDeletedLog(
	not_null<Window::SessionController*> controller,
	not_null<History*> history);
void ShowEditLog(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

} // namespace Moligram
