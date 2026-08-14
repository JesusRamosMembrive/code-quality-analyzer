#include "AppController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
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
