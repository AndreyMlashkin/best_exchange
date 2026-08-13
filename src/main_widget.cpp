#include "main_widget.h"

#include <QSize>
#include <QString>

MainWidget::MainWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Best Exchange"));
    resize(QSize{900, 600});
}

