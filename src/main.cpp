#include "main_widget.h"

#include <QApplication>
#include <QtPlugin>

#if defined(Q_OS_MACOS)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#endif

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    MainWidget mainWidget;
    mainWidget.show();

    return application.exec();
}
