/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_palette.h"

#include "ui/painter.h"
#include "styles/style_settings.h"

namespace Moligram {
namespace {

constexpr auto kHeight = 52;
constexpr auto kDiameter = 22;
constexpr auto kGap = 9;
constexpr auto kRing = 4;
constexpr auto kSelectDuration = crl::time(260);
constexpr auto kHoverDuration = crl::time(150);
constexpr auto kHoverGrow = 0.14;

} // namespace

HighlightPicker::HighlightPicker(
	QWidget *parent,
	Highlight which,
	rpl::producer<QString> title)
: RpWidget(parent)
, _which(which)
, _title(std::move(title))
, _selected(currentIndex()) {
	setMouseTracking(true);
	setCursor(style::cur_pointer);
	_title.changes() | rpl::on_next([=] {
		update();
	}, lifetime());
	resize(width(), style::ConvertScale(kHeight));
}

int HighlightPicker::resizeGetHeight(int newWidth) {
	return style::ConvertScale(kHeight);
}

int HighlightPicker::currentIndex() const {
	const auto &palette = HighlightPalette();
	const auto color = HighlightColor(_which);
	const auto i = ranges::find(palette, color);
	return (i != end(palette)) ? int(i - begin(palette)) : -1;
}

QRectF HighlightPicker::circle(int index) const {
	const auto count = int(HighlightPalette().size());
	const auto diameter = style::ConvertScale(kDiameter);
	const auto gap = style::ConvertScale(kGap);
	const auto right = width() - st::settingsButtonNoIcon.padding.right();
	const auto left = right
		- count * diameter
		- (count - 1) * gap
		+ index * (diameter + gap);
	return QRectF(left, (height() - diameter) / 2., diameter, diameter);
}

int HighlightPicker::indexAt(QPoint point) const {
	const auto extra = style::ConvertScale(kGap) / 2.;
	const auto count = int(HighlightPalette().size());
	for (auto i = 0; i != count; ++i) {
		if (circle(i).marginsAdded({ extra, extra, extra, extra }).contains(point)) {
			return i;
		}
	}
	return -1;
}

void HighlightPicker::setHovered(int index) {
	if (_hovered == index) {
		return;
	}
	_hoveredWas = _hovered;
	_hovered = index;
	_hover.stop();
	_hover.start([=] { update(); }, 0., 1., kHoverDuration, anim::easeOutCubic);
}

void HighlightPicker::mouseMoveEvent(QMouseEvent *e) {
	setHovered(indexAt(e->pos()));
}

void HighlightPicker::leaveEventHook(QEvent *e) {
	setHovered(-1);
}

void HighlightPicker::mousePressEvent(QMouseEvent *e) {
	const auto index = indexAt(e->pos());
	if (index < 0 || index == _selected) {
		return;
	}
	_previous = _selected;
	_selected = index;
	SetHighlightColor(_which, HighlightPalette()[index]);
	_selection.stop();
	_selection.start(
		[=] { update(); },
		0.,
		1.,
		kSelectDuration,
		anim::easeOutBack);
}

void HighlightPicker::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);

	const auto &padding = st::settingsButtonNoIcon.padding;
	p.setFont(st::settingsButtonNoIcon.style.font);
	p.setPen(st::settingsButtonNoIcon.textFg);
	const auto textRight = circle(0).left() - style::ConvertScale(kGap);
	p.drawText(
		QRect(padding.left(), 0, int(textRight) - padding.left(), height()),
		Qt::AlignLeft | Qt::AlignVCenter,
		_title.current());

	const auto selection = _selection.value(1.);
	const auto hover = _hover.value(1.);
	const auto &palette = HighlightPalette();
	const auto ring = style::ConvertScale(kRing);
	for (auto i = 0; i != int(palette.size()); ++i) {
		const auto grow = (i == _hovered)
			? hover
			: (i == _hoveredWas)
			? (1. - hover)
			: 0.;
		auto rect = circle(i);
		const auto center = rect.center();
		const auto scale = 1. + kHoverGrow * grow;
		const auto radius = rect.width() / 2. * scale;

		const auto selected = (i == _selected)
			? selection
			: (i == _previous)
			? (1. - std::min(selection, 1.))
			: 0.;
		const auto color = palette[i];
		if (selected > 0.) {
			auto halo = color;
			halo.setAlphaF(0.35 * std::clamp(selected, 0., 1.));
			p.setPen(Qt::NoPen);
			p.setBrush(halo);
			const auto outer = radius + ring * selected;
			p.drawEllipse(center, outer, outer);
			p.setBrush(st::windowBg);
			const auto cut = radius + ring * 0.45 * selected;
			p.drawEllipse(center, cut, cut);
		}
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawEllipse(center, radius, radius);
		if (selected > 0.) {
			p.setBrush(QColor(255, 255, 255, int(235 * std::clamp(selected, 0., 1.))));
			const auto dot = radius * 0.32 * selected;
			p.drawEllipse(center, dot, dot);
		}
	}
}

} // namespace Moligram
