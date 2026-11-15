/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_hover.h"

#include "apiwrap.h"
#include "base/never_freed_pointer.h"
#include "base/timer.h"
#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_peer.h"
#include "data/data_peer_values.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "moligram/moligram_club.h"
#include "moligram/moligram_settings.h"
#include "ui/effects/animations.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/text/text.h"
#include "window/window_session_controller.h"
#include "mainwindow.h"
#include "styles/style_layers.h"
#include "styles/style_window.h"

namespace Moligram {
namespace {

constexpr auto kShowDelay = crl::time(450);
constexpr auto kHideDelay = crl::time(260);
constexpr auto kAppear = crl::time(170);
constexpr auto kWidth = 300;
constexpr auto kShadow = 16;
constexpr auto kRadius = 16;
constexpr auto kPadding = 16;
constexpr auto kUserpic = 54;
constexpr auto kGap = 10;
constexpr auto kAboutLines = 4;

class HoverCard final : public Ui::RpWidget {
public:
	HoverCard(
		not_null<Ui::RpWidget*> parent,
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer);

	[[nodiscard]] not_null<PeerData*> peer() const {
		return _peer;
	}
	void place(QRect anchor);
	void dismiss();

	Fn<void(bool)> hovered;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void enterEventHook(QEnterEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	void refresh();
	[[nodiscard]] int contentHeight() const;

	const not_null<Window::SessionController*> _controller;
	const not_null<PeerData*> _peer;
	Ui::PeerUserpicView _userpic;
	Ui::Text::String _about;
	QString _status;
	bool _online = false;
	QString _username;
	QRect _anchor;
	Ui::Animations::Simple _appear;
	bool _hiding = false;

};

HoverCard::HoverCard(
	not_null<Ui::RpWidget*> parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer)
: RpWidget(parent)
, _controller(controller)
, _peer(peer) {
	setCursor(style::cur_pointer);
	refresh();
	peer->session().changes().peerUpdates(
		peer,
		Data::PeerUpdate::Flag::About
			| Data::PeerUpdate::Flag::OnlineStatus
			| Data::PeerUpdate::Flag::Name
			| Data::PeerUpdate::Flag::Photo
	) | rpl::on_next([=] {
		refresh();
		place(_anchor);
		update();
	}, lifetime());
	controller->session().downloaderTaskFinished(
	) | rpl::on_next([=] {
		update();
	}, lifetime());
	if (_peer->about().isEmpty()) {
		_peer->session().api().requestFullPeer(_peer);
	}
	show();
	raise();
	_appear.start([=] { update(); }, 0., 1., kAppear, anim::easeOutCubic);
}

void HoverCard::refresh() {
	_about.setText(
		st::defaultTextStyle,
		StripClubMarker(_peer->about()));
	_username = _peer->username().isEmpty()
		? QString()
		: (u"@"_q + _peer->username());
	if (const auto user = _peer->asUser()) {
		const auto now = base::unixtime::now();
		_status = Data::OnlineText(user, now);
		_online = Data::OnlineTextActive(user, now);
	} else {
		_status = QString();
		_online = false;
	}
}

int HoverCard::contentHeight() const {
	const auto padding = style::ConvertScale(kPadding);
	const auto inner = style::ConvertScale(kWidth) - padding * 2;
	auto result = padding * 2 + style::ConvertScale(kUserpic);
	if (!_about.isEmpty()) {
		result += style::ConvertScale(kGap) + std::min(
			_about.countHeight(inner),
			st::defaultTextStyle.font->height * kAboutLines);
	}
	return result;
}

void HoverCard::place(QRect anchor) {
	_anchor = anchor;
	const auto pad = style::ConvertScale(kShadow);
	const auto outer = parentWidget()->size();
	const auto width = style::ConvertScale(kWidth) + pad * 2;
	const auto height = contentHeight() + pad * 2;
	auto left = anchor.x() + anchor.width() + style::ConvertScale(8) - pad;
	if (left + width > outer.width()) {
		left = anchor.x() - width + pad - style::ConvertScale(8);
	}
	auto top = anchor.y() - pad;
	top = std::clamp(top, -pad, std::max(outer.height() - height + pad, -pad));
	setGeometry(left, top, width, height);
}

void HoverCard::dismiss() {
	if (_hiding) {
		return;
	}
	_hiding = true;
	setAttribute(Qt::WA_TransparentForMouseEvents);
	_appear.stop();
	_appear.start([=] {
		update();
		if (!_appear.animating()) {
			deleteLater();
		}
	}, 1., 0., kAppear, anim::easeOutCubic);
}

void HoverCard::enterEventHook(QEnterEvent *e) {
	if (hovered) {
		hovered(true);
	}
}

void HoverCard::leaveEventHook(QEvent *e) {
	if (hovered) {
		hovered(false);
	}
}

void HoverCard::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		_controller->showPeerInfo(_peer);
		dismiss();
	}
}

void HoverCard::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	auto hq = PainterHighQualityEnabler(p);
	const auto appear = std::clamp(_appear.value(_hiding ? 0. : 1.), 0., 1.);
	if (appear <= 0.) {
		return;
	}
	p.setOpacity(appear);
	const auto pad = style::ConvertScale(kShadow);
	const auto card = QRectF(rect()).marginsRemoved({
		float64(pad),
		float64(pad),
		float64(pad),
		float64(pad) });
	const auto scale = 0.96 + 0.04 * appear;
	p.translate(card.center());
	p.scale(scale, scale);
	p.translate(-card.center());

	const auto radius = float64(style::ConvertScale(kRadius));
	p.setPen(Qt::NoPen);
	for (auto i = pad; i > 0; i -= 2) {
		const auto alpha = 0.04 * std::pow(1. - i / float64(pad + 1), 1.6);
		p.setBrush(QColor(0, 0, 0, int(255 * alpha)));
		p.drawRoundedRect(
			card.marginsAdded({ i * 0.6, i * 0.3, i * 0.6, i * 0.8 }),
			radius + i * 0.6,
			radius + i * 0.6);
	}
	p.setBrush(st::boxBg);
	p.drawRoundedRect(card, radius, radius);
	auto border = st::windowFg->c;
	border.setAlphaF(0.07);
	p.setPen(QPen(border, 1.));
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(card.marginsRemoved({ 0.5, 0.5, 0.5, 0.5 }), radius, radius);

	const auto padding = style::ConvertScale(kPadding);
	const auto userpic = style::ConvertScale(kUserpic);
	const auto left = int(card.x()) + padding;
	const auto top = int(card.y()) + padding;
	_peer->paintUserpic(p, _userpic, left, top, userpic);
	if (_online) {
		const auto dot = style::ConvertScale(14);
		const auto center = QPointF(
			left + userpic - dot / 2. + 1,
			top + userpic - dot / 2. + 1);
		p.setPen(Qt::NoPen);
		p.setBrush(st::boxBg);
		p.drawEllipse(center, dot / 2., dot / 2.);
		p.setBrush(QColor(0x22, 0xc5, 0x5e));
		p.drawEllipse(center, dot / 2. - 2.5, dot / 2. - 2.5);
	}

	const auto textLeft = left + userpic + style::ConvertScale(12);
	const auto textWidth = int(card.right()) - padding - textLeft;
	const auto &nameFont = st::boxTitle.style.font;
	const auto &font = st::normalFont;
	auto lines = 1 + (_status.isEmpty() ? 0 : 1) + (_username.isEmpty() ? 0 : 1);
	const auto block = nameFont->height + (lines - 1) * font->height;
	auto y = top + (userpic - block) / 2;
	const auto badge = ShowClubBadge(_peer);
	const auto badgeSize = nameFont->height - style::ConvertScale(4);
	const auto nameWidth = textWidth - (badge ? badgeSize + 6 : 0);
	const auto name = nameFont->elided(_peer->name(), nameWidth);
	p.setFont(nameFont);
	p.setPen(st::windowBoldFg);
	p.drawText(textLeft, y + nameFont->ascent, name);
	if (badge) {
		PaintClubBadge(
			p,
			textLeft + nameFont->width(name) + 6,
			y + (nameFont->height - badgeSize) / 2,
			badgeSize);
	}
	y += nameFont->height;
	p.setFont(font);
	if (!_status.isEmpty()) {
		p.setPen(_online ? st::windowActiveTextFg : st::windowSubTextFg);
		p.drawText(textLeft, y + font->ascent, font->elided(_status, textWidth));
		y += font->height;
	}
	if (!_username.isEmpty()) {
		p.setPen(st::windowSubTextFg);
		p.drawText(textLeft, y + font->ascent, font->elided(_username, textWidth));
	}
	if (!_about.isEmpty()) {
		p.setPen(st::windowFg);
		_about.draw(p, {
			.position = {
				left,
				top + userpic + style::ConvertScale(kGap),
			},
			.outerWidth = width(),
			.availableWidth = int(card.width()) - padding * 2,
			.elisionLines = kAboutLines,
		});
	}
}

void ShowPending();
void HideCard();

struct State {
	QPointer<HoverCard> card;
	base::weak_ptr<Window::SessionController> controller;
	PeerData *pending = nullptr;
	QRect anchor;
	bool overSource = false;
	bool overCard = false;
	base::Timer showTimer;
	base::Timer hideTimer;
};

[[nodiscard]] State &Current() {
	static auto result = base::NeverFreedPointer<State>();
	if (!result) {
		result.createIfNull();
		result->showTimer.setCallback(ShowPending);
		result->hideTimer.setCallback(HideCard);
	}
	return *result;
}

void ShowPending() {
	auto &state = Current();
	const auto controller = state.controller.get();
	if (!controller || !state.pending || !state.overSource) {
		return;
	}
	if (state.card) {
		state.card->dismiss();
	}
	const auto parent = controller->widget()->bodyWidget();
	const auto card = Ui::CreateChild<HoverCard>(
		parent,
		controller,
		not_null(state.pending));
	card->hovered = [](bool over) {
		auto &state = Current();
		state.overCard = over;
		if (over) {
			state.hideTimer.cancel();
		} else if (!state.overSource) {
			state.hideTimer.callOnce(kHideDelay);
		}
	};
	card->place(QRect(
		parent->mapFromGlobal(state.anchor.topLeft()),
		state.anchor.size()));
	state.card = card;
}

void HideCard() {
	auto &state = Current();
	if (state.overSource || state.overCard) {
		return;
	}
	if (state.card) {
		state.card->dismiss();
		state.card = nullptr;
	}
	state.pending = nullptr;
}

} // namespace

void HoverUserpic(
		not_null<Window::SessionController*> controller,
		PeerData *peer,
		QRect global) {
	auto &state = Current();
	if (!Enabled(Option::HoverProfiles) || Enabled(Option::StreamerMode)) {
		peer = nullptr;
	}
	if (!peer) {
		if (state.overSource) {
			state.overSource = false;
			state.showTimer.cancel();
			state.hideTimer.callOnce(kHideDelay);
		}
		return;
	}
	state.overSource = true;
	state.hideTimer.cancel();
	if (state.pending == peer && (state.card || state.showTimer.isActive())) {
		return;
	}
	state.pending = peer;
	state.anchor = global;
	state.controller = base::make_weak(controller);
	state.showTimer.callOnce(state.card ? kHideDelay : kShowDelay);
}

} // namespace Moligram
