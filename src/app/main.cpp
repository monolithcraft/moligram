#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>

#include <cstdlib>

Q_IMPORT_QML_PLUGIN(Moligram_DesignPlugin)
Q_IMPORT_QML_PLUGIN(Moligram_ControlsPlugin)
Q_IMPORT_QML_PLUGIN(Moligram_ScreensPlugin)

using namespace Qt::StringLiterals;

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(u"Moligram"_s);
    QGuiApplication::setApplicationVersion(QStringLiteral(MOLIGRAM_VERSION));
    QGuiApplication::setOrganizationName(u"Monolith"_s);
    QGuiApplication::setDesktopFileName(u"io.github.monolithcraft.Moligram"_s);
    QGuiApplication::setWindowIcon(QIcon(u":/moligram/branding/logo.png"_s));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("Moligram.Screens", "MainWindow");

    return QGuiApplication::exec();
}
