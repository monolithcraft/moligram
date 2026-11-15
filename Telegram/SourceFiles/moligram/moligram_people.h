/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "ui/text/text.h"

class UserData;

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Moligram {

[[nodiscard]] rpl::producer<QString> NoteValue(PeerId peer);
void LocalNameBox(not_null<Ui::GenericBox*> box, not_null<UserData*> user);

class NoteCard final : public Ui::RpWidget {
public:
	NoteCard(QWidget *parent, rpl::producer<QString> note);

protected:
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	[[nodiscard]] QRect inner(int width) const;

	Ui::Text::String _text;

};

} // namespace Moligram
