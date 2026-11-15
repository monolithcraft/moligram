/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

class History;
class HistoryItem;

namespace Main {
class Session;
} // namespace Main

namespace Ui {
struct SimpleBubble;
} // namespace Ui

namespace Moligram {

struct Revision {
	TextWithEntities text;
	TimeId date = 0;
};

struct DeletedRecord {
	MsgId id = 0;
	TimeId date = 0;
	TimeId deleted = 0;
	QString author;
	QString text;
};

void AttachArchive(not_null<Main::Session*> session);

[[nodiscard]] bool KeepDeleted(not_null<HistoryItem*> item);
void MarkDeleted(not_null<HistoryItem*> item);
[[nodiscard]] bool IsDeleted(not_null<const HistoryItem*> item);

void RememberRevision(
	not_null<HistoryItem*> item,
	const TextWithEntities &next);
[[nodiscard]] std::vector<Revision> Revisions(
	not_null<const HistoryItem*> item);

void Forget(not_null<const HistoryItem*> item);

[[nodiscard]] std::vector<DeletedRecord> DeletedIn(
	not_null<History*> history);
[[nodiscard]] HistoryItem *LiveDeleted(
	not_null<History*> history,
	MsgId id);
void ClearDeleted(not_null<History*> history);

[[nodiscard]] std::optional<QColor> MarkColor(
	not_null<const HistoryItem*> item);
void PaintMarks(
	QPainter &p,
	const Ui::SimpleBubble &bubble,
	not_null<const HistoryItem*> item);
void PaintAccent(
	QPainter &p,
	const Ui::SimpleBubble &bubble,
	QColor color);

} // namespace Moligram
