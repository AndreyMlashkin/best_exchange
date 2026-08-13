#include "main_widget.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    MainWidget mainWidget;
    mainWidget.show();

    return application.exec();
}

