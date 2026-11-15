/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

namespace Main {
class Session;
} // namespace Main

class HistoryItem;

namespace Moligram {

void WatchOnlineAlerts(not_null<Main::Session*> session);
void NotifyDeleted(not_null<HistoryItem*> item);

} // namespace Moligram
