#pragma once

#include <qwidget.h>
#include "Input.h"
#include <QMouseEvent>

struct MainWidget : public QWidget
{
    MainWidget(QWidget* parent = 0);

    void mousePressEvent(QMouseEvent* mouseEvent) override;
    void wheelEvent(QWheelEvent* mouseWheelEvent) override;
};
