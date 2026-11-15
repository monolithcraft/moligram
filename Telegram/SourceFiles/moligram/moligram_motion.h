/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

#include "ui/effects/animations.h"

class HistoryItem;

namespace Ui {
class RpWidget;
} // namespace Ui

namespace Moligram {

void CrossFade(not_null<Ui::RpWidget*> widget);
[[nodiscard]] Fn<void(not_null<QWidget*>)> BeginTransition(
	not_null<QWidget*> from);
void ApplyLayoutTweaks();

class MessageAppear final {
public:
	explicit MessageAppear(Fn<void()> update);

	[[nodiscard]] float64 progress(not_null<const HistoryItem*> item);
	[[nodiscard]] static int Shift();

private:
	void check(crl::time now);

	Fn<void()> _update;
	base::flat_map<FullMsgId, crl::time> _started;
	Ui::Animations::Basic _animation;

};

} // namespace Moligram
