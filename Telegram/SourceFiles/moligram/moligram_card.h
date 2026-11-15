/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

#include "ui/abstract_button.h"
#include "ui/effects/animations.h"

namespace Moligram {

class SettingsCard final : public Ui::AbstractButton {
public:
	explicit SettingsCard(QWidget *parent);

protected:
	void paintEvent(QPaintEvent *e) override;
	void onStateChanged(State was, StateChangeSource source) override;
	int resizeGetHeight(int newWidth) override;

private:
	void refreshCache();

	Ui::Animations::Simple _hover;
	QImage _cache;
	QColor _accent;
	QString _subtitle;

};

} // namespace Moligram
