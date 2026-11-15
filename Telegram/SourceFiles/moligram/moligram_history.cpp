/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_history.h"

#include "base/timer.h"
#include "base/unixtime.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "history/view/history_view_item_preview.h"
#include "main/main_session.h"
#include "moligram/moligram_alerts.h"
#include "moligram/moligram_settings.h"
#include "storage/details/storage_file_utilities.h"
#include "storage/storage_account.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/chat_style_radius.h"
#include "ui/chat/message_bubble.h"
#include "ui/painter.h"
#include "styles/style_window.h"

namespace Moligram {
namespace {

using namespace Storage::details;

constexpr auto kFileName = "moligram_history";
constexpr auto kFormatVersion = qint32(1);
constexpr auto kRevisionsLimit = 50;
constexpr auto kDeletedPerChatLimit = 3000;
constexpr auto kSaveDelay = crl::time(1500);
constexpr auto kMarkTint = 0.22;

struct LiveKey {
	const History *history = nullptr;
	MsgId id;

	friend inline auto operator<=>(const LiveKey &, const LiveKey &) = default;
	friend inline bool operator==(const LiveKey &, const LiveKey &) = default;
};

using RevisionKey = std::pair<PeerId, MsgId>;

class Archive final {
public:
	explicit Archive(not_null<Main::Session*> session)
	: _session(session)
	, _saveTimer([=] { save(); }) {
	}

	~Archive() {
		if (_saveTimer.isActive()) {
			save();
		}
	}

	[[nodiscard]] std::vector<DeletedRecord> &deleted(PeerId peer) {
		return _deleted[peer];
	}

	[[nodiscard]] std::vector<Revision> &revisions(RevisionKey key) {
		return _revisions[key];
	}

	[[nodiscard]] const std::vector<Revision> *findRevisions(
			RevisionKey key) const {
		const auto i = _revisions.find(key);
		return (i != end(_revisions)) ? &i->second : nullptr;
	}

	void forgetPeer(PeerId peer) {
		_deleted.remove(peer);
		changed();
	}

	void changed() {
		_saveTimer.callOnce(kSaveDelay);
	}

	void load() {
		const auto key = _session->local().peekLegacyLocalKey();
		if (!key) {
			return;
		}
		auto file = FileReadDescriptor();
		if (!ReadEncryptedFile(
				file,
				QString::fromLatin1(kFileName),
				_session->local().basePath(),
				key)) {
			return;
		}
		auto &stream = file.stream;
		auto version = qint32();
		stream >> version;
		if (version != kFormatVersion) {
			return;
		}
		auto peers = quint32();
		stream >> peers;
		for (auto i = quint32(); i != peers && stream.status() == QDataStream::Ok; ++i) {
			auto peer = quint64();
			auto count = quint32();
			stream >> peer >> count;
			auto &list = _deleted[PeerId(peer)];
			for (auto j = quint32(); j != count && stream.status() == QDataStream::Ok; ++j) {
				auto record = DeletedRecord();
				auto id = qint64();
				auto date = qint32();
				auto deleted = qint32();
				stream >> id >> date >> deleted >> record.author >> record.text;
				record.id = MsgId(id);
				record.date = date;
				record.deleted = deleted;
				list.push_back(std::move(record));
			}
		}
		auto edited = quint32();
		stream >> edited;
		for (auto i = quint32(); i != edited && stream.status() == QDataStream::Ok; ++i) {
			auto peer = quint64();
			auto id = qint64();
			auto count = quint32();
			stream >> peer >> id >> count;
			auto &list = _revisions[{ PeerId(peer), MsgId(id) }];
			for (auto j = quint32(); j != count && stream.status() == QDataStream::Ok; ++j) {
				auto text = QString();
				auto date = qint32();
				stream >> text >> date;
				list.push_back({ .text = { text }, .date = date });
			}
		}
		if (stream.status() != QDataStream::Ok) {
			_deleted.clear();
			_revisions.clear();
		}
	}

private:
	void save() {
		_saveTimer.cancel();
		const auto key = _session->local().peekLegacyLocalKey();
		if (!key) {
			return;
		}
		auto payload = QByteArray();
		auto stream = QDataStream(&payload, QIODevice::WriteOnly);
		stream.setVersion(QDataStream::Qt_5_1);
		stream << kFormatVersion;
		stream << quint32(_deleted.size());
		for (const auto &[peer, list] : _deleted) {
			stream << quint64(peer.value) << quint32(list.size());
			for (const auto &record : list) {
				stream
					<< qint64(record.id.bare)
					<< qint32(record.date)
					<< qint32(record.deleted)
					<< record.author
					<< record.text;
			}
		}
		stream << quint32(_revisions.size());
		for (const auto &[key, list] : _revisions) {
			stream
				<< quint64(key.first.value)
				<< qint64(key.second.bare)
				<< quint32(list.size());
			for (const auto &revision : list) {
				stream << revision.text.text << qint32(revision.date);
			}
		}
		auto data = EncryptedDescriptor(payload.size());
		data.stream.writeRawData(payload.constData(), payload.size());
		auto file = FileWriteDescriptor(
			QString::fromLatin1(kFileName),
			_session->local().basePath());
		file.writeEncrypted(data, key);
	}

	const not_null<Main::Session*> _session;
	base::flat_map<PeerId, std::vector<DeletedRecord>> _deleted;
	base::flat_map<RevisionKey, std::vector<Revision>> _revisions;
	base::Timer _saveTimer;

};

struct Registry {
	base::flat_map<not_null<Main::Session*>, std::unique_ptr<Archive>> archives;
	base::flat_set<LiveKey> deleted;
	bool clearing = false;
};

[[nodiscard]] Registry &Data() {
	static auto registry = Registry();
	return registry;
}

[[nodiscard]] Archive *ArchiveFor(not_null<const HistoryItem*> item) {
	const auto &archives = Data().archives;
	const auto i = archives.find(&item->history()->session());
	return (i != end(archives)) ? i->second.get() : nullptr;
}

[[nodiscard]] LiveKey LiveKeyOf(not_null<const HistoryItem*> item) {
	return { .history = item->history().get(), .id = item->id };
}

[[nodiscard]] QString PreviewText(not_null<HistoryItem*> item) {
	const auto &text = item->originalText();
	return text.empty()
		? item->toPreview(HistoryView::ToPreviewOptions()).text.text
		: text.text;
}

} // namespace

void AttachArchive(not_null<Main::Session*> session) {
	const auto archive = Data().archives.emplace(
		session,
		std::make_unique<Archive>(session)).first->second.get();
	crl::on_main(session, [=] {
		archive->load();
	});
	session->lifetime().add([=] {
		Data().archives.remove(session);
	});
}

bool KeepDeleted(not_null<HistoryItem*> item) {
	return !Data().clearing
		&& Enabled(Option::KeepDeletedMessages)
		&& item->isRegular()
		&& !item->isService()
		&& !item->isSponsored();
}

void MarkDeleted(not_null<HistoryItem*> item) {
	if (!Data().deleted.emplace(LiveKeyOf(item)).second) {
		return;
	}
	if (const auto archive = ArchiveFor(item)) {
		auto &list = archive->deleted(item->history()->peer->id);
		list.push_back({
			.id = item->id,
			.date = item->date(),
			.deleted = base::unixtime::now(),
			.author = item->from()->name(),
			.text = PreviewText(item),
		});
		if (list.size() > kDeletedPerChatLimit) {
			list.erase(begin(list));
		}
		archive->changed();
	}
	NotifyDeleted(item);
	item->history()->owner().notifyItemDataChange(item);
}

bool IsDeleted(not_null<const HistoryItem*> item) {
	return Data().deleted.contains(LiveKeyOf(item));
}

void RememberRevision(
		not_null<HistoryItem*> item,
		const TextWithEntities &next) {
	if (!Enabled(Option::KeepEditHistory) || !item->isRegular()) {
		return;
	}
	const auto &current = item->originalText();
	if (current.text == next.text || current.empty()) {
		return;
	}
	const auto archive = ArchiveFor(item);
	if (!archive) {
		return;
	}
	const auto edited = item->Get<HistoryMessageEdited>();
	auto &list = archive->revisions({ item->history()->peer->id, item->id });
	list.push_back({
		.text = current,
		.date = (edited && edited->date) ? edited->date : item->date(),
	});
	if (list.size() > kRevisionsLimit) {
		list.erase(begin(list));
	}
	archive->changed();
}

std::vector<Revision> Revisions(not_null<const HistoryItem*> item) {
	if (const auto archive = ArchiveFor(item)) {
		const auto key = RevisionKey{ item->history()->peer->id, item->id };
		if (const auto list = archive->findRevisions(key)) {
			return *list;
		}
	}
	return {};
}

void Forget(not_null<const HistoryItem*> item) {
	Data().deleted.remove(LiveKeyOf(item));
}

std::vector<DeletedRecord> DeletedIn(not_null<History*> history) {
	const auto &archives = Data().archives;
	const auto i = archives.find(&history->session());
	if (i == end(archives)) {
		return {};
	}
	auto result = i->second->deleted(history->peer->id);
	ranges::sort(result, ranges::less(), &DeletedRecord::id);
	return result;
}

HistoryItem *LiveDeleted(not_null<History*> history, MsgId id) {
	const auto key = LiveKey{ .history = history.get(), .id = id };
	return Data().deleted.contains(key)
		? history->owner().message(history->peer->id, id)
		: nullptr;
}

void ClearDeleted(not_null<History*> history) {
	auto ids = QVector<MTPint>();
	for (const auto &record : DeletedIn(history)) {
		if (LiveDeleted(history, record.id)) {
			ids.push_back(MTP_int(record.id.bare));
		}
	}
	const auto &archives = Data().archives;
	if (const auto i = archives.find(&history->session()); i != end(archives)) {
		i->second->forgetPeer(history->peer->id);
	}
	if (!ids.isEmpty()) {
		Data().clearing = true;
		history->owner().processMessagesDeleted(history->peer->id, ids);
		Data().clearing = false;
	}
}

std::optional<QColor> MarkColor(not_null<const HistoryItem*> item) {
	if (IsDeleted(item)) {
		return HighlightColor(Highlight::Deleted);
	} else if (!Enabled(Option::HighlightEdited)) {
		return std::nullopt;
	}
	const auto edited = item->Get<HistoryMessageEdited>();
	if (!edited || edited->date <= 0 || item->hideEditedBadge()) {
		return std::nullopt;
	}
	const auto user = item->from()->asUser();
	if (user && user->isBot()) {
		return std::nullopt;
	}
	return HighlightColor(Highlight::Edited);
}

void PaintMarks(
		QPainter &p,
		const Ui::SimpleBubble &bubble,
		not_null<const HistoryItem*> item) {
	if (const auto color = MarkColor(item)) {
		PaintAccent(p, bubble, *color);
	}
}

void PaintAccent(
		QPainter &p,
		const Ui::SimpleBubble &bubble,
		QColor color) {
	using Corner = Ui::BubbleCornerRounding;
	const auto radius = [](Corner corner) -> float64 {
		return (corner == Corner::Large)
			? Ui::BubbleRadiusLarge()
			: (corner == Corner::Small)
			? Ui::BubbleRadiusSmall()
			: 0.;
	};
	const auto rect = QRectF(bubble.geometry);
	const auto &rounding = bubble.rounding;
	const auto tl = radius(rounding.topLeft);
	const auto tr = radius(rounding.topRight);
	const auto br = radius(rounding.bottomRight);
	const auto bl = radius(rounding.bottomLeft);
	auto path = QPainterPath();
	path.moveTo(rect.left() + tl, rect.top());
	path.lineTo(rect.right() + 1 - tr, rect.top());
	path.arcTo(rect.right() + 1 - tr * 2, rect.top(), tr * 2, tr * 2, 90, -90);
	path.lineTo(rect.right() + 1, rect.bottom() + 1 - br);
	path.arcTo(rect.right() + 1 - br * 2, rect.bottom() + 1 - br * 2, br * 2, br * 2, 0, -90);
	path.lineTo(rect.left() + bl, rect.bottom() + 1);
	path.arcTo(rect.left(), rect.bottom() + 1 - bl * 2, bl * 2, bl * 2, 270, -90);
	path.lineTo(rect.left(), rect.top() + tl);
	path.arcTo(rect.left(), rect.top(), tl * 2, tl * 2, 180, -90);
	path.closeSubpath();

	auto hq = PainterHighQualityEnabler(p);
	auto tint = color;
	tint.setAlphaF(kMarkTint);
	p.setPen(Qt::NoPen);
	p.setBrush(tint);
	p.drawPath(path);

	const auto &st = bubble.st->messageStyle(bubble.outbg, bubble.selected);
	const auto tailRight = (rounding.bottomRight == Corner::Tail);
	if (tailRight || rounding.bottomLeft == Corner::Tail) {
		const auto &tail = tailRight ? st.tailRight : st.tailLeft;
		const auto position = tailRight
			? QPoint(
				bubble.geometry.x() + bubble.geometry.width(),
				bubble.geometry.y() + bubble.geometry.height() - tail.height())
			: QPoint(
				bubble.geometry.x() - tail.width(),
				bubble.geometry.y() + bubble.geometry.height() - tail.height());
		const auto was = p.opacity();
		p.setOpacity(was * kMarkTint);
		tail.paint(p, position, bubble.outerWidth, color);
		p.setOpacity(was);
	}

}

} // namespace Moligram
