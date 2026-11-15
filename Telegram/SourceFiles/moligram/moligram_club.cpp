/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_club.h"

#include "api/api_common.h"
#include "apiwrap.h"
#include "base/call_delayed.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_file_origin.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "moligram/moligram_cover.h"
#include "moligram/moligram_settings.h"
#include "storage/localimageloader.h"
#include "ui/image/image.h"
#include "ui/painter.h"
#include "styles/style_settings.h"
#include "styles/style_window.h"

#include <QtCore/QBuffer>
#include <QtCore/QRegularExpression>

namespace Moligram {
namespace {

constexpr auto kStartDelay = crl::time(6000);
constexpr auto kRefreshPeriod = crl::time(20 * 60 * 1000);
constexpr auto kSyncDelay = crl::time(4000);
constexpr auto kPageLimit = 100;
constexpr auto kJoinChecks = 10;
constexpr auto kUploadChecks = 12;
constexpr auto kUploadCheckDelay = crl::time(3000);
constexpr auto kBannerHeight = 64;
constexpr auto kBannerMarginX = 14;
constexpr auto kBannerMarginY = 6;
constexpr auto kBannerRadius = 14;
constexpr auto kBannerLogo = 36;
constexpr auto kPickerHeight = 56;
constexpr auto kSwatchWidth = 44;
constexpr auto kSwatchHeight = 28;
constexpr auto kSwatchGap = 8;
constexpr auto kSwatchRadius = 8;
constexpr auto kSelectDuration = crl::time(220);
constexpr auto kShineDuration = crl::time(1500);
constexpr auto kLineHeight = 48;
constexpr auto kLineMarginY = 8;
constexpr auto kLineLogo = 28;
constexpr auto kShinePeriod = crl::time(5200);
constexpr auto kStatusLimit = 24;
constexpr auto kImageWidth = 1200;
constexpr auto kImageHeight = 400;
constexpr auto kBannersTopic = MsgId(2);

constexpr auto kMarkerEdge = char16_t(0x2063);
constexpr auto kLegacyZero = char16_t(0x200B);
constexpr auto kLegacyOne = char16_t(0x200C);
constexpr auto kLegacyBits = 3;
constexpr auto kNibbleBase = char16_t(0xFE00);
constexpr auto kKindPreset = 0;
constexpr auto kKindColors = 1;
constexpr auto kKindImage = 2;
constexpr auto kImageNibbles = 7;

struct Marker {
	int position = -1;
	int length = 0;
	ClubCard card;
};

[[nodiscard]] bool IsNibble(QChar ch) {
	return ch.unicode() >= kNibbleBase && ch.unicode() < kNibbleBase + 16;
}

[[nodiscard]] int Nibble(QChar ch) {
	return ch.unicode() - kNibbleBase;
}

[[nodiscard]] QChar NibbleChar(int value) {
	return QChar(char16_t(kNibbleBase + (value & 0x0F)));
}

[[nodiscard]] std::optional<Marker> FindMarker(const QString &about) {
	const auto size = int(about.size());
	const auto edge = QChar(kMarkerEdge);
	for (auto i = 0; i < size; ++i) {
		if (about[i] != edge) {
			continue;
		}
		auto j = i + 1;
		while (j < size && (IsNibble(about[j])
			|| about[j] == QChar(kLegacyZero)
			|| about[j] == QChar(kLegacyOne))) {
			++j;
		}
		if (j >= size || about[j] != edge || j == i + 1) {
			continue;
		}
		const auto payload = about.mid(i + 1, j - i - 1);
		auto result = Marker{ .position = i, .length = j - i + 1 };
		auto &card = result.card;
		const auto legacy = ranges::all_of(payload, [](QChar ch) {
			return ch == QChar(kLegacyZero) || ch == QChar(kLegacyOne);
		});
		if (legacy && payload.size() == kLegacyBits) {
			auto value = 0;
			for (const auto ch : payload) {
				value = (value << 1) | ((ch == QChar(kLegacyOne)) ? 1 : 0);
			}
			card.banner = value;
			return result;
		} else if (!ranges::all_of(payload, IsNibble)) {
			continue;
		}
		const auto kind = Nibble(payload[0]);
		const auto rest = payload.mid(1);
		if (kind == kKindPreset && rest.size() == 1) {
			card.banner = std::min(
				Nibble(rest[0]),
				int(BannerPresets().size()) - 1);
			return result;
		} else if (kind == kKindColors && rest.size() == 6) {
			const auto channel = [&](int index) {
				return Nibble(rest[index]) * 17;
			};
			card.banner = kBannerColors;
			card.from = QColor(channel(0), channel(1), channel(2));
			card.to = QColor(channel(3), channel(4), channel(5));
			return result;
		} else if (kind == kKindImage && rest.size() == kImageNibbles) {
			auto id = int64();
			for (const auto ch : rest) {
				id = (id << 4) | Nibble(ch);
			}
			card.banner = kBannerImage;
			card.image = MsgId(id);
			return result;
		}
	}
	return std::nullopt;
}

[[nodiscard]] QString MakeMarker() {
	auto result = QString(QChar(kMarkerEdge));
	const auto banner = ClubBanner();
	const auto image = ClubImage();
	if (banner == kBannerImage && image) {
		result += NibbleChar(kKindImage);
		for (auto i = kImageNibbles - 1; i >= 0; --i) {
			result += NibbleChar(int((image.bare >> (i * 4)) & 0x0F));
		}
	} else if (banner == kBannerColors) {
		const auto [from, to] = ClubColors();
		result += NibbleChar(kKindColors);
		for (const auto &color : { from, to }) {
			result += NibbleChar((color.red() + 8) / 17);
			result += NibbleChar((color.green() + 8) / 17);
			result += NibbleChar((color.blue() + 8) / 17);
		}
	} else {
		result += NibbleChar(kKindPreset);
		result += NibbleChar(std::clamp(
			banner,
			0,
			int(BannerPresets().size()) - 1));
	}
	return result + QChar(kMarkerEdge);
}

[[nodiscard]] std::optional<std::pair<UserId, QString>> ParseStatus(
		const QString &text) {
	static const auto regex = QRegularExpression(
		u"^#moligram_status\\s+(\\d+)\\s+(\\S.*)$"_q);
	const auto match = regex.match(text.trimmed());
	if (!match.hasMatch()) {
		return std::nullopt;
	}
	const auto id = match.captured(1).toULongLong();
	const auto status = match.captured(2).trimmed().left(kStatusLimit);
	if (!id || status.isEmpty()) {
		return std::nullopt;
	}
	return std::make_pair(UserId(id), status);
}

[[nodiscard]] QString StatusText(const QString &status) {
	const auto lower = status.toLower();
	if (lower == u"early"_q) {
		return tr::lng_moligram_club_status_early(tr::now);
	} else if (lower == u"contributor"_q) {
		return tr::lng_moligram_club_status_contributor(tr::now);
	} else if (lower == u"developer"_q || lower == u"dev"_q) {
		return tr::lng_moligram_club_status_developer(tr::now);
	}
	return status;
}

[[nodiscard]] const QImage &Logo(int size) {
	static auto cache = base::flat_map<int, QImage>();
	const auto i = cache.find(size);
	if (i != end(cache)) {
		return i->second;
	}
	const auto ratio = style::DevicePixelRatio();
	auto image = QImage(u":/gui/art/logo_256_no_margin.png"_q).scaled(
		size * ratio,
		size * ratio,
		Qt::KeepAspectRatio,
		Qt::SmoothTransformation);
	image.setDevicePixelRatio(ratio);
	return cache.emplace(size, std::move(image)).first->second;
}

[[nodiscard]] MTPmessages_Search SearchRequest(
		not_null<ChannelData*> channel,
		const QString &query,
		PeerData *from,
		MTPMessagesFilter filter = MTP_inputMessagesFilterEmpty()) {
	using Flag = MTPmessages_Search::Flag;
	return MTPmessages_Search(
		MTP_flags(from ? Flag::f_from_id : Flag()),
		channel->input(),
		MTP_string(query),
		from ? from->input() : MTP_inputPeerEmpty(),
		MTPInputPeer(),
		MTPVector<MTPReaction>(),
		MTPint(),
		filter,
		MTP_int(0),
		MTP_int(0),
		MTP_int(0),
		MTP_int(0),
		MTP_int(kPageLimit),
		MTP_int(0),
		MTP_int(0),
		MTP_long(0));
}

[[nodiscard]] QByteArray PrepareImage(QImage image) {
	const auto target = QSize(kImageWidth, kImageHeight);
	image = image.scaled(
		target,
		Qt::KeepAspectRatioByExpanding,
		Qt::SmoothTransformation);
	image = image.copy(
		(image.width() - target.width()) / 2,
		(image.height() - target.height()) / 2,
		target.width(),
		target.height()).convertToFormat(QImage::Format_RGB32);
	auto result = QByteArray();
	auto buffer = QBuffer(&result);
	buffer.open(QIODevice::WriteOnly);
	image.save(&buffer, "JPG", 90);
	return result;
}

class Club final {
public:
	explicit Club(not_null<Main::Session*> session);

	[[nodiscard]] rpl::producer<std::optional<ClubCard>> value(
		not_null<UserData*> user);
	[[nodiscard]] rpl::producer<PhotoData*> image(MsgId id, UserId owner);
	void upload(QImage image, Fn<void(bool)> done);

private:
	void resolve(Fn<void()> then = nullptr);
	void refresh();
	void requestStatuses();
	void syncSelf();
	void requestImage(MsgId id, UserId owner);
	void waitJoined(QByteArray bytes, Fn<void(bool)> done, int left);
	void sendImage(QByteArray bytes, Fn<void(bool)> done);
	void findUploaded(MsgId after, Fn<void(bool)> done, int left);
	[[nodiscard]] QString status(UserId id) const;

	template <typename Data>
	void process(const Data &data) {
		_session->data().processUsers(data.vusers());
		_session->data().processChats(data.vchats());
	}

	const not_null<Main::Session*> _session;
	base::Timer _refreshTimer;
	base::Timer _syncTimer;
	ChannelData *_channel = nullptr;
	mtpRequestId _resolveRequestId = 0;
	mtpRequestId _loadRequestId = 0;
	std::vector<Fn<void()>> _afterResolve;
	base::flat_set<QString> _written;
	base::flat_map<UserId, QString> _statuses;
	base::flat_map<MsgId, PhotoData*> _images;
	base::flat_set<MsgId> _imageRequests;
	bool _uploading = false;
	rpl::event_stream<> _changes;
	rpl::lifetime _lifetime;

};

Club::Club(not_null<Main::Session*> session)
: _session(session)
, _refreshTimer([=] { refresh(); })
, _syncTimer([=] { syncSelf(); }) {
	_refreshTimer.callOnce(kStartDelay);
	const auto self = session->user();
	rpl::merge(
		Value(Option::ClubMember) | rpl::to_empty,
		Changes(),
		session->changes().peerFlagsValue(
			self,
			Data::PeerUpdate::Flag::About) | rpl::to_empty
	) | rpl::on_next([=] {
		_syncTimer.callOnce(kSyncDelay);
	}, _lifetime);
	crl::on_main(session, [=] {
		if (!self->wasFullUpdated()) {
			session->api().requestFullPeer(self);
		}
	});
}

QString Club::status(UserId id) const {
	const auto i = _statuses.find(id);
	return (i != end(_statuses)) ? StatusText(i->second) : QString();
}

rpl::producer<std::optional<ClubCard>> Club::value(
		not_null<UserData*> user) {
	const auto id = peerToUser(user->id);
	return rpl::combine(
		user->session().changes().peerFlagsValue(
			user,
			Data::PeerUpdate::Flag::About),
		rpl::single(rpl::empty) | rpl::then(_changes.events())
	) | rpl::map([=]() -> std::optional<ClubCard> {
		const auto marker = FindMarker(user->about());
		if (!marker) {
			return std::nullopt;
		}
		auto card = marker->card;
		card.owner = id;
		card.status = status(id);
		return card;
	}) | rpl::distinct_until_changed();
}

void Club::syncSelf() {
	const auto self = _session->user();
	if (!self->wasFullUpdated()) {
		return;
	}
	const auto about = self->about();
	const auto clean = StripClubMarker(about);
	auto wanted = clean;
	if (Enabled(Option::ClubMember)) {
		wanted = clean + MakeMarker();
		const auto limit = _session->appConfig().get<int>(
			self->isPremium()
				? u"about_length_limit_premium"_q
				: u"about_length_limit_default"_q,
			self->isPremium() ? 140 : 70);
		if (wanted.size() > limit) {
			return;
		}
	}
	if (wanted == about || _written.contains(wanted)) {
		return;
	}
	_written.emplace(wanted);
	_session->api().saveSelfBio(wanted);
}

void Club::resolve(Fn<void()> then) {
	if (then) {
		_afterResolve.push_back(std::move(then));
	}
	if (_resolveRequestId) {
		return;
	}
	const auto finish = [=] {
		for (const auto &callback : base::take(_afterResolve)) {
			callback();
		}
	};
	_resolveRequestId = _session->api().request(MTPcontacts_ResolveUsername(
		MTP_flags(0),
		MTP_string(QString::fromLatin1(kClubUsername)),
		MTP_string()
	)).done([=](const MTPcontacts_ResolvedPeer &result) {
		_resolveRequestId = 0;
		const auto &data = result.data();
		process(data);
		const auto peer = _session->data().peerLoaded(
			peerFromMTP(data.vpeer()));
		const auto channel = peer ? peer->asChannel() : nullptr;
		if (channel && channel->isMegagroup()) {
			_channel = channel;
		}
		finish();
	}).fail([=] {
		_resolveRequestId = 0;
		finish();
	}).send();
}

void Club::refresh() {
	if (!_channel) {
		resolve([=] {
			if (_channel) {
				requestStatuses();
			} else {
				_refreshTimer.callOnce(kRefreshPeriod);
			}
		});
	} else if (!_loadRequestId) {
		requestStatuses();
	}
}

void Club::requestStatuses() {
	_loadRequestId = _session->api().request(SearchRequest(
		_channel,
		u"#moligram_status"_q,
		nullptr
	)).done([=](const MTPmessages_Messages &result) {
		_loadRequestId = 0;
		auto statuses = base::flat_map<UserId, QString>();
		const auto own = _channel->id;
		result.match([](const MTPDmessages_messagesNotModified &) {
		}, [&](const auto &data) {
			process(data);
			for (const auto &message : data.vmessages().v) {
				message.match([&](const MTPDmessage &data) {
					const auto from = data.vfrom_id();
					if (from && peerFromMTP(*from) != own) {
						return;
					}
					const auto parsed = ParseStatus(qs(data.vmessage()));
					if (parsed && !statuses.contains(parsed->first)) {
						statuses.emplace(parsed->first, parsed->second);
					}
				}, [](const auto &) {
				});
			}
		});
		_statuses = std::move(statuses);
		_changes.fire({});
		_refreshTimer.callOnce(kRefreshPeriod);
	}).fail([=] {
		_loadRequestId = 0;
		_refreshTimer.callOnce(kRefreshPeriod);
	}).send();
}

rpl::producer<PhotoData*> Club::image(MsgId id, UserId owner) {
	if (!id) {
		return rpl::single(static_cast<PhotoData*>(nullptr));
	}
	requestImage(id, owner);
	return rpl::single(
		rpl::empty
	) | rpl::then(
		_changes.events()
	) | rpl::map([=]() -> PhotoData* {
		const auto i = _images.find(id);
		return (i != end(_images)) ? i->second : nullptr;
	}) | rpl::distinct_until_changed();
}

void Club::requestImage(MsgId id, UserId owner) {
	if (_images.contains(id) || _imageRequests.contains(id)) {
		return;
	}
	if (!_channel) {
		resolve([=] {
			if (_channel) {
				requestImage(id, owner);
			}
		});
		return;
	}
	_imageRequests.emplace(id);
	_session->api().request(MTPchannels_GetMessages(
		_channel->inputChannel(),
		MTP_vector<MTPInputMessage>(1, MTP_inputMessageID(MTP_int(id.bare)))
	)).done([=](const MTPmessages_Messages &result) {
		_imageRequests.remove(id);
		auto photo = (PhotoData*)nullptr;
		result.match([](const MTPDmessages_messagesNotModified &) {
		}, [&](const auto &data) {
			process(data);
			for (const auto &message : data.vmessages().v) {
				message.match([&](const MTPDmessage &data) {
					const auto from = data.vfrom_id();
					if (!from || peerFromMTP(*from) != peerFromUser(owner)) {
						return;
					}
					const auto media = data.vmedia();
					if (!media) {
						return;
					}
					media->match([&](const MTPDmessageMediaPhoto &media) {
						if (const auto value = media.vphoto()) {
							photo = _session->data().processPhoto(*value);
						}
					}, [](const auto &) {
					});
				}, [](const auto &) {
				});
			}
		});
		_images[id] = photo;
		_changes.fire({});
	}).fail([=] {
		_imageRequests.remove(id);
		_images[id] = nullptr;
	}).send();
}

void Club::upload(QImage image, Fn<void(bool)> done) {
	if (_uploading || image.isNull()) {
		done(false);
		return;
	}
	_uploading = true;
	auto finish = [=](bool success) {
		_uploading = false;
		done(success);
	};
	auto bytes = PrepareImage(std::move(image));
	const auto start = [=] {
		if (!_channel) {
			finish(false);
		} else if (_channel->amIn()) {
			sendImage(bytes, finish);
		} else {
			_session->api().joinChannel(_channel);
			waitJoined(bytes, finish, kJoinChecks);
		}
	};
	if (_channel) {
		start();
	} else {
		resolve(start);
	}
}

void Club::waitJoined(QByteArray bytes, Fn<void(bool)> done, int left) {
	if (_channel->amIn()) {
		sendImage(std::move(bytes), done);
	} else if (left <= 0) {
		done(false);
	} else {
		base::call_delayed(crl::time(1000), _session, [=] {
			waitJoined(bytes, done, left - 1);
		});
	}
}

void Club::sendImage(QByteArray bytes, Fn<void(bool)> done) {
	_session->api().request(SearchRequest(
		_channel,
		QString(),
		_session->user(),
		MTP_inputMessagesFilterPhotos()
	)).done([=](const MTPmessages_Messages &result) {
		auto last = MsgId();
		result.match([](const MTPDmessages_messagesNotModified &) {
		}, [&](const auto &data) {
			for (const auto &message : data.vmessages().v) {
				last = std::max(last, MsgId(message.match([](const auto &d) {
					return d.vid().v;
				})));
			}
		});
		const auto history = _session->data().history(_channel);
		auto action = Api::SendAction(history);
		if (_channel->isForum()) {
			action.replyTo.messageId = FullMsgId(_channel->id, kBannersTopic);
			action.replyTo.topicRootId = kBannersTopic;
		}
		_session->api().sendFile(bytes, SendMediaType::Photo, action);
		base::call_delayed(kUploadCheckDelay, _session, [=] {
			findUploaded(last, done, kUploadChecks);
		});
	}).fail([=] {
		done(false);
	}).send();
}

void Club::findUploaded(MsgId after, Fn<void(bool)> done, int left) {
	_session->api().request(SearchRequest(
		_channel,
		QString(),
		_session->user(),
		MTP_inputMessagesFilterPhotos()
	)).done([=](const MTPmessages_Messages &result) {
		auto found = MsgId();
		result.match([](const MTPDmessages_messagesNotModified &) {
		}, [&](const auto &data) {
			process(data);
			for (const auto &message : data.vmessages().v) {
				const auto id = MsgId(message.match([](const auto &d) {
					return d.vid().v;
				}));
				if (id > after) {
					found = std::max(found, id);
				}
			}
		});
		if (found) {
			_images.remove(found);
			SetClubImage(found);
			SetClubBanner(kBannerImage);
			done(true);
		} else if (left > 1) {
			base::call_delayed(kUploadCheckDelay, _session, [=] {
				findUploaded(after, done, left - 1);
			});
		} else {
			done(false);
		}
	}).fail([=] {
		done(false);
	}).send();
}

[[nodiscard]] base::flat_map<
	not_null<Main::Session*>,
	std::unique_ptr<Club>> &Clubs() {
	static auto result = base::flat_map<
		not_null<Main::Session*>,
		std::unique_ptr<Club>>();
	return result;
}

[[nodiscard]] Club *ClubFor(not_null<Main::Session*> session) {
	const auto i = Clubs().find(session);
	return (i != end(Clubs())) ? i->second.get() : nullptr;
}

} // namespace

const std::vector<BannerPreset> &BannerPresets() {
	static const auto result = std::vector<BannerPreset>{
		{ QColor(0x2b, 0x2d, 0x35), QColor(0x56, 0x5c, 0x6e) },
		{ QColor(0x0f, 0x76, 0x6e), QColor(0x63, 0x66, 0xf1) },
		{ QColor(0xb9, 0x1c, 0x1c), QColor(0xf5, 0x9e, 0x0b) },
		{ QColor(0x1e, 0x3a, 0x8a), QColor(0x06, 0xb6, 0xd4) },
		{ QColor(0x6d, 0x28, 0xd9), QColor(0xec, 0x48, 0x99) },
		{ QColor(0x06, 0x5f, 0x46), QColor(0x34, 0xd3, 0x99) },
		{ QColor(0x9d, 0x17, 0x4d), QColor(0xfb, 0x92, 0x3c) },
		{ QColor(0x0f, 0x17, 0x2a), QColor(0x47, 0x55, 0x69) },
	};
	return result;
}

const BannerPreset &BannerAt(int index) {
	const auto &presets = BannerPresets();
	return presets[std::clamp(index, 0, int(presets.size()) - 1)];
}

void AttachClub(not_null<Main::Session*> session) {
	Clubs().emplace(session, std::make_unique<Club>(session));
	session->lifetime().add([=] {
		Clubs().remove(session);
	});
}

bool ShowClubBadge(PeerData *peer) {
	return peer
		&& peer->isUser()
		&& Enabled(Option::ClubBadges)
		&& FindMarker(peer->about()).has_value();
}

QString StripClubMarker(QString about) {
	while (const auto marker = FindMarker(about)) {
		about.remove(marker->position, marker->length);
	}
	return about;
}

void PaintClubBadge(QPainter &p, int x, int y, int size) {
	p.drawImage(QRect(x, y, size, size), Logo(size));
}

rpl::producer<std::optional<ClubCard>> ClubCardValue(
		not_null<UserData*> user) {
	if (const auto club = ClubFor(&user->session())) {
		return club->value(user);
	}
	return rpl::single(std::optional<ClubCard>());
}

rpl::producer<ClubCard> OwnClubCardValue(not_null<Main::Session*> session) {
	const auto owner = session->userId();
	return rpl::single(
		rpl::empty
	) | rpl::then(
		Changes()
	) | rpl::map([=] {
		const auto [from, to] = ClubColors();
		const auto banner = ClubBanner();
		return ClubCard{
			.banner = banner,
			.from = (banner == kBannerColors) ? from : QColor(),
			.to = (banner == kBannerColors) ? to : QColor(),
			.image = (banner == kBannerImage) ? ClubImage() : MsgId(),
			.owner = owner,
		};
	}) | rpl::distinct_until_changed();
}

void UploadClubImage(
		not_null<Main::Session*> session,
		QImage image,
		Fn<void(bool)> done) {
	if (const auto club = ClubFor(session)) {
		club->upload(std::move(image), std::move(done));
	} else {
		done(false);
	}
}

BannerCard::BannerCard(
	QWidget *parent,
	not_null<Main::Session*> session,
	rpl::producer<ClubCard> card)
: RpWidget(parent)
, _session(session)
, _shineTimer([=] { shine(); }) {
	std::move(card) | rpl::on_next([=](ClubCard value) {
		setCard(std::move(value));
	}, lifetime());
	session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		if (_media && !_cacheWithImage) {
			_cacheValid = false;
			update();
		}
	}, lifetime());
	resize(width(), resizeGetHeight(width()));
	_shineTimer.callEach(kShinePeriod);
	crl::on_main(this, [=] { shine(); });
}

void BannerCard::setCard(ClubCard card) {
	const auto imageChanged = (card.image != _card.image)
		|| (card.owner != _card.owner);
	_card = std::move(card);
	_cacheValid = false;
	if (imageChanged) {
		_photoLifetime.destroy();
		_photo = nullptr;
		_media = nullptr;
		const auto club = ClubFor(_session);
		if (_card.image && club) {
			club->image(
				_card.image,
				_card.owner
			) | rpl::on_next([=](PhotoData *photo) {
				_photo = photo;
				_media = photo ? photo->createMediaView() : nullptr;
				if (_media) {
					_media->wanted(Data::PhotoSize::Large, Data::FileOrigin());
				}
				_cacheValid = false;
				update();
			}, _photoLifetime);
		}
	}
	update();
}

int BannerCard::resizeGetHeight(int newWidth) {
	return style::ConvertScale(kBannerHeight + kBannerMarginY * 2);
}

void BannerCard::shine() {
	if (anim::Disabled() || !isVisible() || _shine.animating()) {
		return;
	}
	_shine.start([=] { update(); }, 0., 1., kShineDuration, anim::sineInOut);
}

void BannerCard::paintBackground(QPainter &q, QSize size) {
	const auto image = _media
		? _media->image(Data::PhotoSize::Large)
		: nullptr;
	_cacheWithImage = (image != nullptr);
	if (image) {
		const auto original = image->original();
		const auto scaled = original.scaled(
			size * style::DevicePixelRatio(),
			Qt::KeepAspectRatioByExpanding,
			Qt::SmoothTransformation);
		const auto ratio = style::DevicePixelRatio();
		const auto source = QRect(
			(scaled.width() - size.width() * ratio) / 2,
			(scaled.height() - size.height() * ratio) / 2,
			size.width() * ratio,
			size.height() * ratio);
		q.drawImage(QRect(QPoint(), size), scaled, source);
		auto shade = QLinearGradient(0, 0, size.width(), 0);
		shade.setColorAt(0., QColor(0, 0, 0, 150));
		shade.setColorAt(0.6, QColor(0, 0, 0, 60));
		shade.setColorAt(1., QColor(0, 0, 0, 20));
		q.fillRect(QRect(QPoint(), size), shade);
		return;
	}
	const auto custom = (_card.banner == kBannerColors)
		&& _card.from.isValid()
		&& _card.to.isValid();
	const auto &preset = BannerAt(
		(_card.banner < int(BannerPresets().size())) ? _card.banner : 0);
	auto gradient = QLinearGradient(0, 0, size.width(), size.height());
	gradient.setColorAt(0., custom ? _card.from : preset.from);
	gradient.setColorAt(1., custom ? _card.to : preset.to);
	q.fillRect(QRect(QPoint(), size), gradient);
	auto glare = QRadialGradient(
		QPointF(size.width() * 0.15, 0.),
		size.width() * 0.6);
	glare.setColorAt(0., QColor(255, 255, 255, 46));
	glare.setColorAt(1., QColor(255, 255, 255, 0));
	q.fillRect(QRect(QPoint(), size), glare);
	const auto ratio = style::DevicePixelRatio();
	const auto figure = PrepareFigure(size.height() * 2.1, ratio);
	if (!figure.isNull()) {
		const auto figureSize = figure.size() / ratio;
		q.setOpacity(0.22);
		q.drawImage(
			QPointF(
				size.width() - figureSize.width() * 0.8,
				(size.height() - figureSize.height()) / 2.),
			figure);
		q.setOpacity(1.);
	}
}

void BannerCard::paintEvent(QPaintEvent *e) {
	const auto ratio = style::DevicePixelRatio();
	const auto inner = rect().marginsRemoved(QMargins(
		style::ConvertScale(kBannerMarginX),
		style::ConvertScale(kBannerMarginY),
		style::ConvertScale(kBannerMarginX),
		style::ConvertScale(kBannerMarginY)));
	if (inner.isEmpty()) {
		return;
	}
	if (!_cacheValid || _cache.size() != inner.size() * ratio) {
		_cacheValid = true;
		_cache = QImage(
			inner.size() * ratio,
			QImage::Format_ARGB32_Premultiplied);
		_cache.setDevicePixelRatio(ratio);
		_cache.fill(Qt::transparent);
		auto q = QPainter(&_cache);
		auto hq = PainterHighQualityEnabler(q);
		const auto radius = float64(style::ConvertScale(kBannerRadius));
		auto clip = QPainterPath();
		clip.addRoundedRect(
			QRectF(QPointF(), QSizeF(inner.size())),
			radius,
			radius);
		q.setClipPath(clip);
		paintBackground(q, inner.size());
	}
	auto p = QPainter(this);
	p.drawImage(inner.topLeft(), _cache);
	if (_shine.animating()) {
		const auto progress = _shine.value(1.);
		const auto band = inner.width() * 0.35;
		const auto center = inner.x()
			- band
			+ (inner.width() + band * 2) * progress;
		auto sweep = QLinearGradient(
			center - band / 2.,
			inner.top(),
			center + band / 2.,
			inner.bottom());
		sweep.setColorAt(0., QColor(255, 255, 255, 0));
		sweep.setColorAt(0.5, QColor(255, 255, 255, 42));
		sweep.setColorAt(1., QColor(255, 255, 255, 0));
		auto hq = PainterHighQualityEnabler(p);
		const auto radius = float64(style::ConvertScale(kBannerRadius));
		p.setPen(Qt::NoPen);
		p.setBrush(sweep);
		p.drawRoundedRect(QRectF(inner), radius, radius);
	}

	const auto logo = style::ConvertScale(kBannerLogo);
	const auto padding = style::ConvertScale(14);
	p.drawImage(
		QPoint(inner.x() + padding, inner.y() + (inner.height() - logo) / 2),
		Logo(logo));
	const auto left = inner.x() + padding + logo + style::ConvertScale(12);
	auto right = inner.x() + inner.width() - padding;
	if (!_card.status.isEmpty()) {
		const auto &font = st::semiboldFont;
		const auto pillPadding = style::ConvertScale(10);
		const auto pillHeight = font->height + style::ConvertScale(8);
		const auto pillWidth = font->width(_card.status) + pillPadding * 2;
		const auto pill = QRect(
			right - pillWidth,
			inner.y() + (inner.height() - pillHeight) / 2,
			pillWidth,
			pillHeight);
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(255, 255, 255, 46));
		p.drawRoundedRect(pill, pillHeight / 2., pillHeight / 2.);
		p.setPen(QColor(255, 255, 255));
		p.setFont(font);
		p.drawText(pill, Qt::AlignCenter, _card.status);
		right = pill.x() - style::ConvertScale(10);
	}
	const auto available = std::max(right - left, 1);
	const auto &title = st::semiboldFont;
	const auto &about = st::normalFont;
	const auto gap = style::ConvertScale(2);
	const auto top = inner.y()
		+ (inner.height() - title->height - gap - about->height) / 2;
	p.setPen(QColor(255, 255, 255));
	p.setFont(title);
	p.drawText(left, top + title->ascent, u"Moligram"_q);
	p.setPen(QColor(255, 255, 255, 215));
	p.setFont(about);
	p.drawText(
		left,
		top + title->height + gap + about->ascent,
		about->elided(tr::lng_moligram_club_badge(tr::now), available));
}

ClubBackdrop::ClubBackdrop(not_null<UserData*> user, Fn<void()> changed)
: _session(&user->session())
, _changed(std::move(changed)) {
	ClubCardValue(
		user
	) | rpl::on_next([=](std::optional<ClubCard> card) {
		setCard(std::move(card));
	}, _lifetime);
	_session->downloaderTaskFinished(
	) | rpl::filter([=] {
		return _media
			&& !_imageReady
			&& _media->image(Data::PhotoSize::Large);
	}) | rpl::on_next([=] {
		_imageReady = true;
		_cacheValid = false;
		updateColors();
		_changed();
	}, _lifetime);
}

bool ClubBackdrop::active() const {
	return _card.has_value();
}

QColor ClubBackdrop::edgeColor() const {
	return _edge;
}

void ClubBackdrop::setCard(std::optional<ClubCard> card) {
	if (_card == card) {
		return;
	}
	const auto imageChanged = !_card
		|| !card
		|| (card->image != _card->image)
		|| (card->owner != _card->owner);
	_card = std::move(card);
	_cacheValid = false;
	if (imageChanged) {
		_photoLifetime.destroy();
		_media = nullptr;
		_imageReady = false;
		const auto club = ClubFor(_session);
		if (_card && _card->image && club) {
			club->image(
				_card->image,
				_card->owner
			) | rpl::on_next([=](PhotoData *photo) {
				_media = photo ? photo->createMediaView() : nullptr;
				if (_media) {
					_media->wanted(Data::PhotoSize::Large, Data::FileOrigin());
				}
				_imageReady = _media
					&& _media->image(Data::PhotoSize::Large);
				_cacheValid = false;
				updateColors();
				_changed();
			}, _photoLifetime);
		}
	}
	updateColors();
	_changed();
}

void ClubBackdrop::paintFigure(QPainter &q, QSize size, float64 opacity) {
	const auto ratio = style::DevicePixelRatio();
	const auto figure = PrepareFigure(size.height() * 1.08, ratio);
	if (figure.isNull()) {
		return;
	}
	auto faded = figure;
	{
		auto mask = QPainter(&faded);
		mask.setCompositionMode(QPainter::CompositionMode_DestinationIn);
		auto fade = QLinearGradient(0, 0, 0, faded.height());
		fade.setColorAt(0.5, QColor(0, 0, 0, 255));
		fade.setColorAt(0.82, QColor(0, 0, 0, 0));
		mask.fillRect(faded.rect(), fade);
	}
	const auto figureSize = QSizeF(faded.size()) / ratio;
	q.setOpacity(opacity);
	q.drawImage(
		QPointF(
			size.width() - figureSize.width() * 0.66,
			size.height() - figureSize.height() * 0.94),
		faded);
	q.setOpacity(1.);
}

void ClubBackdrop::paintGradient(QPainter &q, QSize size) {
	const auto custom = (_card->banner == kBannerColors)
		&& _card->from.isValid()
		&& _card->to.isValid();
	const auto &preset = BannerAt(
		(_card->banner < int(BannerPresets().size())) ? _card->banner : 0);
	const auto from = custom ? _card->from : preset.from;
	const auto to = custom ? _card->to : preset.to;
	const auto rect = QRect(QPoint(), size);
	auto gradient = QLinearGradient(0, 0, size.width(), size.height());
	gradient.setColorAt(0., from);
	gradient.setColorAt(1., to);
	q.fillRect(rect, gradient);
	auto glare = QRadialGradient(
		QPointF(size.width() / 2., size.height() * 0.3),
		size.width() * 0.55);
	glare.setColorAt(0., QColor(255, 255, 255, 56));
	glare.setColorAt(1., QColor(255, 255, 255, 0));
	q.fillRect(rect, glare);
	paintFigure(q, size, 0.3);
	auto shade = QLinearGradient(0, 0, 0, size.height());
	shade.setColorAt(0.45, QColor(0, 0, 0, 0));
	shade.setColorAt(1., QColor(0, 0, 0, 64));
	q.fillRect(rect, shade);
}

void ClubBackdrop::paintImage(
		QPainter &q,
		QSize size,
		const QImage &original) {
	const auto ratio = style::DevicePixelRatio();
	const auto scaled = original.scaled(
		size * ratio,
		Qt::KeepAspectRatioByExpanding,
		Qt::SmoothTransformation);
	const auto source = QRect(
		(scaled.width() - size.width() * ratio) / 2,
		(scaled.height() - size.height() * ratio) / 2,
		size.width() * ratio,
		size.height() * ratio);
	const auto rect = QRect(QPoint(), size);
	q.drawImage(rect, scaled, source);
	q.fillRect(rect, QColor(0, 0, 0, 56));
	paintFigure(q, size, 0.2);
	auto shade = QLinearGradient(0, 0, 0, size.height());
	shade.setColorAt(0.35, QColor(0, 0, 0, 0));
	shade.setColorAt(1., QColor(0, 0, 0, 120));
	q.fillRect(rect, shade);
}

void ClubBackdrop::updateColors() {
	_accent = _edge = QColor();
	if (!_card) {
		return;
	}
	const auto image = _media
		? _media->image(Data::PhotoSize::Large)
		: nullptr;
	if (image) {
		const auto average = image->original().scaled(
			1,
			1,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation).pixelColor(0, 0);
		auto accent = average.toHsl();
		accent.setHslF(
			accent.hslHueF(),
			std::max(float64(accent.hslSaturationF()), 0.55),
			std::clamp(float64(accent.lightnessF()), 0.55, 0.7));
		_accent = accent.toRgb();
		_edge = average.darker(150);
		return;
	}
	const auto custom = (_card->banner == kBannerColors)
		&& _card->from.isValid()
		&& _card->to.isValid();
	const auto &preset = BannerAt(
		(_card->banner < int(BannerPresets().size())) ? _card->banner : 0);
	const auto from = custom ? _card->from : preset.from;
	const auto to = custom ? _card->to : preset.to;
	_edge = anim::color(from, to, 0.5);
	_accent = _edge.lighter(135);
}

void ClubBackdrop::validate(QSize size) {
	const auto ratio = style::DevicePixelRatio();
	const auto image = _media
		? _media->image(Data::PhotoSize::Large)
		: nullptr;
	if (_cacheValid
		&& _cache.size() == size * ratio
		&& _cacheWithImage == (image != nullptr)) {
		return;
	}
	_cacheValid = true;
	_cacheWithImage = (image != nullptr);
	_cache = QImage(size * ratio, QImage::Format_ARGB32_Premultiplied);
	_cache.setDevicePixelRatio(ratio);
	_cache.fill(Qt::transparent);
	auto q = QPainter(&_cache);
	auto hq = PainterHighQualityEnabler(q);
	if (image) {
		paintImage(q, size, image->original());
	} else {
		paintGradient(q, size);
	}
	PaintGrain(q, QRect(QPoint(), size), 0.05);
}

void ClubBackdrop::paint(QPainter &p, QSize full, QRect target) {
	if (!_card || full.isEmpty()) {
		return;
	}
	validate(full);
	const auto x = target.x() + (target.width() - full.width()) / 2;
	const auto y = target.y() + (target.height() - full.height()) / 2;
	p.drawImage(x, y, _cache);
}

void ClubBackdrop::paintGlow(QPainter &p, QRect userpic) {
	if (!_card || !_accent.isValid() || userpic.isEmpty()) {
		return;
	}
	auto hq = PainterHighQualityEnabler(p);
	const auto center = QRectF(userpic).center();
	const auto radius = userpic.width() / 2.;
	const auto outer = radius * 1.75;
	auto glow = QRadialGradient(center, outer);
	const auto color = [&](int alpha) {
		auto result = _accent;
		result.setAlpha(alpha);
		return result;
	};
	const auto edge = radius / outer;
	glow.setColorAt(0., color(220));
	glow.setColorAt(edge, color(170));
	glow.setColorAt(edge + (1. - edge) * 0.4, color(60));
	glow.setColorAt(1., color(0));
	p.setPen(Qt::NoPen);
	p.setBrush(glow);
	p.drawEllipse(center, outer, outer);
	auto ring = QColor(255, 255, 255, 110);
	p.setPen(QPen(ring, 1.5));
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius + 2.5, radius + 2.5);
}

ClubLine::ClubLine(
	QWidget *parent,
	rpl::producer<ClubCard> card,
	bool self)
: RpWidget(parent)
, _self(self)
, _shineTimer([=] { shine(); }) {
	std::move(card) | rpl::on_next([=](ClubCard value) {
		_card = std::move(value);
		update();
	}, lifetime());
	resize(width(), resizeGetHeight(width()));
	_shineTimer.callEach(kShinePeriod);
	crl::on_main(this, [=] { shine(); });
}

int ClubLine::resizeGetHeight(int newWidth) {
	return style::ConvertScale(kLineHeight + kLineMarginY * 2);
}

void ClubLine::shine() {
	if (anim::Disabled() || !isVisible() || _shine.animating()) {
		return;
	}
	_shine.start([=] { update(); }, 0., 1., kShineDuration, anim::sineInOut);
}

void ClubLine::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);
	const auto inner = QRectF(rect()).marginsRemoved(QMarginsF(
		style::ConvertScale(kBannerMarginX),
		style::ConvertScale(kLineMarginY),
		style::ConvertScale(kBannerMarginX),
		style::ConvertScale(kLineMarginY)));
	if (inner.isEmpty()) {
		return;
	}
	const auto custom = (_card.banner == kBannerColors)
		&& _card.from.isValid()
		&& _card.to.isValid();
	const auto &preset = BannerAt(
		(_card.banner < int(BannerPresets().size())) ? _card.banner : 0);
	const auto from = custom ? _card.from : preset.from;
	const auto to = custom ? _card.to : preset.to;
	const auto alpha = [](QColor color, int value) {
		color.setAlpha(value);
		return color;
	};
	const auto radius = inner.height() / 2.;

	auto fill = QLinearGradient(inner.topLeft(), inner.topRight());
	fill.setColorAt(0., alpha(from, 46));
	fill.setColorAt(1., alpha(to, 20));
	auto border = QLinearGradient(inner.topLeft(), inner.topRight());
	border.setColorAt(0., alpha(from, 150));
	border.setColorAt(1., alpha(to, 60));
	p.setPen(QPen(QBrush(border), 1.));
	p.setBrush(fill);
	p.drawRoundedRect(
		inner.marginsRemoved({ 0.5, 0.5, 0.5, 0.5 }),
		radius,
		radius);

	if (_shine.animating()) {
		const auto progress = _shine.value(1.);
		const auto band = inner.width() * 0.3;
		const auto center = inner.x()
			- band
			+ (inner.width() + band * 2) * progress;
		auto sweep = QLinearGradient(
			center - band / 2.,
			inner.top(),
			center + band / 2.,
			inner.bottom());
		sweep.setColorAt(0., QColor(255, 255, 255, 0));
		sweep.setColorAt(0.5, QColor(255, 255, 255, 30));
		sweep.setColorAt(1., QColor(255, 255, 255, 0));
		p.setPen(Qt::NoPen);
		p.setBrush(sweep);
		p.drawRoundedRect(inner, radius, radius);
	}

	const auto logo = style::ConvertScale(kLineLogo);
	const auto padding = int(inner.height() - logo) / 2;
	const auto logoLeft = int(inner.x()) + padding;
	const auto logoTop = int(inner.y()) + padding;
	auto glow = QRadialGradient(
		QPointF(logoLeft + logo / 2., logoTop + logo / 2.),
		logo * 0.9);
	glow.setColorAt(0., alpha(from.lighter(130), 120));
	glow.setColorAt(1., alpha(from, 0));
	p.setPen(Qt::NoPen);
	p.setBrush(glow);
	p.drawEllipse(
		QPointF(logoLeft + logo / 2., logoTop + logo / 2.),
		logo * 0.9,
		logo * 0.9);
	PaintClubBadge(p, logoLeft, logoTop, logo);

	auto left = logoLeft + logo + style::ConvertScale(12);
	auto right = int(inner.right()) - style::ConvertScale(16);
	if (!_card.status.isEmpty()) {
		const auto &font = st::normalFont;
		const auto pillHeight = font->height + style::ConvertScale(6);
		const auto pillWidth = font->width(_card.status)
			+ style::ConvertScale(18);
		const auto pill = QRectF(
			right - pillWidth,
			inner.y() + (inner.height() - pillHeight) / 2.,
			pillWidth,
			pillHeight);
		auto pillFill = QLinearGradient(pill.topLeft(), pill.topRight());
		pillFill.setColorAt(0., from);
		pillFill.setColorAt(1., to);
		p.setPen(Qt::NoPen);
		p.setBrush(pillFill);
		p.drawRoundedRect(pill, pillHeight / 2., pillHeight / 2.);
		p.setPen(QColor(255, 255, 255));
		p.setFont(font);
		p.drawText(pill, Qt::AlignCenter, _card.status);
		right = int(pill.x()) - style::ConvertScale(10);
	}
	const auto available = std::max(right - left, 1);
	const auto &title = st::semiboldFont;
	const auto &about = st::normalFont;
	const auto gap = style::ConvertScale(1);
	const auto top = int(inner.y())
		+ (int(inner.height()) - title->height - gap - about->height) / 2;
	const auto name = u"Moligram"_q;
	auto text = QLinearGradient(
		left,
		0,
		left + title->width(name),
		0);
	text.setColorAt(0., from.lighter(150));
	text.setColorAt(1., to.lighter(150));
	p.setPen(QPen(QBrush(text), 1.));
	p.setFont(title);
	p.drawText(left, top + title->ascent, name);
	p.setPen(st::windowSubTextFg);
	p.setFont(about);
	p.drawText(
		left,
		top + title->height + gap + about->ascent,
		about->elided(
			(_self
				? tr::lng_moligram_club_you
				: tr::lng_moligram_club_also)(tr::now),
			available));
}

BannerPicker::BannerPicker(
	QWidget *parent,
	Fn<void()> chooseColors,
	Fn<void()> chooseImage)
: RpWidget(parent)
, _chooseColors(std::move(chooseColors))
, _chooseImage(std::move(chooseImage))
, _selected(selectedIndex()) {
	setMouseTracking(true);
	resize(width(), resizeGetHeight(width()));
	ClubBannerValue() | rpl::skip(1) | rpl::on_next([=] {
		const auto now = selectedIndex();
		if (now == _selected) {
			update();
			return;
		}
		_previous = std::exchange(_selected, now);
		_selection.stop();
		_selection.start(
			[=] { update(); },
			0.,
			1.,
			kSelectDuration,
			anim::easeOutCubic);
	}, lifetime());
	Changes() | rpl::on_next([=] { update(); }, lifetime());
}

int BannerPicker::count() const {
	return int(BannerPresets().size()) + 2;
}

int BannerPicker::valueAt(int index) const {
	const auto presets = int(BannerPresets().size());
	return (index < presets)
		? index
		: (index == presets)
		? kBannerColors
		: kBannerImage;
}

int BannerPicker::selectedIndex() const {
	const auto banner = ClubBanner();
	for (auto i = 0; i != count(); ++i) {
		if (valueAt(i) == banner) {
			return i;
		}
	}
	return 0;
}

int BannerPicker::resizeGetHeight(int newWidth) {
	return style::ConvertScale(kPickerHeight);
}

QRectF BannerPicker::swatch(int index) const {
	const auto total = count();
	const auto left = st::settingsButtonNoIcon.padding.left();
	const auto right = st::settingsButtonNoIcon.padding.right();
	const auto gap = style::ConvertScale(kSwatchGap);
	const auto available = width() - left - right - gap * (total - 1);
	const auto w = std::min(
		float64(style::ConvertScale(kSwatchWidth)),
		std::max(available / float64(total), 8.));
	const auto h = float64(style::ConvertScale(kSwatchHeight));
	return QRectF(left + index * (w + gap), (height() - h) / 2., w, h);
}

int BannerPicker::indexAt(QPoint point) const {
	for (auto i = 0; i != count(); ++i) {
		if (swatch(i).marginsAdded({ 3, 3, 3, 3 }).contains(point)) {
			return i;
		}
	}
	return -1;
}

void BannerPicker::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);
	const auto radius = float64(style::ConvertScale(kSwatchRadius));
	const auto progress = _selection.value(1.);
	const auto presets = int(BannerPresets().size());
	for (auto i = 0; i != count(); ++i) {
		const auto rect = swatch(i);
		p.setPen(Qt::NoPen);
		if (i < presets) {
			const auto &preset = BannerPresets()[i];
			auto gradient = QLinearGradient(rect.topLeft(), rect.bottomRight());
			gradient.setColorAt(0., preset.from);
			gradient.setColorAt(1., preset.to);
			p.setBrush(gradient);
			p.drawRoundedRect(rect, radius, radius);
		} else if (i == presets) {
			const auto [from, to] = ClubColors();
			auto gradient = QLinearGradient(rect.topLeft(), rect.bottomRight());
			gradient.setColorAt(0., from);
			gradient.setColorAt(1., to);
			p.setBrush(gradient);
			p.drawRoundedRect(rect, radius, radius);
			p.setPen(QPen(QColor(255, 255, 255, 230), 2.));
			const auto c = rect.center();
			const auto half = rect.height() * 0.2;
			p.drawLine(QPointF(c.x() - half, c.y()), QPointF(c.x() + half, c.y()));
			p.drawLine(QPointF(c.x(), c.y() - half), QPointF(c.x(), c.y() + half));
		} else {
			p.setBrush(st::windowBgOver);
			p.drawRoundedRect(rect, radius, radius);
			auto pen = QPen(st::windowSubTextFg->c, 1.6);
			pen.setJoinStyle(Qt::RoundJoin);
			p.setPen(pen);
			p.setBrush(Qt::NoBrush);
			const auto frame = rect.marginsRemoved({
				rect.width() * 0.26,
				rect.height() * 0.22,
				rect.width() * 0.26,
				rect.height() * 0.22 });
			p.drawRoundedRect(frame, 2., 2.);
			auto mountain = QPainterPath();
			mountain.moveTo(frame.left() + 1, frame.bottom() - 1);
			mountain.lineTo(frame.left() + frame.width() * 0.4, frame.top() + frame.height() * 0.45);
			mountain.lineTo(frame.left() + frame.width() * 0.62, frame.bottom() - frame.height() * 0.3);
			mountain.lineTo(frame.left() + frame.width() * 0.78, frame.top() + frame.height() * 0.6);
			mountain.lineTo(frame.right() - 1, frame.bottom() - 1);
			p.drawPath(mountain);
		}
		const auto active = (i == _selected)
			? progress
			: (i == _previous)
			? (1. - progress)
			: 0.;
		const auto hover = (i == _hovered && i != _selected) ? 0.35 : 0.;
		const auto ring = std::max(active, hover);
		if (ring > 0.) {
			auto color = st::windowActiveTextFg->c;
			color.setAlphaF(ring);
			p.setPen(QPen(color, style::ConvertScale(2) * 1.));
			p.setBrush(Qt::NoBrush);
			const auto outer = rect.marginsAdded({ 3., 3., 3., 3. });
			p.drawRoundedRect(outer, radius + 3., radius + 3.);
		}
		if (active > 0. && i < presets) {
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(255, 255, 255, int(235 * active)));
			const auto dot = style::ConvertScale(3) * active;
			p.drawEllipse(rect.center(), dot, dot);
		}
	}
}

void BannerPicker::mouseMoveEvent(QMouseEvent *e) {
	const auto index = indexAt(e->pos());
	setCursor((index >= 0) ? style::cur_pointer : style::cur_default);
	if (_hovered != index) {
		_hovered = index;
		update();
	}
}

void BannerPicker::leaveEventHook(QEvent *e) {
	if (_hovered >= 0) {
		_hovered = -1;
		update();
	}
}

void BannerPicker::mousePressEvent(QMouseEvent *e) {
	const auto index = indexAt(e->pos());
	if (index < 0) {
		return;
	}
	const auto value = valueAt(index);
	if (value == kBannerColors) {
		_chooseColors();
	} else if (value == kBannerImage) {
		_chooseImage();
	} else {
		SetClubBanner(value);
	}
}

} // namespace Moligram
