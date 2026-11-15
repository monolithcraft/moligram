/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

#include "ui/effects/animations.h"
#include "ui/rp_widget.h"

namespace Moligram {

enum class Option;

class QuickPanel final : public Ui::RpWidget {
public:
	QuickPanel(QWidget *parent, Fn<void()> customize);

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	struct Tile {
		Option visibility;
		const style::icon *icon = nullptr;
		QString label;
		Fn<bool()> active;
		Fn<void()> toggle;
		QRect geometry;
		bool shownActive = false;
		Ui::Animations::Simple activeAnimation;
		Ui::Animations::Simple overAnimation;
		Ui::Animations::Simple pressAnimation;
	};

	void refresh();
	void syncStates(bool animated);
	void layoutTiles(int width);
	void setOver(int index);
	void setPressed(int index);
	void paintTile(QPainter &p, Tile &tile);
	[[nodiscard]] int tileAt(QPoint point) const;
	[[nodiscard]] bool overCustomize(QPoint point) const;

	Fn<void()> _customize;
	std::vector<std::unique_ptr<Tile>> _all;
	std::vector<int> _shown;
	QRect _customizeRect;
	bool _customizeOver = false;
	int _over = -1;
	int _pressed = -1;

};

} // namespace Moligram
