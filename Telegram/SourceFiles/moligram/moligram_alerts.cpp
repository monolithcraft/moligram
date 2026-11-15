/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_alerts.h"

#include "base/never_freed_pointer.h"
#include "base/timer.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_changes.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mainwindow.h"
#include "media/audio/media_audio_track.h"
#include "moligram/moligram_settings.h"
#include "ui/effects/animations.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_widgets.h"
#include "styles/style_layers.h"
#include "styles/style_dialogs.h"

namespace Moligram {
namespace {

constexpr auto kDeletedBatchDelay = crl::time(450);
constexpr auto kCardLifetime = crl::time(5200);
constexpr auto kCardAppear = crl::time(300);
constexpr auto kCardDisappear = crl::time(220);
constexpr auto kCardHover = crl::time(160);
constexpr auto kPulsePeriod = crl::time(2400);
constexpr auto kCardWidth = 340;
constexpr auto kCardHeight = 62;
constexpr auto kCardTop = 12;
constexpr auto kCardShadow = 16;
constexpr auto kCardRadius = 14;
constexpr auto kCardUserpic = 38;
constexpr auto kCardSlide = 10;

struct PendingDeletion {
	QString author;
	int count = 0;
};

class DeletionBatch final {
public:
	DeletionBatch() : _timer([=] { flush(); }) {
	}

	void add(PeerId author, const QString &name) {
		auto &entry = _pending[author];
		entry.author = name;
		++entry.count;
		_timer.callOnce(kDeletedBatchDelay);
	}

private:
	void flush() {
		const auto window = Core::App().activeWindow();
		for (const auto &[author, entry] : base::take(_pending)) {
			if (!window) {
				continue;
			}
			window->showToast(tr::lng_moligram_deleted_alert(
				tr::now,
				lt_count,
				entry.count,
				lt_user,
				tr::bold(entry.author),
				tr::marked));
		}
	}

	base::flat_map<PeerId, PendingDeletion> _pending;
	base::Timer _timer;

};

class OnlineCard final : public Ui::RpWidget {
public:
	OnlineCard(
		not_null<Ui::RpWidget*> parent,
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user);

	void dismiss();

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void enterEventHook(QEnterEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	void place(QSize outer);
	void paintShadow(QPainter &p, QRectF card);

	const not_null<Window::SessionController*> _controller;
	const not_null<UserData*> _user;
	Ui::PeerUserpicView _userpic;
	Ui::Animations::Simple _appear;
	Ui::Animations::Simple _hover;
	Ui::Animations::Basic _pulse;
	base::Timer _hideTimer;
	crl::time _started = 0;
	crl::time _pausedAt = 0;
	bool _hiding = false;

};

OnlineCard::OnlineCard(
	not_null<Ui::RpWidget*> parent,
	not_null<Window::SessionController*> controller,
	not_null<UserData*> user)
: RpWidget(parent)
, _controller(controller)
, _user(user)
, _pulse([=](crl::time) { update(); return true; })
, _hideTimer([=] { dismiss(); }) {
	setCursor(style::cur_pointer);
	parent->sizeValue(
	) | rpl::on_next([=](QSize outer) {
		place(outer);
	}, lifetime());
	controller->session().downloaderTaskFinished(
	) | rpl::on_next([=] {
		update();
	}, lifetime());
	show();
	raise();
	_started = crl::now();
	_pulse.start();
	_hideTimer.callOnce(kCardLifetime);
	_appear.start(
		[=] { update(); },
		0.,
		1.,
		kCardAppear,
		anim::easeOutCubic);
}

void OnlineCard::place(QSize outer) {
	const auto pad = style::ConvertScale(kCardShadow);
	const auto width = std::min(
		style::ConvertScale(kCardWidth),
		outer.width() - style::ConvertScale(24)) + pad * 2;
	const auto height = style::ConvertScale(kCardHeight) + pad * 2;
	setGeometry(
		(outer.width() - width) / 2,
		style::ConvertScale(kCardTop) - pad,
		width,
		height);
}

void OnlineCard::dismiss() {
	if (_hiding) {
		return;
	}
	_hiding = true;
	_hideTimer.cancel();
	setAttribute(Qt::WA_TransparentForMouseEvents);
	_appear.stop();
	_appear.start([=] {
		update();
		if (!_appear.animating()) {
			deleteLater();
		}
	}, 1., 0., kCardDisappear, anim::easeInCubic);
}

void OnlineCard::enterEventHook(QEnterEvent *e) {
	_pausedAt = crl::now();
	_hideTimer.cancel();
	_hover.start([=] { update(); }, 0., 1., kCardHover, anim::easeOutCubic);
}

void OnlineCard::leaveEventHook(QEvent *e) {
	if (_pausedAt) {
		_started += crl::now() - _pausedAt;
		_pausedAt = 0;
	}
	const auto left = kCardLifetime - (crl::now() - _started);
	_hideTimer.callOnce(std::max(left, crl::time(600)));
	_hover.start([=] { update(); }, 1., 0., kCardHover, anim::easeOutCubic);
}

void OnlineCard::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		_controller->showPeerHistory(
			_user,
			Window::SectionShow::Way::ClearStack);
	}
	dismiss();
}

void OnlineCard::paintShadow(QPainter &p, QRectF card) {
	const auto pad = style::ConvertScale(kCardShadow);
	const auto radius = style::ConvertScale(kCardRadius);
	p.setPen(Qt::NoPen);
	for (auto i = pad; i > 0; i -= 2) {
		const auto alpha = 0.035 * std::pow(1. - i / float64(pad + 1), 1.6);
		p.setBrush(QColor(0, 0, 0, int(255 * alpha)));
		const auto shift = i * 0.35;
		p.drawRoundedRect(
			card.marginsAdded({ i * 0.6, i * 0.4 - shift, i * 0.6, i * 0.6 + shift }),
			radius + i * 0.6,
			radius + i * 0.6);
	}
}

void OnlineCard::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);
	const auto appear = std::clamp(_appear.value(_hiding ? 0. : 1.), 0., 1.);
	if (appear <= 0.) {
		return;
	}
	const auto hover = _hover.value(_pausedAt ? 1. : 0.);
	const auto pad = float64(style::ConvertScale(kCardShadow));
	const auto radius = style::ConvertScale(kCardRadius);
	const auto card = QRectF(rect()).marginsRemoved({ pad, pad, pad, pad });

	p.setOpacity(appear);
	p.translate(0, -(1. - appear) * style::ConvertScale(kCardSlide));

	paintShadow(p, card);

	p.setPen(Qt::NoPen);
	p.setBrush(anim::color(st::boxBg, st::windowBgOver, hover * 0.5));
	p.drawRoundedRect(card, radius, radius);
	auto border = st::windowFg->c;
	border.setAlphaF(0.07);
	p.setPen(QPen(border, 1.));
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(
		card.marginsRemoved({ 0.5, 0.5, 0.5, 0.5 }),
		radius - 0.5,
		radius - 0.5);

	const auto size = style::ConvertScale(kCardUserpic);
	const auto skip = style::ConvertScale(12);
	const auto userpic = QRectF(
		card.left() + skip,
		card.top() + (card.height() - size) / 2.,
		size,
		size);
	_user->paintUserpic(
		p,
		_userpic,
		int(userpic.x()),
		int(userpic.y()),
		size);

	const auto online = st::dialogsOnlineBadgeFg->c;
	const auto phase = (crl::now() % kPulsePeriod) / float64(kPulsePeriod);
	const auto breathe = 0.5 - 0.5 * std::cos(phase * 2. * M_PI);
	const auto dot = style::ConvertScale(11) * (0.92 + 0.08 * breathe);
	const auto dotCenter = QPointF(
		userpic.right() - style::ConvertScale(4),
		userpic.bottom() - style::ConvertScale(4));
	auto aura = online;
	aura.setAlphaF(0.25 * breathe);
	p.setPen(Qt::NoPen);
	p.setBrush(aura);
	const auto auraRadius = dot / 2. + style::ConvertScale(3) * breathe;
	p.drawEllipse(dotCenter, auraRadius, auraRadius);
	p.setPen(QPen(st::boxBg, style::ConvertScale(2)));
	p.setBrush(online);
	p.drawEllipse(dotCenter, dot / 2., dot / 2.);

	const auto &actionFont = st::semiboldFont;
	const auto action = tr::lng_moligram_online_open(tr::now);
	const auto actionPadding = style::ConvertScale(12);
	const auto actionHeight = style::ConvertScale(28);
	const auto actionWidth = actionFont->width(action) + actionPadding * 2;
	const auto actionRect = QRectF(
		card.right() - skip - actionWidth,
		card.top() + (card.height() - actionHeight) / 2.,
		actionWidth,
		actionHeight);
	auto actionBg = st::windowBgActive->c;
	actionBg.setAlphaF(0.14 + 0.08 * hover);
	p.setPen(Qt::NoPen);
	p.setBrush(actionBg);
	p.drawRoundedRect(actionRect, actionHeight / 2., actionHeight / 2.);
	p.setPen(st::windowActiveTextFg);
	p.setFont(actionFont);
	p.drawText(actionRect, Qt::AlignCenter, action);

	const auto textLeft = userpic.right() + skip;
	const auto textWidth = int(actionRect.left() - skip - textLeft);
	const auto &nameFont = st::semiboldFont;
	const auto &subFont = st::normalFont;
	const auto gap = style::ConvertScale(2);
	const auto block = nameFont->height + gap + subFont->height;
	const auto top = card.top() + (card.height() - block) / 2.;
	p.setPen(st::windowBoldFg);
	p.setFont(nameFont);
	p.drawText(
		QPointF(textLeft, top + nameFont->ascent),
		nameFont->elided(_user->name(), textWidth));
	p.setPen(st::windowSubTextFg);
	p.setFont(subFont);
	p.drawText(
		QPointF(textLeft, top + nameFont->height + gap + subFont->ascent),
		subFont->elided(tr::lng_moligram_online_now(tr::now), textWidth));
}

[[nodiscard]] QPointer<OnlineCard> &CurrentCard() {
	static auto result = QPointer<OnlineCard>();
	return result;
}

void ShowOnlineCard(
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user) {
	auto &current = CurrentCard();
	if (current) {
		current->dismiss();
	}
	const auto parent = controller->widget()->bodyWidget();
	current = Ui::CreateChild<OnlineCard>(parent, controller, user);
}

} // namespace

void NotifyDeleted(not_null<HistoryItem*> item) {
	if (!Enabled(Option::DeletionAlerts)
		|| Enabled(Option::StreamerMode)
		|| item->out()
		|| !item->isRegular()
		|| item->history()->peer->isBroadcast()) {
		return;
	}
	static auto batch = base::NeverFreedPointer<DeletionBatch>();
	batch.createIfNull();
	const auto from = item->from();
	batch->add(from->id, from->name());
}

void WatchOnlineAlerts(not_null<Main::Session*> session) {
	struct State {
		base::flat_map<PeerId, bool> online;
		std::unique_ptr<Media::Audio::Track> sound;
	};
	const auto state = session->lifetime().make_state<State>();
	session->changes().peerUpdates(
		Data::PeerUpdate::Flag::OnlineStatus
	) | rpl::on_next([=](const Data::PeerUpdate &update) {
		const auto user = update.peer->asUser();
		if (!user || !InList(PeerList::OnlineAlerts, user->id)) {
			return;
		}
		const auto online = user->lastseen().isOnline(base::unixtime::now());
		auto &was = state->online[user->id];
		const auto cameOnline = online && !was;
		was = online;
		if (!cameOnline) {
			return;
		}
		const auto window = Core::App().activeWindow();
		const auto controller = window ? window->sessionController() : nullptr;
		if (!controller || &controller->session() != session) {
			return;
		}
		ShowOnlineCard(controller, user);
		if (Core::App().settings().soundNotify()) {
			if (!state->sound) {
				state->sound = Media::Audio::Current().createTrack();
				state->sound->fillFromFile(u":/sounds/moligram_online.mp3"_q);
			}
			state->sound->playOnce();
		}
	}, session->lifetime());
}

} // namespace Moligram
