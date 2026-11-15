/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

#include "moligram/moligram_settings.h"
#include "ui/effects/animations.h"
#include "ui/rp_widget.h"

namespace Moligram {

class HighlightPicker final : public Ui::RpWidget {
public:
	HighlightPicker(
		QWidget *parent,
		Highlight which,
		rpl::producer<QString> title);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	[[nodiscard]] QRectF circle(int index) const;
	[[nodiscard]] int indexAt(QPoint point) const;
	[[nodiscard]] int currentIndex() const;
	void setHovered(int index);

	const Highlight _which;
	rpl::variable<QString> _title;
	int _selected = -1;
	int _previous = -1;
	int _hovered = -1;
	int _hoveredWas = -1;
	Ui::Animations::Simple _selection;
	Ui::Animations::Simple _hover;

};

} // namespace Moligram
