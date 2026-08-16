// test_qchartrenderer.cpp —— QChartRenderer 单元测试
// 用 QImage 作 QPaintDevice；在 QGuiApplication（offscreen）下可完整覆盖
// 文字渲染（QFontDatabase）与 QPixmap 缓存路径。
#include <QtTest>
#include <QImage>
#include <memory>
#include "../../QPainterChartRenderer.h"
#include "../../QValueAxis.h"
#include "../../QChartLayer.h"
#include "../../QChartProjectionFactory.h"
#include "../../QChartProjection.h"
#include "test_qchartrenderer.h"

namespace {
// 构建一个 Cartesian 场景：plotArea(20,20,360,260) + 两个边框轴；
// withLayer=false 时不加 layer（无网格，便于做透明像素断言）。
struct SceneFixture {
    std::unique_ptr<QChartProjection> proj;
    QValueAxis* xAxis;
    QValueAxis* yAxis;
    QChartLayer* layer = nullptr;
    QChartScene scene;

    explicit SceneFixture(const QRectF& dataBounds = QRectF(0, 0, 10, 10),
                          bool withLayer = true) {
        proj = QChartProjectionFactory::create(CoordinateSystem::Cartesian);
        xAxis = new QValueAxis(nullptr, Qt::AlignBottom);
        yAxis = new QValueAxis(nullptr, Qt::AlignLeft);

        scene.plotArea   = QRectF(20, 20, 360, 260);
        scene.dataBounds = dataBounds;
        scene.viewRect   = dataBounds;   // Cartesian: viewRect ≡ dataBounds
        scene.projection = proj.get();
        scene.axes       = QList<QChartAxis*>{ xAxis, yAxis };

        if (withLayer) {
            layer = new QChartLayer();
            layer->setAxisX(xAxis);
            layer->setAxisY(yAxis);
            scene.layers = QList<QChartLayer*>{ layer };
        }
    }
    ~SceneFixture() {
        delete xAxis;
        delete yAxis;
        delete layer;
    }
};

int countOpaque(const QImage& img) {
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(img.pixel(x, y)) > 0) ++n;
    return n;
}

QImage render(SceneFixture& fx, bool caching, bool invalidate) {
    QPainterChartRenderer renderer;
    renderer.setCachingEnabled(caching);
    if (invalidate) {
        renderer.invalidateBackground();
        renderer.invalidateForeground();
    }
    QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    renderer.render(fx.scene, &img);
    return img;
}
} // namespace

// ===== 渲染到 QImage（缓存开启）非空 =====
void TestQChartRenderer::renderToImage_nonEmpty() {
    SceneFixture fx;
    QImage img = render(fx, /*caching=*/true, /*invalidate=*/false);
    QVERIFY2(countOpaque(img) > 100, "轴/刻度/标签/网格应产生可观的非透明像素");
}

// ===== 轴应画在 plotArea 边缘（像素采样验证）=====
void TestQChartRenderer::renderToImage_axesAtPlotAreaEdges() {
    SceneFixture fx(QRectF(0, 0, 10, 10), /*withLayer=*/false);  // 无网格
    QImage img = render(fx, /*caching=*/true, /*invalidate=*/false);

    // plotArea = (20,20,360,260) → left=20, top=20, right=380, bottom=280
    // 底部轴线 y=280、左侧轴线 x=20（黑色，alpha>0）
    QVERIFY2(qAlpha(img.pixel(200, 280)) > 0, "底部轴线应命中 plotArea.bottom()");
    QVERIFY2(qAlpha(img.pixel(20, 150)) > 0, "左侧轴线应命中 plotArea.left()");
    // 无 layer → 无网格；plotArea 内部远离轴线的点应透明
    QVERIFY2(qAlpha(img.pixel(200, 150)) == 0, "无网格时 plotArea 内部应透明");
}

// ===== 缓存开启与关闭（直接绘制）结果像素级一致 =====
void TestQChartRenderer::cachingMatchesDirectDrawing() {
    SceneFixture fx;
    QImage direct  = render(fx, /*caching=*/false, /*invalidate=*/false);
    QImage cached  = render(fx, /*caching=*/true,  /*invalidate=*/false);
    QVERIFY(countOpaque(direct) > 0);
    QCOMPARE(countOpaque(cached), countOpaque(direct));
    QVERIFY2(cached == direct, "缓存路径应产出与直接绘制一致的像素");
}

// ===== 缓存脏标记：不 invalidate 复用旧缓存，invalidate 后重建 =====
void TestQChartRenderer::caching_invalidateTriggersRebuild() {
    // 两个场景 dataBounds 不同 → 刻度位置/标签不同 → 像素应不同
    SceneFixture sceneA(QRectF(0, 0, 10, 10));
    SceneFixture sceneB(QRectF(0, 0, 20, 10));

    QPainterChartRenderer renderer;   // 缓存开启（默认）
    auto renderTo = [&](SceneFixture& fx) {
        QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        renderer.render(fx.scene, &img);
        return img;
    };

    QImage a = renderTo(sceneA);       // 建缓存（场景 A）
    QImage b = renderTo(sceneB);       // 未 invalidate → 应复用场景 A 的旧缓存
    QVERIFY2(b == a, "未 invalidate 时应复用旧缓存（陈旧）");

    renderer.invalidateBackground();
    renderer.invalidateForeground();
    QImage b2 = renderTo(sceneB);      // invalidate 后 → 重建为场景 B
    QVERIFY2(b2 != a, "invalidate 后应重建缓存，内容更新为场景 B");
}

// ===== 背景填充：valid 填整设备、invalid 透明 =====
void TestQChartRenderer::backgroundFill_validAndInvalid() {
    // valid → 角落像素 == backgroundColor（整 device 填充）
    SceneFixture fxValid(QRectF(0, 0, 10, 10), /*withLayer=*/false);
    fxValid.scene.backgroundColor = QColor("#FF0000");
    QImage imgValid = render(fxValid, /*caching=*/true, /*invalidate=*/false);
    QCOMPARE(imgValid.pixel(0, 0), QColor("#FF0000").rgba());
    QCOMPARE(imgValid.pixel(399, 299), QColor("#FF0000").rgba());

    // invalid → 不填充，角落透明
    SceneFixture fxInvalid(QRectF(0, 0, 10, 10), /*withLayer=*/false);
    QImage imgInvalid = render(fxInvalid, /*caching=*/true, /*invalidate=*/false);
    QCOMPARE(qAlpha(imgInvalid.pixel(0, 0)), 0);
}
