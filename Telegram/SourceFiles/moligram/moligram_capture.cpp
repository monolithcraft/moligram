/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#include "moligram/moligram_capture.h"

#include "moligram/moligram_settings.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QGuiApplication>
#include <QtNetwork/QLocalSocket>

namespace Moligram {
namespace {

constexpr auto kSocketTimeout = 300;

[[nodiscard]] QString HyprlandSocket() {
	const auto signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
	const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
	if (signature.isEmpty() || runtime.isEmpty()) {
		return QString();
	}
	return runtime + u"/hypr/"_q + signature + u"/.socket.sock"_q;
}

[[nodiscard]] QByteArray Ask(const QByteArray &request) {
	const auto path = HyprlandSocket();
	if (path.isEmpty()) {
		return QByteArray();
	}
	auto socket = QLocalSocket();
	socket.connectToServer(path);
	if (!socket.waitForConnected(kSocketTimeout)) {
		return QByteArray();
	}
	socket.write(request);
	socket.flush();
	auto result = QByteArray();
	while (socket.waitForReadyRead(kSocketTimeout)) {
		result += socket.readAll();
	}
	return result;
}

[[nodiscard]] std::vector<QString> OwnWindows() {
	const auto pid = QCoreApplication::applicationPid();
	const auto clients = QJsonDocument::fromJson(Ask("j/clients")).array();
	auto result = std::vector<QString>();
	for (const auto &client : clients) {
		const auto object = client.toObject();
		if (object.value(u"pid"_q).toInteger() == pid) {
			result.push_back(object.value(u"address"_q).toString());
		}
	}
	return result;
}

bool Hidden = false;

void Apply() {
	const auto value = Hidden ? "\"1\"" : "\"0\"";
	for (const auto &address : OwnWindows()) {
		Ask("dispatch hl.dsp.window.set_prop({ prop = \"no_screen_share\", "
			"value = " + QByteArray(value) + ", window = \"address:"
			+ address.toLatin1() + "\" })");
	}
}

} // namespace

bool CaptureHidingSupported() {
	return !HyprlandSocket().isEmpty();
}

void SetCaptureHidden(bool hidden) {
	Hidden = hidden || Enabled(Option::StreamerMode);
	if (CaptureHidingSupported()) {
		Apply();
	}
}

void StartCaptureGuard() {
	if (!CaptureHidingSupported()) {
		return;
	}
	static auto lifetime = rpl::lifetime();
	Value(Option::StreamerMode) | rpl::on_next([](bool enabled) {
		SetCaptureHidden(enabled);
	}, lifetime);
	QObject::connect(
		qApp,
		&QGuiApplication::focusWindowChanged,
		qApp,
		[] {
			if (Hidden) {
				Apply();
			}
		});
}

} // namespace Moligram
