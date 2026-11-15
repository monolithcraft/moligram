/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace Moligram {

void HoverUserpic(
	not_null<Window::SessionController*> controller,
	PeerData *peer,
	QRect global);

} // namespace Moligram
