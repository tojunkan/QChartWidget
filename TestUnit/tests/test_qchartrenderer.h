// test_qchartrenderer.h —— QChartRenderer 单元测试声明
#pragma once
#include <QObject>

class TestQChartRenderer : public QObject {
    Q_OBJECT
private slots:
    void renderToImage_nonEmpty();                 // QImage + 缓存开启，渲染非空
    void renderToImage_axesAtPlotAreaEdges();      // 轴画在 plotArea 边缘（像素采样）
    void cachingMatchesDirectDrawing();            // 缓存结果与直接绘制一致
    void caching_invalidateTriggersRebuild();      // 缓存脏标记：invalidate 后重建
    void backgroundFill_validAndInvalid();         // backgroundColor valid 填充整设备、invalid 透明
};
