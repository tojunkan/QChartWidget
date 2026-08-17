// demo_line3d.cpp —— 3D 参数螺旋线（动态：相机沿路径移动）
// 数据 = (r=1, θ, z)：θ∈[0,720°]，z=θ/720·4（上升 4 单位），经 QChartCylindricalProjection3D → 螺旋线。
// 动态：QPropertyAnimation 沿螺旋路径动画 camera3D 的 position/lookAt（QVector3D 插值器已注册）。
// 交互：左键 orbit / 滚轮 dolly / 右键 pan。
#include "demos.h"
#include "../../QChartWidget3D.h"
#include "../../QChartLayer3D.h"
#include "../../QChartLineSeries3D.h"
#include "../../QChartCylindricalProjection3D.h"
#include <QPropertyAnimation>
#include <QtMath>
#include <QDebug>

namespace {
    // 螺旋线上角度 θ（度）处的点：半径 R、总高 H、总角 T
    QVector3D helixPoint(qreal R, qreal H, qreal T, qreal thetaDeg) {
        const qreal rad = qDegreesToRadians(thetaDeg);
        return QVector3D(R * qCos(rad), R * qSin(rad), H * thetaDeg / T);
    }
}

QWidget* buildDemoLine3D() {
    qDebug() << "\n========== 3D 参数螺旋线（相机沿路径）==========";

    auto* w = new QChartWidget3D();
    w->setWindowTitle("3D 螺旋线 - 相机沿路径飞行");
    w->resize(560, 520);
    w->setObjectName("widget3d");

    const qreal R = 1.5;      // 螺旋半径
    const qreal H = 4.0;      // 总高
    const qreal T = 720.0;    // 总角

    // 数据：θ∈[0,720°]，z 线性上升
    auto* layer = new QChartLayer3D(w);
    auto* line = new QChartLineSeries3D("螺旋线", layer);
    line->setColor(QColor("#26C6DA"));
    line->setLineWidth(2.0);
    const int segments = 180;
    for (int i = 0; i <= segments; ++i) {
        const qreal th = T * i / segments;
        line->append(QDataPoint3D(1.0, th, H * th / T));
    }
    layer->addSeries3D(line);
    w->addLayer3D(layer);
    w->setProjection3D(std::make_unique<QChartCylindricalProjection3D>());   // 自动 fit

    // 相机动画：沿螺旋路径飞（lookAt 贴螺旋、position 外圈伴飞），循环
    QChartCamera3D* cam = w->camera3D();
    QPropertyAnimation::KeyValues lookAtKeys, posKeys;
    const qreal keyAngles[] = { 0.0, 240.0, 480.0, 720.0 };
    const qreal keyFracs[] = { 0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0 };
    for (int i = 0; i < 4; ++i) {
        const qreal th = keyAngles[i];
        const QVector3D look = helixPoint(R, H, T, th);
        const QVector3D pos  = look + QVector3D(2.6 * qCos(qDegreesToRadians(th)),
                                                2.6 * qSin(qDegreesToRadians(th)), 1.6);
        lookAtKeys.append({ keyFracs[i], QVariant::fromValue(look) });
        posKeys.append({ keyFracs[i], QVariant::fromValue(pos) });
    }

    auto* animLook = new QPropertyAnimation(cam, "lookAt", w);
    animLook->setKeyValues(lookAtKeys);
    animLook->setDuration(12000);
    animLook->setLoopCount(-1);
    animLook->start();

    auto* animPos = new QPropertyAnimation(cam, "position", w);
    animPos->setKeyValues(posKeys);
    animPos->setDuration(12000);
    animPos->setLoopCount(-1);
    animPos->start();

    qDebug() << "相机动画已启动（position/lookAt 沿螺旋，12s 循环）";
    return w;
}
