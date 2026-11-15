/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_settings.h"

#include "base/timer.h"
#include "lang/lang_keys.h"
#include "settings.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtGui/QFontDatabase>

namespace Moligram {
namespace {

constexpr auto kSaveDelay = crl::time(500);
constexpr auto kVersion = 3;

struct Entry {
	Option option;
	const char *key;
	bool fallback;
};

constexpr auto kEntries = std::array{
	Entry{ Option::SendReadMessages, "send_read_messages", true },
	Entry{ Option::SendReadStories, "send_read_stories", true },
	Entry{ Option::SendOnlineStatus, "send_online_status", true },
	Entry{ Option::SendTyping, "send_typing", true },
	Entry{ Option::SendUploadProgress, "send_upload_progress", true },
	Entry{ Option::ReadAfterSending, "read_after_sending", true },
	Entry{ Option::KeepDeletedMessages, "keep_deleted_messages", true },
	Entry{ Option::KeepEditHistory, "keep_edit_history", true },
	Entry{ Option::StreamerMode, "streamer_mode", false },
	Entry{ Option::ShowMessageSeconds, "show_message_seconds", false },
	Entry{ Option::ShowPeerIds, "show_peer_ids", false },
	Entry{ Option::HideStories, "hide_stories", false },
	Entry{ Option::GhostInMainMenu, "ghost_in_main_menu", true },
	Entry{ Option::ConfirmStickerSending, "confirm_sticker_sending", false },
	Entry{ Option::SilentSending, "silent_sending", false },
	Entry{ Option::DeletionAlerts, "deletion_alerts", false },
	Entry{ Option::HighlightEdited, "highlight_edited", true },
	Entry{ Option::ClubMember, "club_member", true },
	Entry{ Option::SendListened, "send_listened", true },
	Entry{ Option::CleanLinks, "clean_links", true },
	Entry{ Option::LockOnMinimize, "lock_on_minimize", false },
	Entry{ Option::PanicKey, "panic_key", true },
	Entry{ Option::ForwardNoAuthor, "forward_no_author", false },
	Entry{ Option::ClubBadges, "club_badges", true },
	Entry{ Option::CompactDialogs, "compact_dialogs", false },
	Entry{ Option::HoverProfiles, "hover_profiles", true },
	Entry{ Option::DoNotDisturb, "do_not_disturb", false },
	Entry{ Option::QuickDnd, "quick_dnd", true },
	Entry{ Option::QuickPanel, "quick_panel", true },
	Entry{ Option::QuickGhost, "quick_ghost", true },
	Entry{ Option::QuickStreamer, "quick_streamer", true },
	Entry{ Option::QuickHideStories, "quick_hide_stories", true },
	Entry{ Option::QuickSeconds, "quick_seconds", false },
	Entry{ Option::QuickPeerIds, "quick_peer_ids", false },
	Entry{ Option::QuickKeepDeleted, "quick_keep_deleted", false },
	Entry{ Option::QuickKeepEdits, "quick_keep_edits", false },
	Entry{ Option::QuickSilent, "quick_silent", false },
};
static_assert(kEntries.size() == size_t(Option::Count));

constexpr auto kGhostOptions = std::array{
	Option::SendReadMessages,
	Option::SendReadStories,
	Option::SendOnlineStatus,
	Option::SendTyping,
	Option::SendUploadProgress,
};

constexpr auto kHighlightKeys = std::array{
	"deleted_color",
	"edited_color",
};

[[nodiscard]] std::array<QColor, 2> DefaultHighlights() {
	return { QColor(0xef, 0x44, 0x44), QColor(0xfa, 0xcc, 0x15) };
}

constexpr auto kTextKeys = std::array{
	"nicknames",
	"notes",
};

constexpr auto kListKeys = std::array{
	"ghost_exceptions",
	"online_alerts",
};

class Store final {
public:
	Store() : _saveTimer([=] { save(); }) {
		for (const auto &entry : kEntries) {
			_values[size_t(entry.option)] = entry.fallback;
		}
	}

	[[nodiscard]] rpl::variable<bool> &at(Option option) {
		return _values[size_t(option)];
	}

	[[nodiscard]] base::flat_set<PeerId> &list(PeerList list) {
		return _lists[size_t(list)];
	}

	[[nodiscard]] QStringList &words() {
		return _words;
	}

	[[nodiscard]] base::flat_map<PeerId, QString> &texts(PeerText which) {
		return _texts[size_t(which)];
	}

	[[nodiscard]] std::pair<QColor, QColor> &clubColors() {
		return _clubColors;
	}

	[[nodiscard]] MsgId &clubImage() {
		return _clubImage;
	}

	[[nodiscard]] int &radius() {
		return _radius;
	}

	[[nodiscard]] int &stickers() {
		return _stickers;
	}

	[[nodiscard]] rpl::variable<int> &banner() {
		return _banner;
	}

	[[nodiscard]] QColor &color(Highlight which) {
		return _colors[size_t(which)];
	}

	void load() {
		auto file = QFile(path());
		if (!file.open(QIODevice::ReadOnly)) {
			return;
		}
		const auto object = QJsonDocument::fromJson(file.readAll()).object();
		const auto version = object.value(u"version"_q).toInt();
		for (const auto &entry : kEntries) {
			const auto key = QLatin1String(entry.key);
			if ((version < 2 && key.startsWith(u"quick_"_q))
				|| (version < 3 && key == u"club_member"_q)) {
				continue;
			}
			const auto value = object.value(QLatin1String(entry.key));
			if (value.isBool()) {
				at(entry.option) = value.toBool();
			}
		}
		for (auto i = 0; i != int(kHighlightKeys.size()); ++i) {
			const auto value = QColor::fromString(
				object.value(QLatin1String(kHighlightKeys[i])).toString());
			if (value.isValid()) {
				_colors[i] = value;
			}
		}
		_banner = std::max(object.value(u"club_banner"_q).toInt(), 0);
		{
			const auto from = QColor::fromString(
				object.value(u"club_from"_q).toString());
			const auto to = QColor::fromString(
				object.value(u"club_to"_q).toString());
			if (from.isValid() && to.isValid()) {
				_clubColors = { from, to };
			}
			_clubImage = MsgId(int64(object.value(u"club_image"_q).toDouble()));
		}
		_radius = std::clamp(object.value(u"bubble_radius"_q).toInt(-1), -1, 24);
		_stickers = std::clamp(object.value(u"sticker_size"_q).toInt(-1), -1, 512);
		for (auto i = 0; i != int(kTextKeys.size()); ++i) {
			const auto map = object.value(QLatin1String(kTextKeys[i])).toObject();
			for (auto j = map.begin(); j != map.end(); ++j) {
				const auto id = PeerId(j.key().toULongLong());
				const auto text = j.value().toString();
				if (id && !text.isEmpty()) {
					_texts[i].emplace(id, text);
				}
			}
		}
		for (const auto &word : object.value(u"filter_words"_q).toArray()) {
			if (const auto text = word.toString().trimmed(); !text.isEmpty()) {
				_words.push_back(text);
			}
		}
		for (auto i = 0; i != int(kListKeys.size()); ++i) {
			const auto ids = object.value(QLatin1String(kListKeys[i])).toArray();
			for (const auto &id : ids) {
				_lists[i].emplace(PeerId(id.toString().toULongLong()));
			}
		}
	}

	void scheduleSave() {
		_saveTimer.callOnce(kSaveDelay);
		_changes.fire({});
	}

	void saveNow() {
		_saveTimer.cancel();
		save();
		_changes.fire({});
	}

	[[nodiscard]] rpl::producer<> changes() const {
		return _changes.events();
	}

private:
	[[nodiscard]] static QString path() {
		return cWorkingDir() + u"tdata/moligram.json"_q;
	}

	void save() {
		auto object = QJsonObject();
		object.insert(u"version"_q, kVersion);
		for (const auto &entry : kEntries) {
			object.insert(
				QLatin1String(entry.key),
				_values[size_t(entry.option)].current());
		}
		for (auto i = 0; i != int(kListKeys.size()); ++i) {
			auto ids = QJsonArray();
			for (const auto &peer : _lists[i]) {
				ids.push_back(QString::number(peer.value));
			}
			object.insert(QLatin1String(kListKeys[i]), ids);
		}
		for (auto i = 0; i != int(kHighlightKeys.size()); ++i) {
			object.insert(
				QLatin1String(kHighlightKeys[i]),
				_colors[i].name(QColor::HexRgb));
		}
		object.insert(u"club_banner"_q, _banner.current());
		object.insert(u"club_from"_q, _clubColors.first.name(QColor::HexRgb));
		object.insert(u"club_to"_q, _clubColors.second.name(QColor::HexRgb));
		object.insert(u"club_image"_q, double(_clubImage.bare));
		object.insert(u"bubble_radius"_q, _radius);
		object.insert(u"sticker_size"_q, _stickers);
		for (auto i = 0; i != int(kTextKeys.size()); ++i) {
			auto map = QJsonObject();
			for (const auto &[id, text] : _texts[i]) {
				map.insert(QString::number(id.value), text);
			}
			object.insert(QLatin1String(kTextKeys[i]), map);
		}
		object.insert(u"filter_words"_q, QJsonArray::fromStringList(_words));
		auto file = QSaveFile(path());
		if (file.open(QIODevice::WriteOnly)) {
			file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
			file.commit();
		}
	}

	std::array<rpl::variable<bool>, size_t(Option::Count)> _values;
	std::array<base::flat_set<PeerId>, 2> _lists;
	QStringList _words;
	std::array<base::flat_map<PeerId, QString>, 2> _texts;
	std::array<QColor, 2> _colors = DefaultHighlights();
	rpl::variable<int> _banner = 0;
	std::pair<QColor, QColor> _clubColors = {
		QColor(0x7c, 0x3a, 0xed),
		QColor(0x06, 0xb6, 0xd4),
	};
	MsgId _clubImage = 0;
	int _radius = -1;
	int _stickers = -1;
	rpl::event_stream<> _changes;
	base::Timer _saveTimer;

};

[[nodiscard]] Store &Storage() {
	static auto instance = Store();
	return instance;
}

} // namespace

std::pair<QColor, QColor> ClubColors() {
	return Storage().clubColors();
}

void SetClubColors(QColor from, QColor to) {
	Storage().clubColors() = { from, to };
	Storage().scheduleSave();
}

MsgId ClubImage() {
	return Storage().clubImage();
}

void SetClubImage(MsgId id) {
	Storage().clubImage() = id;
	Storage().scheduleSave();
}

int StickerSize() {
	return Storage().stickers();
}

void SetStickerSize(int size) {
	if (Storage().stickers() != size) {
		Storage().stickers() = size;
		Storage().scheduleSave();
	}
}

int BubbleRadius() {
	return Storage().radius();
}

void SetBubbleRadius(int radius) {
	if (Storage().radius() != radius) {
		Storage().radius() = radius;
		Storage().scheduleSave();
	}
}

int ClubBanner() {
	return Storage().banner().current();
}

void SetClubBanner(int banner) {
	if (Storage().banner().current() != banner) {
		Storage().banner() = banner;
		Storage().scheduleSave();
	}
}

rpl::producer<int> ClubBannerValue() {
	return Storage().banner().value();
}

bool Enabled(Option option) {
	return Storage().at(option).current();
}

void Set(Option option, bool enabled) {
	auto &value = Storage().at(option);
	if (value.current() != enabled) {
		value = enabled;
		Storage().saveNow();
	}
}

rpl::producer<bool> Value(Option option) {
	return Storage().at(option).value();
}

rpl::producer<> Changes() {
	return Storage().changes();
}

bool InList(PeerList list, PeerId peer) {
	return Storage().list(list).contains(peer);
}

void SetInList(PeerList list, PeerId peer, bool included) {
	auto &ids = Storage().list(list);
	const auto changed = included
		? ids.emplace(peer).second
		: (ids.remove(peer) > 0);
	if (changed) {
		Storage().scheduleSave();
	}
}

std::vector<PeerId> ListPeers(PeerList list) {
	const auto &ids = Storage().list(list);
	return { ids.begin(), ids.end() };
}

bool SendsReads(PeerId peer) {
	return Enabled(Option::SendReadMessages)
		|| InList(PeerList::GhostExceptions, peer);
}

bool SendsTyping(PeerId peer) {
	return Enabled(Option::SendTyping)
		|| InList(PeerList::GhostExceptions, peer);
}

QStringList FilterWords() {
	return Storage().words();
}

QString PeerTextValue(PeerText which, PeerId peer) {
	const auto &map = Storage().texts(which);
	const auto i = map.find(peer);
	return (i != end(map)) ? i->second : QString();
}

void SetPeerText(PeerText which, PeerId peer, QString text) {
	text = text.trimmed();
	auto &map = Storage().texts(which);
	if (PeerTextValue(which, peer) == text) {
		return;
	} else if (text.isEmpty()) {
		map.remove(peer);
	} else {
		map[peer] = text;
	}
	Storage().scheduleSave();
}

void SetFilterWords(QStringList words) {
	for (auto &word : words) {
		word = word.trimmed();
	}
	words.removeAll(QString());
	words.removeDuplicates();
	if (Storage().words() != words) {
		Storage().words() = std::move(words);
		Storage().scheduleSave();
	}
}

bool Filtered(const QString &text) {
	if (text.isEmpty()) {
		return false;
	}
	for (const auto &word : Storage().words()) {
		if (text.contains(word, Qt::CaseInsensitive)) {
			return true;
		}
	}
	return false;
}

QColor HighlightColor(Highlight which) {
	return Storage().color(which);
}

void SetHighlightColor(Highlight which, QColor color) {
	auto &current = Storage().color(which);
	if (color.isValid() && current != color) {
		current = color;
		Storage().scheduleSave();
	}
}

const std::vector<QColor> &HighlightPalette() {
	static const auto result = std::vector<QColor>{
		QColor(0xef, 0x44, 0x44),
		QColor(0xf9, 0x73, 0x16),
		QColor(0xfa, 0xcc, 0x15),
		QColor(0x22, 0xc5, 0x5e),
		QColor(0x14, 0xb8, 0xa6),
		QColor(0x3b, 0x82, 0xf6),
		QColor(0x8b, 0x5c, 0xf6),
		QColor(0xec, 0x48, 0x99),
	};
	return result;
}

bool GhostMode() {
	return ranges::none_of(kGhostOptions, Enabled);
}

void SetGhostMode(bool enabled) {
	for (const auto option : kGhostOptions) {
		Set(option, !enabled);
	}
}

rpl::producer<bool> GhostModeValue() {
	return rpl::single(rpl::empty) | rpl::then(Changes()) | rpl::map([] {
		return GhostMode();
	}) | rpl::distinct_until_changed();
}

QString FormatMessageTime(QTime time) {
	if (!Enabled(Option::ShowMessageSeconds)) {
		return QLocale().toString(time, QLocale::ShortFormat);
	}
	auto format = QLocale().timeFormat(QLocale::ShortFormat);
	if (!format.contains(u"ss"_q)) {
		format.replace(u"mm"_q, u"mm:ss"_q);
	}
	return QLocale().toString(time, format);
}

QString InterfaceFont(const QString &chosen) {
	if (!chosen.isEmpty()) {
		return chosen;
	}
	static const auto registered = [] {
		auto result = QString();
		for (const auto &name : { u"Nunito.ttf"_q, u"Nunito-Italic.ttf"_q }) {
			const auto id = QFontDatabase::addApplicationFont(
				u":/gui/fonts/"_q + name);
			const auto families = QFontDatabase::applicationFontFamilies(id);
			if (result.isEmpty() && !families.isEmpty()) {
				result = families.front();
			}
		}
		return result;
	}();
	return registered;
}

void Load() {
	Storage().load();
}

} // namespace Moligram
