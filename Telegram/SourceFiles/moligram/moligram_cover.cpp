/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_cover.h"

#include "ui/painter.h"
#include "styles/style_window.h"

#include <random>

namespace Moligram {
namespace {

constexpr auto kGrainSide = 128;
constexpr auto kGrainSeed = 0x6d6f6cu;
constexpr auto kSweepWidth = 0.45;
constexpr auto kFigureScale = 1.25;
constexpr auto kFigureOpacity = 0.13;

[[nodiscard]] QImage GenerateGrain() {
	auto result = QImage(
		kGrainSide,
		kGrainSide,
		QImage::Format_ARGB32_Premultiplied);
	auto engine = std::mt19937(kGrainSeed);
	auto noise = std::normal_distribution<float>(0.f, 1.f);
	for (auto y = 0; y != kGrainSide; ++y) {
		auto line = reinterpret_cast<uint32*>(result.scanLine(y));
		for (auto x = 0; x != kGrainSide; ++x) {
			const auto value = std::clamp(noise(engine), -2.5f, 2.5f);
			const auto alpha = int(std::abs(value) * 22.f);
			const auto channel = (value > 0) ? alpha : 0;
			line[x] = (uint32(alpha) << 24)
				| (uint32(channel) << 16)
				| (uint32(channel) << 8)
				| uint32(channel);
		}
	}
	return result;
}

[[nodiscard]] QColor Mix(QColor a, QColor b, float64 ratio) {
	const auto inverse = 1. - ratio;
	return QColor::fromRgbF(
		a.redF() * inverse + b.redF() * ratio,
		a.greenF() * inverse + b.greenF() * ratio,
		a.blueF() * inverse + b.blueF() * ratio,
		1.);
}

} // namespace

QImage PrepareFigure(float64 height, int ratio) {
	static const auto source = QImage(u":/gui/art/moligram_mark.png"_q);
	if (source.isNull() || height <= 0) {
		return QImage();
	}
	const auto target = QSize(
		int(height * source.width() / source.height()),
		int(height)) * ratio;
	auto result = source.scaled(
		target,
		Qt::IgnoreAspectRatio,
		Qt::SmoothTransformation
	).convertToFormat(QImage::Format_ARGB32_Premultiplied);
	{
		auto p = QPainter(&result);
		p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
		auto mask = QLinearGradient(0, 0, result.width(), 0);
		mask.setColorAt(0., QColor(0, 0, 0, 0));
		mask.setColorAt(0.55, QColor(0, 0, 0, 255));
		p.fillRect(result.rect(), mask);
	}
	result.setDevicePixelRatio(ratio);
	return result;
}

const QImage &Grain() {
	static const auto result = GenerateGrain();
	return result;
}

void PaintGrain(QPainter &p, const QRect &rect, float64 opacity) {
	const auto was = p.opacity();
	p.setOpacity(was * opacity);
	static const auto pixmap = QPixmap::fromImage(Grain());
	p.drawTiledPixmap(rect, pixmap);
	p.setOpacity(was);
}

void Cover::refresh(QSize size) {
	const auto accent = st::windowBgActive->c;
	const auto bg = st::mainMenuBg->c;
	const auto ratio = style::DevicePixelRatio();
	if (_cache.size() == size * ratio && _accent == accent && _bg == bg) {
		return;
	}
	_accent = accent;
	_bg = bg;
	_cache = QImage(size * ratio, QImage::Format_ARGB32_Premultiplied);
	_cache.setDevicePixelRatio(ratio);

	const auto width = size.width();
	const auto height = size.height();
	auto p = QPainter(&_cache);
	auto hq = PainterHighQualityEnabler(p);

	const auto fg = st::windowFg->c;
	auto base = QLinearGradient(0, 0, width, height);
	base.setColorAt(0., Mix(Mix(bg, fg, 0.05), accent, 0.06));
	base.setColorAt(1., bg);
	p.fillRect(QRect(QPoint(), size), base);

	const auto figure = PrepareFigure(height * kFigureScale, ratio);
	if (!figure.isNull()) {
		const auto figureSize = figure.size() / ratio;
		auto glow = QRadialGradient(
			QPointF(width - figureSize.width() * 0.45, height * 0.42),
			height * 0.9);
		auto light = accent;
		light.setAlphaF(0.10);
		glow.setColorAt(0., light);
		light.setAlphaF(0.);
		glow.setColorAt(1., light);
		p.fillRect(QRect(QPoint(), size), glow);

		p.setOpacity(kFigureOpacity);
		p.drawImage(
			QPointF(
				width - figureSize.width() * 0.86,
				height - figureSize.height() * 0.80),
			figure);
		p.setOpacity(1.);
	}

	auto fade = QLinearGradient(0, height * 0.55, 0, height);
	auto transparent = bg;
	transparent.setAlphaF(0.);
	fade.setColorAt(0., transparent);
	fade.setColorAt(1., bg);
	p.fillRect(QRect(0, int(height * 0.55), width, height), fade);
}

void Cover::paint(QPainter &p, QRect rect, float64 reveal) {
	if (rect.isEmpty()) {
		return;
	}
	refresh(rect.size());
	p.drawImage(rect.topLeft(), _cache);
	if (reveal <= 0. || reveal >= 1.) {
		return;
	}
	const auto width = rect.width();
	const auto travel = width * (1. + kSweepWidth * 2.);
	const auto center = rect.left() - width * kSweepWidth + travel * reveal;
	const auto half = width * kSweepWidth / 2.;
	auto sweep = QLinearGradient(
		center - half,
		rect.top(),
		center + half,
		rect.bottom());
	auto shine = QColor(255, 255, 255, 0);
	sweep.setColorAt(0., shine);
	shine.setAlpha(int(18 * std::sin(reveal * M_PI)));
	sweep.setColorAt(0.5, shine);
	shine.setAlpha(0);
	sweep.setColorAt(1., shine);
	p.fillRect(rect, sweep);
}

} // namespace Moligram
