/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_log.h"

#include "base/unixtime.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "lang/lang_keys.h"
#include "moligram/moligram_history.h"
#include "moligram/moligram_settings.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/chat_theme.h"
#include "ui/chat/message_bubble.h"
#include "ui/painter.h"
#include "ui/text/text_options.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/shadow.h"
#include "window/section_memento.h"
#include "window/section_widget.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h"
#include "styles/style_dialogs.h"
#include "styles/style_info.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

namespace Moligram {
namespace {

constexpr auto kBubbleSkip = 6;
constexpr auto kGroupSkip = 14;
constexpr auto kFooterSkip = 4;
constexpr auto kNameSkip = 3;
constexpr auto kListPadding = 12;

struct Entry {
	QString author;
	TextWithEntities text;
	TimeId date = 0;
	QString caption;
	std::optional<QColor> accent;
	MsgId jumpTo = 0;
};

struct LogData {
	not_null<History*> history;
	QString title;
	QString subtitle;
	QString empty;
	std::vector<Entry> entries;
	bool searchable = false;
};

class LogList final : public Ui::RpWidget {
public:
	LogList(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		std::shared_ptr<LogData> data);

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	struct Item {
		Ui::Text::String text;
		QString footer;
		int footerWidth = 0;
		QRect bubble;
		std::optional<QRect> date;
		QString dateText;
		bool last = false;
		int entry = 0;
	};

public:
	void setQuery(const QString &query);

private:
	void rebuild();
	[[nodiscard]] const Entry &entryAt(int index) const {
		return _data->entries[_items[index].entry];
	}

	[[nodiscard]] int itemAt(QPoint point) const;
	void paintDate(QPainter &p, const Item &item) const;
	void paintItem(QPainter &p, int index) const;

	const not_null<Window::SessionController*> _controller;
	const std::shared_ptr<LogData> _data;
	std::vector<Item> _items;
	QStringList _query;
	base::unique_qptr<Ui::PopupMenu> _menu;
	int _over = -1;

};

LogList::LogList(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	std::shared_ptr<LogData> data)
: RpWidget(parent)
, _controller(controller)
, _data(std::move(data)) {
	setMouseTracking(true);
	rebuild();
	controller->chatStyle()->paletteChanged(
	) | rpl::on_next([=] {
		update();
	}, lifetime());
}

void LogList::setQuery(const QString &query) {
	auto words = query.simplified().toLower().split(
		' ',
		Qt::SkipEmptyParts);
	if (_query == words) {
		return;
	}
	_query = std::move(words);
	_over = -1;
	rebuild();
	resizeToWidth(width());
	update();
}

void LogList::rebuild() {
	_items.clear();
	_items.reserve(_data->entries.size());
	for (auto i = 0; i != int(_data->entries.size()); ++i) {
		const auto &entry = _data->entries[i];
		if (!_query.isEmpty()) {
			const auto haystack = (entry.text.text
				+ ' '
				+ entry.author).toLower();
			const auto missing = ranges::any_of(_query, [&](const QString &word) {
				return !haystack.contains(word);
			});
			if (missing) {
				continue;
			}
		}
		auto &item = _items.emplace_back();
		item.entry = i;
		item.text = Ui::Text::String(st::msgMinWidth / 2);
		item.text.setMarkedText(
			st::messageTextStyle,
			entry.text,
			Ui::ItemTextDefaultOptions());
		const auto time = QLocale().toString(
			base::unixtime::parse(entry.date).time(),
			QLocale::ShortFormat);
		item.footer = entry.caption.isEmpty()
			? time
			: (entry.caption + u" · "_q + time);
		item.footerWidth = st::msgDateFont->width(item.footer);
	}
}

int LogList::resizeGetHeight(int newWidth) {
	const auto padding = st::msgPadding;
	const auto left = st::msgMargin.left();
	const auto available = std::min(
		st::msgMaxWidth,
		newWidth - left - st::msgMargin.right());
	const auto inner = available - padding.left() - padding.right();
	auto top = style::ConvertScale(kListPadding);
	auto lastDay = QDate();
	for (auto i = 0; i != int(_items.size()); ++i) {
		auto &item = _items[i];
		const auto &entry = entryAt(i);
		const auto day = base::unixtime::parse(entry.date).date();
		item.date.reset();
		if (day != lastDay) {
			lastDay = day;
			item.dateText = langDayOfMonthFull(day);
			const auto &font = st::msgServiceFont;
			const auto &pad = st::msgServicePadding;
			const auto width = font->width(item.dateText)
				+ pad.left()
				+ pad.right();
			const auto height = font->height + pad.top() + pad.bottom();
			item.date = QRect((newWidth - width) / 2, top, width, height);
			top += height + style::ConvertScale(kGroupSkip);
		}
		const auto nameWidth = st::msgNameFont->width(entry.author);
		const auto textWidth = std::min(item.text.maxWidth(), inner);
		const auto contentWidth = std::max({
			textWidth,
			nameWidth,
			item.footerWidth,
			st::msgMinWidth - padding.left() - padding.right(),
		});
		const auto width = std::min(contentWidth, inner);
		const auto textHeight = item.text.countHeight(width);
		const auto height = padding.top()
			+ st::msgNameFont->height
			+ style::ConvertScale(kNameSkip)
			+ textHeight
			+ style::ConvertScale(kFooterSkip)
			+ st::msgDateFont->height
			+ padding.bottom();
		item.bubble = QRect(
			left,
			top,
			width + padding.left() + padding.right(),
			height);
		const auto next = i + 1;
		item.last = (next == int(_items.size()))
			|| (base::unixtime::parse(entryAt(next).date).date() != day)
			|| (entryAt(next).author != entry.author);
		top += height
			+ style::ConvertScale(item.last ? kGroupSkip : kBubbleSkip);
	}
	return top + style::ConvertScale(kListPadding);
}

void LogList::paintDate(QPainter &p, const Item &item) const {
	if (!item.date) {
		return;
	}
	const auto rect = *item.date;
	auto hq = PainterHighQualityEnabler(p);
	p.setPen(Qt::NoPen);
	p.setBrush(st::msgServiceBg);
	const auto radius = rect.height() / 2.;
	p.drawRoundedRect(rect, radius, radius);
	p.setPen(st::msgServiceFg);
	p.setFont(st::msgServiceFont);
	p.drawText(rect, Qt::AlignCenter, item.dateText);
}

void LogList::paintItem(QPainter &p, int index) const {
	const auto &item = _items[index];
	const auto &entry = entryAt(index);
	const auto st = _controller->chatStyle();
	const auto selected = (index == _over) && entry.jumpTo;
	using Corner = Ui::BubbleCornerRounding;
	auto rounding = Ui::BubbleRounding{
		.topLeft = Corner::Large,
		.topRight = Corner::Large,
		.bottomLeft = item.last ? Corner::Tail : Corner::Large,
		.bottomRight = Corner::Large,
	};
	const auto bubble = Ui::SimpleBubble{
		.st = st,
		.geometry = item.bubble,
		.outerWidth = width(),
		.selected = selected,
		.outbg = false,
		.rounding = rounding,
	};
	Ui::PaintBubble(p, bubble);
	if (entry.accent) {
		PaintAccent(p, bubble, *entry.accent);
	}
	const auto &padding = st::msgPadding;
	const auto &style = st->messageStyle(false, selected);
	const auto inner = item.bubble.marginsRemoved(padding);
	auto top = inner.top();
	p.setFont(st::msgNameFont);
	p.setPen(entry.accent ? *entry.accent : style.msgServiceFg->c);
	p.drawText(
		inner.left(),
		top + st::msgNameFont->ascent,
		st::msgNameFont->elided(entry.author, inner.width()));
	top += st::msgNameFont->height + style::ConvertScale(kNameSkip);
	p.setPen(style.historyTextFg);
	item.text.draw(p, {
		.position = { inner.left(), top },
		.outerWidth = width(),
		.availableWidth = inner.width(),
		.palette = &style.textPalette,
	});
	p.setFont(st::msgDateFont);
	p.setPen(style.msgDateFg);
	p.drawText(
		inner.right() + 1 - item.footerWidth,
		inner.bottom() + 1 - st::msgDateFont->descent,
		item.footer);
}

void LogList::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	const auto clip = e->rect();
	if (_items.empty()) {
		const auto &font = st::msgServiceFont;
		const auto &pad = st::msgServicePadding;
		const auto available = std::min(
			width() - style::ConvertScale(48),
			st::msgMaxWidth);
		const auto lines = _query.isEmpty()
			? _data->empty
			: tr::lng_search_tab_no_results(tr::now);
		const auto bounds = QFontMetrics(font->f).boundingRect(
			QRect(0, 0, available, height()),
			Qt::AlignCenter | Qt::TextWordWrap,
			lines);
		p.setFont(font);
		const auto pill = QRect(
			(width() - bounds.width()) / 2 - pad.left(),
			(height() - bounds.height()) / 2 - pad.top(),
			bounds.width() + pad.left() + pad.right(),
			bounds.height() + pad.top() + pad.bottom());
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(st::msgServiceBg);
		const auto radius = std::min(pill.height() / 2., 12.);
		p.drawRoundedRect(pill, radius, radius);
		p.setPen(st::msgServiceFg);
		p.drawText(
			pill.marginsRemoved(pad),
			Qt::AlignCenter | Qt::TextWordWrap,
			lines);
		return;
	}
	for (auto i = 0; i != int(_items.size()); ++i) {
		const auto &item = _items[i];
		if (item.date && item.date->intersects(clip)) {
			paintDate(p, item);
		}
		if (item.bubble.marginsAdded({ 16, 0, 0, 4 }).intersects(clip)) {
			paintItem(p, i);
		}
	}
}

int LogList::itemAt(QPoint point) const {
	for (auto i = 0; i != int(_items.size()); ++i) {
		if (_items[i].bubble.contains(point)) {
			return i;
		}
	}
	return -1;
}

void LogList::mouseMoveEvent(QMouseEvent *e) {
	const auto over = itemAt(e->pos());
	if (_over == over) {
		return;
	}
	const auto was = _over;
	_over = over;
	const auto jumpable = (over >= 0) && entryAt(over).jumpTo;
	setCursor(jumpable ? style::cur_pointer : style::cur_default);
	if (was >= 0) {
		update(_items[was].bubble.marginsAdded({ 16, 0, 0, 4 }));
	}
	if (over >= 0) {
		update(_items[over].bubble.marginsAdded({ 16, 0, 0, 4 }));
	}
}

void LogList::leaveEventHook(QEvent *e) {
	if (_over >= 0) {
		const auto was = std::exchange(_over, -1);
		update(_items[was].bubble.marginsAdded({ 16, 0, 0, 4 }));
	}
	setCursor(style::cur_default);
}

void LogList::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		return;
	}
	const auto index = itemAt(e->pos());
	if (index < 0 || !entryAt(index).jumpTo) {
		return;
	}
	_controller->showPeerHistory(
		_data->history,
		Window::SectionShow::Way::Backward,
		entryAt(index).jumpTo);
}

void LogList::contextMenuEvent(QContextMenuEvent *e) {
	const auto index = itemAt(e->pos());
	if (index < 0) {
		return;
	}
	const auto text = entryAt(index).text.text;
	_menu = base::make_unique_q<Ui::PopupMenu>(this, st::popupMenuWithIcons);
	_menu->addAction(tr::lng_context_copy_text(tr::now), [=] {
		QGuiApplication::clipboard()->setText(text);
	}, &st::menuIconCopy);
	if (const auto id = entryAt(index).jumpTo) {
		_menu->addAction(tr::lng_moligram_deleted_jump(tr::now), [=] {
			_controller->showPeerHistory(
				_data->history,
				Window::SectionShow::Way::Backward,
				id);
		}, &st::menuIconShowInChat);
	}
	_menu->popup(e->globalPos());
}

class LogTopBar final : public Ui::RpWidget {
public:
	LogTopBar(
		QWidget *parent,
		std::shared_ptr<LogData> data,
		Fn<void()> back);

protected:
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	const std::shared_ptr<LogData> _data;
	object_ptr<Ui::IconButton> _back;

};

LogTopBar::LogTopBar(
	QWidget *parent,
	std::shared_ptr<LogData> data,
	Fn<void()> back)
: RpWidget(parent)
, _data(std::move(data))
, _back(this, st::historyTopBarBack) {
	_back->setClickedCallback(std::move(back));
	_back->moveToLeft(0, 0);
	resize(width(), st::topBarHeight);
}

int LogTopBar::resizeGetHeight(int newWidth) {
	return st::topBarHeight;
}

void LogTopBar::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	p.fillRect(e->rect(), st::topBarBg);
	const auto left = _back->width();
	const auto available = width() - left - st::topBarArrowPadding.right();
	const auto &title = st::semiboldFont;
	const auto &sub = st::normalFont;
	const auto block = title->height + sub->height;
	const auto top = (height() - block) / 2;
	p.setFont(title);
	p.setPen(st::dialogsNameFg);
	p.drawText(
		left,
		top + title->ascent,
		title->elided(_data->title, available));
	p.setFont(sub);
	p.setPen(st::historyStatusFg);
	p.drawText(
		left,
		top + title->height + sub->ascent,
		sub->elided(_data->subtitle, available));
}

class LogWidget final : public Window::SectionWidget {
public:
	LogWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		std::shared_ptr<LogData> data);

	bool hasTopBarShadow() const override {
		return true;
	}
	QPixmap grabForShowAnimation(
		const Window::SectionSlideParams &params) override;
	bool showInternal(
		not_null<Window::SectionMemento*> memento,
		const Window::SectionShow &params) override;
	std::shared_ptr<Window::SectionMemento> createMemento() override;

	void setInternalState(const QRect &geometry);

	bool floatPlayerHandleWheelEvent(QEvent *e) override {
		return _scroll->viewportEvent(e);
	}
	QRect floatPlayerAvailableRect() override {
		return mapToGlobal(_scroll->geometry());
	}

protected:
	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void doSetInnerFocus() override {
		setFocus();
	}

private:
	void updateControlsGeometry();

	const std::shared_ptr<LogData> _data;
	std::shared_ptr<Ui::ChatTheme> _theme;
	object_ptr<LogTopBar> _topBar;
	object_ptr<Ui::PlainShadow> _topBarShadow;
	object_ptr<Ui::RpWidget> _searchBar = { nullptr };
	Ui::InputField *_search = nullptr;
	object_ptr<Ui::ScrollArea> _scroll;
	QPointer<LogList> _list;
	bool _scrolledToBottom = false;

};

class LogMemento final : public Window::SectionMemento {
public:
	explicit LogMemento(std::shared_ptr<LogData> data)
	: _data(std::move(data)) {
	}

	object_ptr<Window::SectionWidget> createWidget(
			QWidget *parent,
			not_null<Window::SessionController*> controller,
			Window::Column column,
			const QRect &geometry) override {
		if (column == Window::Column::Third) {
			return nullptr;
		}
		auto result = object_ptr<LogWidget>(parent, controller, _data);
		result->setInternalState(geometry);
		return result;
	}

	[[nodiscard]] const std::shared_ptr<LogData> &data() const {
		return _data;
	}

private:
	const std::shared_ptr<LogData> _data;

};

LogWidget::LogWidget(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	std::shared_ptr<LogData> data)
: SectionWidget(parent, controller, data->history->peer)
, _data(std::move(data))
, _topBar(this, _data, [=] { controller->showBackFromStack(); })
, _topBarShadow(this)
, _scroll(this, st::historyScroll) {
	_list = _scroll->setOwnedWidget(
		object_ptr<LogList>(this, controller, _data));
	Window::ChatThemeValueFromPeer(
		controller,
		_data->history->peer
	) | rpl::on_next([=](std::shared_ptr<Ui::ChatTheme> &&theme) {
		_theme = std::move(theme);
		controller->setChatStyleTheme(_theme);
		update();
	}, lifetime());
	if (_data->searchable && !_data->entries.empty()) {
		_searchBar.create(this);
		_search = Ui::CreateChild<Ui::InputField>(
			_searchBar.data(),
			st::dialogsFilter,
			tr::lng_dlg_filter());
		_searchBar->paintRequest() | rpl::on_next([=](QRect clip) {
			QPainter(_searchBar.data()).fillRect(clip, st::topBarBg);
		}, _searchBar->lifetime());
		_search->changes() | rpl::on_next([=] {
			if (_list) {
				_list->setQuery(_search->getLastText());
				updateControlsGeometry();
				_scroll->scrollToY(_list->height());
			}
		}, _search->lifetime());
	}
	_topBarShadow->raise();
}

void LogWidget::setInternalState(const QRect &geometry) {
	setGeometry(geometry);
	Ui::SendPendingMoveResizeEvents(this);
}

QPixmap LogWidget::grabForShowAnimation(
		const Window::SectionSlideParams &params) {
	if (params.withTopBarShadow) {
		_topBarShadow->hide();
	}
	auto result = Ui::GrabWidget(this);
	if (params.withTopBarShadow) {
		_topBarShadow->show();
	}
	return result;
}

bool LogWidget::showInternal(
		not_null<Window::SectionMemento*> memento,
		const Window::SectionShow &params) {
	return false;
}

std::shared_ptr<Window::SectionMemento> LogWidget::createMemento() {
	return std::make_shared<LogMemento>(_data);
}

void LogWidget::resizeEvent(QResizeEvent *e) {
	if (width() && height()) {
		updateControlsGeometry();
	}
}

void LogWidget::updateControlsGeometry() {
	_topBar->resizeToWidth(width());
	_topBar->moveToLeft(0, 0);
	auto top = _topBar->height();
	if (_searchBar) {
		const auto padding = st::dialogsFilterPadding;
		const auto fieldHeight = st::dialogsFilter.heightMin;
		const auto barHeight = fieldHeight + padding.y() * 2;
		_searchBar->setGeometry(0, top, width(), barHeight);
		_search->setGeometry(
			padding.x(),
			padding.y(),
			width() - padding.x() * 2,
			fieldHeight);
		top += barHeight;
	}
	_topBarShadow->setGeometry(0, top, width(), st::lineWidth);
	_scroll->setGeometry(0, top, width(), height() - top);
	if (_list) {
		_list->resizeToWidth(width());
		if (_list->height() < _scroll->height()) {
			_list->resize(width(), _scroll->height());
		}
		if (!_scrolledToBottom) {
			_scrolledToBottom = true;
			_scroll->scrollToY(_list->height());
		}
	}
}

void LogWidget::paintEvent(QPaintEvent *e) {
	if (animatingShow()) {
		SectionWidget::paintEvent(e);
		return;
	} else if (controller()->contentOverlapped(this, e)) {
		return;
	}
	if (!_theme) {
		auto p = QPainter(this);
		p.fillRect(e->rect(), st::windowBg);
		return;
	}
	const auto above = _topBar->height()
		+ (_searchBar ? _searchBar->height() : 0);
	const auto clip = e->rect().intersected(
		QRect(0, above, width(), height() - above));
	SectionWidget::PaintBackground(controller(), _theme.get(), this, clip);
}

void ShowLog(
		not_null<Window::SessionController*> controller,
		std::shared_ptr<LogData> data) {
	controller->showSection(std::make_shared<LogMemento>(std::move(data)));
}

} // namespace

void ShowDeletedLog(
		not_null<Window::SessionController*> controller,
		not_null<History*> history) {
	auto data = std::make_shared<LogData>(LogData{
		.history = history,
		.title = tr::lng_moligram_deleted_title(tr::now),
		.subtitle = history->peer->name(),
		.empty = tr::lng_moligram_deleted_empty(tr::now),
		.searchable = true,
	});
	const auto color = HighlightColor(Highlight::Deleted);
	for (const auto &record : DeletedIn(history)) {
		const auto when = QLocale().toString(
			base::unixtime::parse(record.deleted).time(),
			QLocale::ShortFormat);
		data->entries.push_back({
			.author = record.author,
			.text = { record.text },
			.date = record.date,
			.caption = tr::lng_moligram_log_deleted_at(tr::now, lt_time, when),
			.accent = color,
			.jumpTo = LiveDeleted(history, record.id) ? record.id : MsgId(),
		});
	}
	ranges::stable_sort(data->entries, ranges::less(), &Entry::date);
	if (!data->entries.empty()) {
		data->subtitle = tr::lng_moligram_log_count(
			tr::now,
			lt_count,
			data->entries.size(),
			lt_chat,
			history->peer->name());
	}
	ShowLog(controller, std::move(data));
}

void ShowEditLog(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	const auto history = item->history();
	auto data = std::make_shared<LogData>(LogData{
		.history = history,
		.title = tr::lng_moligram_edited_history(tr::now),
		.subtitle = history->peer->name(),
		.empty = QString(),
	});
	const auto author = item->from()->name();
	const auto revisions = Revisions(item);
	const auto color = HighlightColor(Highlight::Edited);
	for (auto i = 0; i != int(revisions.size()); ++i) {
		data->entries.push_back({
			.author = author,
			.text = revisions[i].text,
			.date = revisions[i].date,
			.caption = i
				? tr::lng_moligram_log_edit(tr::now, lt_number, QString::number(i))
				: tr::lng_moligram_log_original(tr::now),
			.accent = color,
		});
	}
	const auto edited = item->Get<HistoryMessageEdited>();
	data->entries.push_back({
		.author = author,
		.text = item->originalText(),
		.date = (edited && edited->date) ? edited->date : item->date(),
		.caption = tr::lng_moligram_edited_now(tr::now),
		.jumpTo = item->id,
	});
	data->subtitle = tr::lng_moligram_log_versions(
		tr::now,
		lt_count,
		data->entries.size(),
		lt_chat,
		history->peer->name());
	ShowLog(controller, std::move(data));
}

} // namespace Moligram
