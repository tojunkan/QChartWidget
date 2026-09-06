// test_widget_gl.h —— 批次 A widget GL 宿主冒烟（wayland/xcb 实窗；offscreen QSKIP）
#pragma once
#include <QObject>

class TestWidgetGl : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();          // offscreen → QSKIP
    void glWidgetRenders();       // plotArea 对齐 QOpenGLWidget 宿主 + FBO 取证
};
