/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

namespace Moligram {

[[nodiscard]] const QImage &Grain();
void PaintGrain(QPainter &p, const QRect &rect, float64 opacity);
[[nodiscard]] QImage PrepareFigure(float64 height, int ratio);

class Cover final {
public:
	void paint(QPainter &p, QRect rect, float64 reveal);

private:
	void refresh(QSize size);

	QImage _cache;
	QColor _accent;
	QColor _bg;

};

} // namespace Moligram
