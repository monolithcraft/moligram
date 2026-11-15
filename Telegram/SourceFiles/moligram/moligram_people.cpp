/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_people.h"

#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "moligram/moligram_settings.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_window.h"

namespace Moligram {
namespace {

constexpr auto kMarginX = 14;
constexpr auto kMarginY = 6;
constexpr auto kPaddingX = 14;
constexpr auto kPaddingY = 10;
constexpr auto kRadius = 14;
constexpr auto kTitleSkip = 4;
constexpr auto kNicknameLimit = 64;
constexpr auto kNoteLimit = 1024;

} // namespace

rpl::producer<QString> NoteValue(PeerId peer) {
	return rpl::single(
		rpl::empty
	) | rpl::then(
		Changes()
	) | rpl::map([=] {
		return PeerTextValue(PeerText::Note, peer);
	}) | rpl::distinct_until_changed();
}

void LocalNameBox(not_null<Ui::GenericBox*> box, not_null<UserData*> user) {
	box->setWidth(st::boxWideWidth);
	box->setNoContentMargin(true);
	const auto id = user->id;

	const auto header = box->addRow(
		object_ptr<Ui::RpWidget>(box),
		style::margins());
	const auto userpic = std::make_shared<Ui::PeerUserpicView>();
	const auto size = st::moligramLocalUserpic;
	const auto realName = langFullName(user->firstName, user->lastName);
	const auto username = user->username().isEmpty()
		? QString()
		: (u"@"_q + user->username());
	header->resize(
		st::boxWideWidth,
		style::ConvertScale(24) * 2
			+ size
			+ style::ConvertScale(12)
			+ st::boxTitle.style.font->height
			+ (username.isEmpty() ? 0 : st::normalFont->height));
	header->paintRequest() | rpl::on_next([=] {
		auto p = Painter(header);
		auto hq = PainterHighQualityEnabler(p);
		const auto width = header->width();
		auto top = style::ConvertScale(24);
		auto glow = QRadialGradient(
			QPointF(width / 2., top + size / 2.),
			size * 1.4);
		auto accent = st::windowBgActive->c;
		accent.setAlphaF(0.16);
		glow.setColorAt(0., accent);
		accent.setAlphaF(0.);
		glow.setColorAt(1., accent);
		p.fillRect(header->rect(), glow);
		user->paintUserpic(p, *userpic, (width - size) / 2, top, size);
		top += size + style::ConvertScale(12);
		const auto &title = st::boxTitle.style.font;
		p.setFont(title);
		p.setPen(st::windowBoldFg);
		p.drawText(
			QRect(0, top, width, title->height),
			Qt::AlignCenter,
			title->elided(realName, width - style::ConvertScale(48)));
		top += title->height;
		if (!username.isEmpty()) {
			p.setFont(st::normalFont);
			p.setPen(st::windowSubTextFg);
			p.drawText(
				QRect(0, top, width, st::normalFont->height),
				Qt::AlignCenter,
				username);
		}
	}, header->lifetime());

	const auto nickname = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::moligramField,
		tr::lng_moligram_local_nickname(),
		PeerTextValue(PeerText::Nickname, id)));
	nickname->setMaxLength(kNicknameLimit);
	box->addSkip(st::moligramFieldSkip);
	const auto note = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::moligramField,
		Ui::InputField::Mode::MultiLine,
		tr::lng_moligram_local_note(),
		PeerTextValue(PeerText::Note, id)));
	note->setMaxLength(kNoteLimit);
	box->addSkip(st::moligramFieldSkip);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_moligram_local_about(),
		st::boxDividerLabel));
	box->addSkip(st::moligramFieldSkip);
	box->setFocusCallback([=] { nickname->setFocusFast(); });
	const auto save = [=] {
		SetPeerText(PeerText::Nickname, id, nickname->getLastText());
		SetPeerText(PeerText::Note, id, note->getLastText());
		user->setName(
			user->firstName,
			user->lastName,
			user->nameOrPhone,
			user->editableUsername());
		box->closeBox();
	};
	nickname->submits() | rpl::on_next(save, nickname->lifetime());
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

NoteCard::NoteCard(QWidget *parent, rpl::producer<QString> note)
: RpWidget(parent) {
	std::move(note) | rpl::on_next([=](const QString &text) {
		_text.setText(st::defaultTextStyle, text);
		resizeToWidth(width());
		update();
	}, lifetime());
}

QRect NoteCard::inner(int width) const {
	return QRect(
		style::ConvertScale(kMarginX),
		style::ConvertScale(kMarginY),
		std::max(width - style::ConvertScale(kMarginX) * 2, 1),
		std::max(height() - style::ConvertScale(kMarginY) * 2, 1));
}

int NoteCard::resizeGetHeight(int newWidth) {
	const auto textWidth = std::max(
		newWidth - style::ConvertScale(kMarginX + kPaddingX) * 2,
		1);
	return style::ConvertScale(kMarginY + kPaddingY) * 2
		+ st::semiboldFont->height
		+ style::ConvertScale(kTitleSkip)
		+ _text.countHeight(textWidth);
}

void NoteCard::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	auto hq = PainterHighQualityEnabler(p);
	const auto rect = inner(width());
	const auto radius = float64(style::ConvertScale(kRadius));
	auto bg = st::windowBgActive->c;
	bg.setAlphaF(0.08);
	p.setPen(Qt::NoPen);
	p.setBrush(bg);
	p.drawRoundedRect(rect, radius, radius);

	const auto left = rect.x() + style::ConvertScale(kPaddingX);
	auto top = rect.y() + style::ConvertScale(kPaddingY);
	p.setFont(st::semiboldFont);
	p.setPen(st::windowActiveTextFg);
	p.drawText(
		left,
		top + st::semiboldFont->ascent,
		tr::lng_moligram_local_note(tr::now));
	top += st::semiboldFont->height + style::ConvertScale(kTitleSkip);
	p.setPen(st::windowFg);
	_text.draw(p, {
		.position = { left, top },
		.outerWidth = width(),
		.availableWidth = rect.width() - style::ConvertScale(kPaddingX) * 2,
	});
}

} // namespace Moligram
