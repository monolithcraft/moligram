/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_quick_panel.h"

#include "lang/lang_keys.h"
#include "moligram/moligram_settings.h"
#include "ui/painter.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h"

namespace Moligram {
namespace {

constexpr auto kMaxColumns = 3;

[[nodiscard]] int Columns(int count) {
	return (count == 4) ? 2 : std::clamp(count, 1, kMaxColumns);
}
constexpr auto kTileHeight = 66;
constexpr auto kGap = 8;
constexpr auto kSidePadding = 14;
constexpr auto kHeaderHeight = 30;
constexpr auto kBottomPadding = 12;
constexpr auto kRadius = 12.;
constexpr auto kBadge = 30;
constexpr auto kToggleDuration = crl::time(220);
constexpr auto kHoverDuration = crl::time(140);
constexpr auto kPressDuration = crl::time(110);
constexpr auto kPressedScale = 0.94;

[[nodiscard]] Fn<bool()> Reader(Option option) {
	return [=] { return Enabled(option); };
}

[[nodiscard]] Fn<void()> Toggler(Option option) {
	return [=] { Set(option, !Enabled(option)); };
}

[[nodiscard]] QColor Mix(const QColor &a, const QColor &b, float64 ratio) {
	return anim::color(a, b, ratio);
}

} // namespace

QuickPanel::QuickPanel(QWidget *parent, Fn<void()> customize)
: RpWidget(parent)
, _customize(std::move(customize)) {
	const auto add = [&](
			Option visibility,
			const style::icon &icon,
			QString label,
			Fn<bool()> active,
			Fn<void()> toggle) {
		auto tile = std::make_unique<Tile>();
		tile->visibility = visibility;
		tile->icon = &icon;
		tile->label = std::move(label);
		tile->active = std::move(active);
		tile->toggle = std::move(toggle);
		tile->shownActive = tile->active();
		_all.push_back(std::move(tile));
	};
	add(
		Option::QuickGhost,
		st::menuIconStealth,
		tr::lng_moligram_quick_ghost(tr::now),
		[] { return GhostMode(); },
		[] { SetGhostMode(!GhostMode()); });
	add(
		Option::QuickStreamer,
		st::menuIconUserHide,
		tr::lng_moligram_quick_streamer(tr::now),
		Reader(Option::StreamerMode),
		Toggler(Option::StreamerMode));
	add(
		Option::QuickDnd,
		st::menuIconMute,
		tr::lng_moligram_quick_dnd(tr::now),
		Reader(Option::DoNotDisturb),
		Toggler(Option::DoNotDisturb));
	add(
		Option::QuickHideStories,
		st::menuIconStoriesArchive,
		tr::lng_moligram_quick_stories(tr::now),
		Reader(Option::HideStories),
		Toggler(Option::HideStories));
	add(
		Option::QuickKeepDeleted,
		st::menuIconDelete,
		tr::lng_moligram_quick_deleted(tr::now),
		Reader(Option::KeepDeletedMessages),
		Toggler(Option::KeepDeletedMessages));
	add(
		Option::QuickKeepEdits,
		st::menuIconEdit,
		tr::lng_moligram_quick_edits(tr::now),
		Reader(Option::KeepEditHistory),
		Toggler(Option::KeepEditHistory));
	add(
		Option::QuickSeconds,
		st::menuIconSchedule,
		tr::lng_moligram_quick_seconds(tr::now),
		Reader(Option::ShowMessageSeconds),
		Toggler(Option::ShowMessageSeconds));
	add(
		Option::QuickPeerIds,
		st::menuIconInfo,
		tr::lng_moligram_quick_ids(tr::now),
		Reader(Option::ShowPeerIds),
		Toggler(Option::ShowPeerIds));
	add(
		Option::QuickSilent,
		st::menuIconSilent,
		tr::lng_moligram_quick_silent(tr::now),
		Reader(Option::SilentSending),
		Toggler(Option::SilentSending));

	setMouseTracking(true);
	refresh();
	Changes() | rpl::on_next([=] {
		refresh();
	}, lifetime());
}

void QuickPanel::refresh() {
	_shown.clear();
	for (auto i = 0, count = int(_all.size()); i != count; ++i) {
		if (Enabled(_all[i]->visibility)) {
			_shown.push_back(i);
		}
	}
	if (width() > 0) {
		resizeToWidth(width());
	}
	syncStates(true);
	update();
}

void QuickPanel::syncStates(bool animated) {
	for (const auto &tile : _all) {
		const auto active = tile->active();
		if (tile->shownActive == active) {
			continue;
		}
		tile->shownActive = active;
		if (animated) {
			tile->activeAnimation.start(
				[=] { update(); },
				active ? 0. : 1.,
				active ? 1. : 0.,
				kToggleDuration,
				anim::easeOutCubic);
		}
	}
}

void QuickPanel::layoutTiles(int width) {
	const auto inner = width - kSidePadding * 2;
	const auto columns = Columns(int(_shown.size()));
	const auto tileWidth = (inner - kGap * (columns - 1)) / columns;
	for (auto i = 0, count = int(_shown.size()); i != count; ++i) {
		const auto column = i % columns;
		const auto row = i / columns;
		_all[_shown[i]]->geometry = QRect(
			kSidePadding + column * (tileWidth + kGap),
			kHeaderHeight + row * (kTileHeight + kGap),
			tileWidth,
			kTileHeight);
	}
	const auto &gear = st::menuIconSettings;
	_customizeRect = QRect(
		width - kSidePadding - gear.width() - 6,
		(kHeaderHeight - gear.height()) / 2 - 2,
		gear.width() + 6,
		gear.height() + 4);
}

int QuickPanel::resizeGetHeight(int newWidth) {
	layoutTiles(newWidth);
	const auto columns = Columns(int(_shown.size()));
	const auto rows = (int(_shown.size()) + columns - 1) / columns;
	return kHeaderHeight
		+ rows * kTileHeight
		+ std::max(rows - 1, 0) * kGap
		+ kBottomPadding;
}

int QuickPanel::tileAt(QPoint point) const {
	for (const auto index : _shown) {
		if (_all[index]->geometry.contains(point)) {
			return index;
		}
	}
	return -1;
}

bool QuickPanel::overCustomize(QPoint point) const {
	return _customizeRect.contains(point);
}

void QuickPanel::paintTile(QPainter &p, Tile &tile) {
	const auto active = tile.activeAnimation.value(tile.shownActive ? 1. : 0.);
	const auto over = tile.overAnimation.value(
		(&tile == (_over >= 0 ? _all[_over].get() : nullptr)) ? 1. : 0.);
	const auto pressed = tile.pressAnimation.value(
		(&tile == (_pressed >= 0 ? _all[_pressed].get() : nullptr))
			? 1.
			: 0.);

	const auto rect = QRectF(tile.geometry);
	const auto scale = 1. - (1. - kPressedScale) * pressed;
	p.save();
	p.translate(rect.center());
	p.scale(scale, scale);
	p.translate(-rect.center());

	const auto idle = Mix(st::windowBgOver->c, st::windowBgRipple->c, over);
	const auto lit = Mix(idle, st::windowFg->c, 0.05);
	p.setPen(Qt::NoPen);
	p.setBrush(Mix(idle, lit, active));
	p.drawRoundedRect(rect, kRadius, kRadius);

	const auto accent = st::windowActiveTextFg->c;
	auto border = Mix(st::windowFg->c, accent, active);
	border.setAlphaF(0.06 + 0.42 * active);
	p.setPen(QPen(border, 1.));
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);

	if (active > 0.) {
		auto glow = QRadialGradient(
			QPointF(rect.center().x(), rect.top()),
			rect.width() * 0.6);
		auto tint = accent;
		tint.setAlphaF(0.10 * active);
		glow.setColorAt(0., tint);
		tint.setAlphaF(0.);
		glow.setColorAt(1., tint);
		auto clip = QPainterPath();
		clip.addRoundedRect(rect, kRadius, kRadius);
		p.save();
		p.setClipPath(clip);
		p.fillRect(rect, glow);
		p.restore();

		const auto dot = 5. * active;
		p.setPen(Qt::NoPen);
		p.setBrush(accent);
		p.drawEllipse(
			QPointF(rect.right() - 10., rect.top() + 10.),
			dot / 2.,
			dot / 2.);
	}

	const auto badge = QRectF(
		rect.x() + (rect.width() - kBadge) / 2.,
		rect.y() + 8,
		kBadge,
		kBadge);
	const auto foreground = Mix(
		Mix(st::windowSubTextFg->c, st::windowFg->c, over * 0.6),
		accent,
		active);
	const auto icon = tile.icon;
	icon->paint(
		p,
		QPoint(
			int(badge.x() + (badge.width() - icon->width()) / 2.),
			int(badge.y() + (badge.height() - icon->height()) / 2.)),
		width(),
		foreground);

	const auto &font = st::normalFont;
	p.setFont(font);
	p.setPen(Mix(st::windowSubTextFg->c, st::windowBoldFg->c, std::max(active, over * 0.6)));
	p.drawText(
		QRectF(
			rect.x() + 4,
			rect.y() + rect.height() - font->height - 7,
			rect.width() - 8,
			font->height),
		Qt::AlignCenter,
		font->elided(tile.label, int(rect.width()) - 8));
	p.restore();
}

void QuickPanel::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);

	const auto &header = st::semiboldFont;
	p.setFont(header);
	p.setPen(st::windowSubTextFg);
	p.drawText(
		QRect(kSidePadding + 2, 0, width(), kHeaderHeight - 6),
		Qt::AlignLeft | Qt::AlignVCenter,
		tr::lng_moligram_quick_header(tr::now));

	const auto &gear = st::menuIconSettings;
	gear.paint(
		p,
		QPoint(
			_customizeRect.x() + (_customizeRect.width() - gear.width()) / 2,
			_customizeRect.y() + (_customizeRect.height() - gear.height()) / 2),
		width(),
		_customizeOver ? st::windowFg->c : st::windowSubTextFg->c);

	for (const auto index : _shown) {
		paintTile(p, *_all[index]);
	}
}

void QuickPanel::setOver(int index) {
	if (_over == index) {
		return;
	}
	const auto was = _over;
	_over = index;
	const auto animate = [&](int i, bool now) {
		if (i >= 0) {
			_all[i]->overAnimation.start(
				[=] { update(); },
				now ? 0. : 1.,
				now ? 1. : 0.,
				kHoverDuration);
		}
	};
	animate(was, false);
	animate(index, true);
	setCursor((index >= 0 || _customizeOver)
		? style::cur_pointer
		: style::cur_default);
}

void QuickPanel::setPressed(int index) {
	if (_pressed == index) {
		return;
	}
	const auto was = _pressed;
	_pressed = index;
	if (was >= 0) {
		_all[was]->pressAnimation.start(
			[=] { update(); },
			1.,
			0.,
			kPressDuration * 2,
			anim::easeOutBack);
	}
	if (index >= 0) {
		_all[index]->pressAnimation.start(
			[=] { update(); },
			0.,
			1.,
			kPressDuration);
	}
}

void QuickPanel::mouseMoveEvent(QMouseEvent *e) {
	const auto customize = overCustomize(e->pos());
	if (_customizeOver != customize) {
		_customizeOver = customize;
		update(_customizeRect);
	}
	setOver(tileAt(e->pos()));
}

void QuickPanel::mousePressEvent(QMouseEvent *e) {
	setPressed(tileAt(e->pos()));
}

void QuickPanel::mouseReleaseEvent(QMouseEvent *e) {
	const auto pressed = _pressed;
	setPressed(-1);
	if (pressed >= 0 && pressed == tileAt(e->pos())) {
		_all[pressed]->toggle();
		syncStates(true);
		update();
	} else if (pressed < 0 && overCustomize(e->pos()) && _customize) {
		_customize();
	}
}

void QuickPanel::leaveEventHook(QEvent *e) {
	_customizeOver = false;
	setOver(-1);
	setPressed(-1);
	update();
}

} // namespace Moligram
