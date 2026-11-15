/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_motion.h"

#include "history/history_item.h"
#include "moligram/moligram_settings.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/ui_utility.h"
#include "styles/style_dialogs.h"
#include "styles/style_window.h"

namespace Moligram {
namespace {

constexpr auto kFadeDuration = crl::time(340);
constexpr auto kOldShift = 0.12;
constexpr auto kOldDim = 0.28;
constexpr auto kNewShift = 0.18;
constexpr auto kShadowWidth = 28;
constexpr auto kFadeDrift = 28;
constexpr auto kAppearDuration = crl::time(280);
constexpr auto kAppearShift = 16;

} // namespace

Fn<void(not_null<QWidget*>)> BeginTransition(not_null<QWidget*> from) {
	const auto parent = from->parentWidget();
	if (anim::Disabled()
		|| !parent
		|| !from->isVisible()
		|| from->width() <= 0) {
		return nullptr;
	}
	auto snapshot = Ui::GrabWidget(from);
	const auto geometry = from->geometry();
	return [=, old = std::move(snapshot)](not_null<QWidget*> to) {
		if (to->parentWidget() != parent || to->width() <= 0) {
			return;
		}
		const auto target = to->geometry();
		const auto delta = QPointF(geometry.topLeft() - target.topLeft());
		const auto fresh = Ui::GrabWidget(to);
		const auto overlay = Ui::CreateChild<Ui::RpWidget>(parent);
		overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
		overlay->setAttribute(Qt::WA_OpaquePaintEvent);
		overlay->setGeometry(target);
		const auto animation = overlay->lifetime().make_state<
			Ui::Animations::Simple>();
		overlay->paintRequest() | rpl::on_next([=] {
			const auto t = animation->value(1.);
			const auto width = float64(overlay->width());
			const auto height = float64(overlay->height());
			auto p = QPainter(overlay);
			p.setRenderHint(QPainter::SmoothPixmapTransform);
			const auto oldShift = -width * kOldShift * t;
			p.fillRect(overlay->rect(), st::windowBg);
			p.drawPixmap(delta + QPointF(oldShift, 0.), old);
			p.fillRect(
				QRectF(oldShift, 0., width, height),
				QColor(0, 0, 0, int(255 * kOldDim * t)));
			const auto newShift = width * kNewShift * (1. - t);
			const auto shadow = style::ConvertScale(kShadowWidth) * 1.;
			auto edge = QLinearGradient(newShift - shadow, 0., newShift, 0.);
			edge.setColorAt(0., QColor(0, 0, 0, 0));
			edge.setColorAt(1., QColor(0, 0, 0, int(255 * 0.22 * (1. - t))));
			p.fillRect(QRectF(newShift - shadow, 0., shadow, height), edge);
			p.setOpacity(std::min(1., 0.35 + t * 1.3));
			p.drawPixmap(QPointF(newShift, 0.), fresh);
		}, overlay->lifetime());
		overlay->show();
		overlay->raise();
		animation->start([=] {
			if (animation->animating()) {
				overlay->raise();
				overlay->update();
			} else {
				crl::on_main(overlay, [=] {
					delete overlay;
				});
			}
		}, 0., 1., kFadeDuration, anim::easeOutQuint);
	};
}

void CrossFade(not_null<Ui::RpWidget*> widget) {
	const auto parent = widget->parentWidget();
	if (anim::Disabled()
		|| !parent
		|| !widget->isVisible()
		|| widget->width() <= 0) {
		return;
	}
	const auto snapshot = Ui::GrabWidget(widget);
	const auto overlay = Ui::CreateChild<Ui::RpWidget>(parent);
	overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
	overlay->setGeometry(widget->geometry());
	const auto animation = overlay->lifetime().make_state<
		Ui::Animations::Simple>();
	overlay->paintRequest() | rpl::on_next([=] {
		const auto value = animation->value(1.);
		auto p = QPainter(overlay);
		p.setOpacity(1. - value);
		p.drawPixmap(
			QPointF(-style::ConvertScale(kFadeDrift) * value, 0.),
			snapshot);
	}, overlay->lifetime());
	overlay->show();
	overlay->raise();
	animation->start([=] {
		if (animation->animating()) {
			overlay->raise();
			overlay->update();
		} else {
			crl::on_main(overlay, [=] {
				delete overlay;
			});
		}
	}, 0., 1., kFadeDuration, anim::easeOutCubic);
}

void ApplyLayoutTweaks() {
	if (!Enabled(Option::CompactDialogs)) {
		return;
	}
	const auto shrink = [](const style::DialogRow &row, int delta) {
		auto &st = const_cast<style::DialogRow&>(row);
		const auto photo = style::ConvertScale(38);
		const auto vertical = style::ConvertScale(6);
		const auto left = st.padding.left() + photo + style::ConvertScale(12);
		st.height -= delta;
		st.padding = style::margins(
			st.padding.left(),
			vertical,
			st.padding.right(),
			vertical);
		st.photoSize = photo;
		st.nameLeft = left;
		st.textLeft = left;
		st.nameTop = style::ConvertScale(6);
		st.textTop -= style::ConvertScale(8);
		if (st.tagTop) {
			st.tagTop -= delta;
		}
	};
	const auto delta = style::ConvertScale(12);
	shrink(st::defaultDialogRow, delta);
	shrink(st::taggedDialogRow, delta);
	shrink(st::forumDialogRow, delta);
	shrink(st::taggedForumDialogRow, delta);
	const_cast<int&>(st::dialogsRowHeight) -= delta;
}

MessageAppear::MessageAppear(Fn<void()> update)
: _update(std::move(update))
, _animation([=](crl::time now) { check(now); }) {
}

int MessageAppear::Shift() {
	return style::ConvertScale(kAppearShift);
}

float64 MessageAppear::progress(not_null<const HistoryItem*> item) {
	if (!item->out()) {
		return 1.;
	}
	const auto id = item->fullId();
	const auto i = _started.find(id);
	const auto now = crl::now();
	if (i == end(_started)) {
		if (!item->isSending() || anim::Disabled()) {
			return 1.;
		}
		_started.emplace(id, now);
		if (!_animation.animating()) {
			_animation.start();
		}
		return 0.;
	}
	const auto passed = now - i->second;
	if (passed >= kAppearDuration) {
		return 1.;
	}
	return anim::easeOutCubic(1., float64(passed) / kAppearDuration);
}

void MessageAppear::check(crl::time now) {
	auto active = false;
	for (const auto &[id, started] : _started) {
		if (now - started < kAppearDuration) {
			active = true;
			break;
		}
	}
	if (!active) {
		_animation.stop();
	}
	_update();
}

} // namespace Moligram
