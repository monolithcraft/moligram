/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "ui/effects/animations.h"
#include "ui/rp_widget.h"

class UserData;
class PhotoData;

namespace Data {
class PhotoMedia;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace Moligram {

inline constexpr auto kClubUsername = "moligram_club";
inline constexpr auto kBannerColors = 100;
inline constexpr auto kBannerImage = 200;

struct ClubCard {
	int banner = 0;
	QColor from;
	QColor to;
	MsgId image = 0;
	UserId owner = 0;
	QString status;

	friend inline bool operator==(
		const ClubCard &,
		const ClubCard &) = default;
};

struct BannerPreset {
	QColor from;
	QColor to;
};

[[nodiscard]] const std::vector<BannerPreset> &BannerPresets();
[[nodiscard]] const BannerPreset &BannerAt(int index);

void AttachClub(not_null<Main::Session*> session);
[[nodiscard]] bool ShowClubBadge(PeerData *peer);
[[nodiscard]] QString StripClubMarker(QString about);
void PaintClubBadge(QPainter &p, int x, int y, int size);
[[nodiscard]] rpl::producer<std::optional<ClubCard>> ClubCardValue(
	not_null<UserData*> user);
[[nodiscard]] rpl::producer<ClubCard> OwnClubCardValue(
	not_null<Main::Session*> session);
void UploadClubImage(
	not_null<Main::Session*> session,
	QImage image,
	Fn<void(bool)> done);

class BannerCard final : public Ui::RpWidget {
public:
	BannerCard(
		QWidget *parent,
		not_null<Main::Session*> session,
		rpl::producer<ClubCard> card);

protected:
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	void shine();
	void setCard(ClubCard card);
	void paintBackground(QPainter &p, QSize size);

	const not_null<Main::Session*> _session;
	ClubCard _card;
	PhotoData *_photo = nullptr;
	std::shared_ptr<Data::PhotoMedia> _media;
	rpl::lifetime _photoLifetime;
	QImage _cache;
	bool _cacheValid = false;
	bool _cacheWithImage = false;
	Ui::Animations::Simple _shine;
	base::Timer _shineTimer;

};

class ClubLine final : public Ui::RpWidget {
public:
	ClubLine(QWidget *parent, rpl::producer<ClubCard> card, bool self);

protected:
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	void shine();

	const bool _self = false;
	ClubCard _card;
	Ui::Animations::Simple _shine;
	base::Timer _shineTimer;

};

class ClubBackdrop final {
public:
	ClubBackdrop(not_null<UserData*> user, Fn<void()> changed);

	[[nodiscard]] bool active() const;
	[[nodiscard]] QColor edgeColor() const;
	void paint(QPainter &p, QSize full, QRect target);
	void paintGlow(QPainter &p, QRect userpic);

private:
	void setCard(std::optional<ClubCard> card);
	void validate(QSize size);
	void updateColors();
	void paintGradient(QPainter &q, QSize size);
	void paintImage(QPainter &q, QSize size, const QImage &original);
	void paintFigure(QPainter &q, QSize size, float64 opacity);

	const not_null<Main::Session*> _session;
	const Fn<void()> _changed;
	std::optional<ClubCard> _card;
	std::shared_ptr<Data::PhotoMedia> _media;
	rpl::lifetime _photoLifetime;
	QImage _cache;
	bool _cacheValid = false;
	bool _cacheWithImage = false;
	bool _imageReady = false;
	QColor _accent;
	QColor _edge;
	rpl::lifetime _lifetime;

};

class BannerPicker final : public Ui::RpWidget {
public:
	BannerPicker(
		QWidget *parent,
		Fn<void()> chooseColors,
		Fn<void()> chooseImage);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	[[nodiscard]] int count() const;
	[[nodiscard]] int valueAt(int index) const;
	[[nodiscard]] QRectF swatch(int index) const;
	[[nodiscard]] int indexAt(QPoint point) const;
	[[nodiscard]] int selectedIndex() const;

	const Fn<void()> _chooseColors;
	const Fn<void()> _chooseImage;
	int _selected = 0;
	int _previous = -1;
	int _hovered = -1;
	Ui::Animations::Simple _selection;

};

} // namespace Moligram
