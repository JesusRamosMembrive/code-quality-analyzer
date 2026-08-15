#include "AppController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStyleHints>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    // The UI's colors (e.g. "#f4f6f8" panels) assume a light palette; without this,
    // Qt 6.8+ auto-follows the OS color scheme and flips default text/control colors
    // to light on Windows dark mode, making them unreadable against those backgrounds.
    application.styleHints()->setColorScheme(Qt::ColorScheme::Light);
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
