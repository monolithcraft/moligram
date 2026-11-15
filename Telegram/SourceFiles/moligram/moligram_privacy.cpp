/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_privacy.h"

#include "core/application.h"
#include "main/main_domain.h"
#include "moligram/moligram_settings.h"
#include "storage/storage_domain.h"
#include "window/main_window.h"
#include "window/window_controller.h"

#include <QtCore/QUrlQuery>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>

namespace Moligram {
namespace {

[[nodiscard]] bool TrackingKey(const QString &key, const QString &host) {
	static const auto exact = base::flat_set<QString>{
		u"fbclid"_q, u"gclid"_q, u"dclid"_q, u"gbraid"_q, u"wbraid"_q,
		u"msclkid"_q, u"yclid"_q, u"ysclid"_q, u"igshid"_q, u"igsh"_q,
		u"mc_cid"_q, u"mc_eid"_q, u"_hsenc"_q, u"_hsmi"_q, u"vero_id"_q,
		u"rb_clickid"_q, u"_openstat"_q, u"spm"_q, u"ttclid"_q,
		u"twclid"_q, u"li_fat_id"_q, u"srsltid"_q, u"ref_src"_q,
		u"ref_url"_q, u"feature"_q,
	};
	const auto lower = key.toLower();
	if (lower.startsWith(u"utm_"_q) || lower.startsWith(u"oly_"_q)) {
		return true;
	} else if (lower == u"feature"_q) {
		return host.contains(u"youtu"_q);
	} else if (exact.contains(lower)) {
		return true;
	} else if (lower == u"si"_q) {
		return host.contains(u"youtu"_q) || host.contains(u"spotify"_q);
	} else if (lower == u"s"_q || lower == u"t"_q) {
		return (host == u"x.com"_q) || host.endsWith(u"twitter.com"_q);
	}
	return false;
}

[[nodiscard]] bool OwnWindow(QObject *object) {
	auto result = false;
	Core::App().enumerateWindows([&](not_null<Window::Controller*> w) {
		if (object == w->widget().get()) {
			result = true;
		}
	});
	return result;
}

[[nodiscard]] bool HasPasscode() {
	return Core::App().domain().local().hasLocalPasscode();
}

void Panic() {
	if (HasPasscode() && !Core::App().passcodeLocked()) {
		Core::App().lockByPasscode();
	}
	Core::App().enumerateWindows([&](not_null<Window::Controller*> w) {
		if (!w->widget()->minimizeToTray()) {
			w->widget()->showMinimized();
		}
	});
}

class Guard final : public QObject {
protected:
	bool eventFilter(QObject *object, QEvent *event) override {
		const auto type = event->type();
		if (type == QEvent::KeyPress) {
			const auto key = static_cast<QKeyEvent*>(event);
			const auto modifiers = key->modifiers()
				& (Qt::ControlModifier
					| Qt::ShiftModifier
					| Qt::AltModifier
					| Qt::MetaModifier);
			if (key->key() == Qt::Key_H
				&& modifiers == (Qt::ControlModifier | Qt::ShiftModifier)
				&& Enabled(Option::PanicKey)) {
				Panic();
				return true;
			}
		} else if ((type == QEvent::WindowStateChange
				|| type == QEvent::Hide)
			&& Enabled(Option::LockOnMinimize)
			&& OwnWindow(object)) {
			const auto widget = static_cast<QWidget*>(object);
			const auto away = (type == QEvent::Hide)
				|| (widget->windowState() & Qt::WindowMinimized);
			if (away && HasPasscode() && !Core::App().passcodeLocked()) {
				Core::App().lockByPasscode();
			}
		}
		return QObject::eventFilter(object, event);
	}

};

} // namespace

QString CleanUrl(const QString &url) {
	if (!Enabled(Option::CleanLinks)) {
		return url;
	}
	auto parsed = QUrl(url);
	const auto scheme = parsed.scheme().toLower();
	if ((scheme != u"http"_q && scheme != u"https"_q)
		|| !parsed.hasQuery()) {
		return url;
	}
	const auto host = parsed.host().toLower();
	auto query = QUrlQuery(parsed);
	auto changed = false;
	for (const auto &[key, value] : query.queryItems()) {
		if (TrackingKey(key, host)) {
			query.removeAllQueryItems(key);
			changed = true;
		}
	}
	if (!changed) {
		return url;
	}
	parsed.setQuery(query.isEmpty() ? QString() : query.query());
	if (query.isEmpty()) {
		parsed.setQuery(QString(), QUrl::DecodedMode);
		auto result = parsed.toString();
		if (result.endsWith('?')) {
			result.chop(1);
		}
		return result;
	}
	return parsed.toString();
}

void StartPrivacyGuard() {
	static auto guard = Guard();
	qApp->installEventFilter(&guard);
}

} // namespace Moligram
