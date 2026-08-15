#include "AppController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStyleHints>

int main(int argc, char *argv[])
{
    // Without an explicit style, Qt Quick Controls picks a platform default (e.g.
    // "Windows" on this OS) whose built-in controls — ComboBox in particular — draw
    // with their own native-look colors instead of consistently following the
    // palette set on ApplicationWindow, leaving text unreadable against our dark
    // theme. "Basic" is the style every color override in this app was designed for.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication application(argc, argv);
    // The UI is a permanent dark theme; without this, Qt 6.8+ auto-follows the OS
    // color scheme and flips default text/control colors on Windows light mode,
    // making them unreadable against the app's dark backgrounds.
    application.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    AppController appController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &appController);

    engine.loadFromModule("CodeQA.Desktop", "Main");
    if (engine.rootObjects().isEmpty())
    {
        return 1;
    }

    return application.exec();
}
