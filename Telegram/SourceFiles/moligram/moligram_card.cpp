/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_card.h"

#include "lang/lang_keys.h"
#include "moligram/moligram_cover.h"
#include "ui/painter.h"
#include "styles/style_layers.h"
#include "styles/style_window.h"

namespace Moligram {
namespace {

constexpr auto kHeight = 74;
constexpr auto kMarginX = 14;
constexpr auto kMarginY = 4;
constexpr auto kRadius = 16;
constexpr auto kLogoSize = 44;
constexpr auto kHoverDuration = crl::time(160);

[[nodiscard]] QColor Mix(QColor a, QColor b, float64 ratio) {
	const auto inverse = 1. - ratio;
	return QColor::fromRgbF(
		a.redF() * inverse + b.redF() * ratio,
		a.greenF() * inverse + b.greenF() * ratio,
		a.blueF() * inverse + b.blueF() * ratio,
		1.);
}

[[nodiscard]] const QImage &Logo() {
	static const auto result = [] {
		const auto ratio = style::DevicePixelRatio();
		const auto size = style::ConvertScale(kLogoSize) * ratio;
		auto image = QImage(u":/gui/art/logo_256_no_margin.png"_q).scaled(
			size,
			size,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation);
		image.setDevicePixelRatio(ratio);
		return image;
	}();
	return result;
}

} // namespace

SettingsCard::SettingsCard(QWidget *parent)
: AbstractButton(parent)
, _subtitle(tr::lng_moligram_settings_card(tr::now)) {
	setCursor(style::cur_pointer);
	resize(width(), style::ConvertScale(kHeight + kMarginY * 2));
}

int SettingsCard::resizeGetHeight(int newWidth) {
	return style::ConvertScale(kHeight + kMarginY * 2);
}

void SettingsCard::onStateChanged(State was, StateChangeSource source) {
	const auto over = isOver();
	if (over != bool(was & StateFlag::Over)) {
		_hover.start(
			[=] { update(); },
			over ? 0. : 1.,
			over ? 1. : 0.,
			kHoverDuration,
			anim::easeOutCubic);
	}
}

void SettingsCard::refreshCache() {
	const auto ratio = style::DevicePixelRatio();
	const auto inner = rect().marginsRemoved(QMargins(
		style::ConvertScale(kMarginX),
		style::ConvertScale(kMarginY),
		style::ConvertScale(kMarginX),
		style::ConvertScale(kMarginY)));
	const auto accent = st::windowBgActive->c;
	if (_cache.size() == inner.size() * ratio && _accent == accent) {
		return;
	}
	_accent = accent;
	_cache = QImage(inner.size() * ratio, QImage::Format_ARGB32_Premultiplied);
	_cache.setDevicePixelRatio(ratio);
	_cache.fill(Qt::transparent);

	const auto size = inner.size();
	const auto radius = float64(style::ConvertScale(kRadius));
	auto p = QPainter(&_cache);
	auto hq = PainterHighQualityEnabler(p);
	auto clip = QPainterPath();
	clip.addRoundedRect(QRectF(QPointF(), QSizeF(size)), radius, radius);
	p.setClipPath(clip);

	const auto bg = st::windowBg->c;
	const auto fg = st::windowFg->c;
	auto base = QLinearGradient(0, 0, size.width(), size.height());
	base.setColorAt(0., Mix(Mix(bg, fg, 0.06), accent, 0.16));
	base.setColorAt(1., Mix(bg, fg, 0.035));
	p.fillRect(QRect(QPoint(), size), base);

	auto glow = QRadialGradient(
		QPointF(size.width() * 0.08, size.height() * 0.5),
		size.height() * 1.6);
	auto light = accent;
	light.setAlphaF(0.20);
	glow.setColorAt(0., light);
	light.setAlphaF(0.);
	glow.setColorAt(1., light);
	p.fillRect(QRect(QPoint(), size), glow);

	const auto figure = PrepareFigure(size.height() * 1.9, ratio);
	if (!figure.isNull()) {
		const auto figureSize = figure.size() / ratio;
		p.setOpacity(0.16);
		p.drawImage(
			QPointF(
				size.width() - figureSize.width() * 0.78,
				(size.height() - figureSize.height()) / 2.),
			figure);
		p.setOpacity(1.);
	}

	p.setClipping(false);
	auto border = accent;
	border.setAlphaF(0.28);
	p.setPen(QPen(border, 1.));
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(
		QRectF(QPointF(), QSizeF(size)).marginsRemoved(
			QMarginsF(0.5, 0.5, 0.5, 0.5)),
		radius - 0.5,
		radius - 0.5);
}

void SettingsCard::paintEvent(QPaintEvent *e) {
	refreshCache();
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);
	const auto inner = rect().marginsRemoved(QMargins(
		style::ConvertScale(kMarginX),
		style::ConvertScale(kMarginY),
		style::ConvertScale(kMarginX),
		style::ConvertScale(kMarginY)));
	p.drawImage(inner.topLeft(), _cache);

	const auto hover = _hover.value(isOver() ? 1. : 0.);
	if (hover > 0.) {
		const auto radius = float64(style::ConvertScale(kRadius));
		auto over = st::windowFg->c;
		over.setAlphaF(0.04 * hover);
		p.setPen(Qt::NoPen);
		p.setBrush(over);
		p.drawRoundedRect(QRectF(inner), radius, radius);
	}

	const auto logo = style::ConvertScale(kLogoSize);
	const auto padding = style::ConvertScale(16);
	const auto logoTop = inner.y() + (inner.height() - logo) / 2;
	p.drawImage(QPoint(inner.x() + padding, logoTop), Logo());

	const auto textLeft = inner.x() + padding + logo + style::ConvertScale(14);
	const auto chevron = style::ConvertScale(7);
	const auto textRight = inner.x() + inner.width()
		- padding
		- chevron
		- style::ConvertScale(12);
	const auto available = std::max(textRight - textLeft, 1);
	const auto &titleFont = st::boxTitle.style.font;
	const auto &subFont = st::normalFont;
	const auto gap = style::ConvertScale(3);
	const auto block = titleFont->height + gap + subFont->height;
	const auto top = inner.y() + (inner.height() - block) / 2;

	p.setFont(titleFont);
	p.setPen(st::windowBoldFg);
	p.drawText(textLeft, top + titleFont->ascent, u"Moligram"_q);

	p.setFont(subFont);
	p.setPen(st::windowSubTextFg);
	p.drawText(
		textLeft,
		top + titleFont->height + gap + subFont->ascent,
		subFont->elided(_subtitle, available));

	const auto shift = style::ConvertScale(3) * hover;
	const auto cx = textRight + style::ConvertScale(12) + shift;
	const auto cy = inner.y() + inner.height() / 2.;
	auto pen = QPen(st::windowSubTextFg->c, style::ConvertScale(2) * 1.);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	const auto half = chevron / 2. + 1.;
	auto path = QPainterPath();
	path.moveTo(cx, cy - half * 1.6);
	path.lineTo(cx + half, cy);
	path.lineTo(cx, cy + half * 1.6);
	p.drawPath(path);
}

} // namespace Moligram
