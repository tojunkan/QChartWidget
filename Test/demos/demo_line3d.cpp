// demo_line3d.cpp —— 3D 参数螺旋线（动态：相机沿路径移动）
// 数据 = (r=1, θ, z)：θ∈[0,720°]，z=θ/720·4（上升 4 单位），经 QChartCylindricalProjection3D → 螺旋线。
// 动态：QPropertyAnimation 沿螺旋路径动画 camera3D 的 viewCubeCenter + yaw（R5 viewCube 模型）。
// §12：加盒/spine/刻度（Cylindrical3D 有反向 → 视图驱动路径，orbit 时盒随 dataBounds 更新）；'A' 键开关。
// 交互：左键 orbit / 滚轮 dolly（R6 无平移手势）。
#include "demos.h"
#include "../../QChartWidget3D.h"
#include "../../QChartLayer3D.h"
#include "../../QChartLineSeries3D.h"
#include "../../QChartCylindricalProjection3D.h"
#include "../../QValueAxis.h"
#include <QPropertyAnimation>
#include <QKeyEvent>
#include <QtMath>
#include <QDebug>

namespace {
    // 螺旋线上角度 θ（度）处的点：半径 R、总高 H、总角 T
    QVector3D helixPoint(qreal R, qreal H, qreal T, qreal thetaDeg) {
        const qreal rad = qDegreesToRadians(thetaDeg);
        return QVector3D(R * qCos(rad), R * qSin(rad), H * thetaDeg / T);
    }

    // 'A' 键：轴/网格开关（无 Q_OBJECT）
    class LineKeyFilter : public QObject {
    public:
        QChartWidget3D* w = nullptr;
        QChartLayer3D* layer = nullptr;
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (ev->type() == QEvent::KeyPress) {
                auto* ke = static_cast<QKeyEvent*>(ev);
                if (ke->key() == Qt::Key_A) {
                    if (layer) {
                        layer->axes3D()->setVisible(!layer->axes3D()->visible());
                        qDebug() << "轴/网格开关:" << (layer->axes3D()->visible() ? "开" : "关");
                        w->invalidateForeground();
                        w->update();
                    }
                    return true;
                }
            }
            return QObject::eventFilter(obj, ev);
        }
    };
}

QWidget* buildDemoLine3D() {
    qDebug() << "\n========== 3D 参数螺旋线（相机沿路径）==========";

    auto* w = new QChartWidget3D();
    w->setWindowTitle("3D 螺旋线 - 相机沿路径飞行");
    w->resize(560, 520);
    w->setFocusPolicy(Qt::StrongFocus);
    w->setObjectName("widget3d");

    auto* filter = new LineKeyFilter();
    filter->setParent(w);
    filter->w = w;
    w->installEventFilter(filter);

    const qreal R = 1.5;      // 螺旋半径
    const qreal H = 4.0;      // 总高
    const qreal T = 720.0;    // 总角

    // 数据：θ∈[0,720°]，z 线性上升
    auto* layer = new QChartLayer3D(w);
    // §12：轴绑定（tickCount 3；Cylindrical3D 有反向 → 视图驱动：轴盒随 viewCube 反算更新）
    auto* axisR = new QValueAxis(w, Qt::AlignBottom);
    auto* axisTh = new QValueAxis(w, Qt::AlignLeft);
    auto* axisZ = new QValueAxis(w, Qt::AlignBottom);
    axisR->setTickCount(3);
    axisTh->setTickCount(3);
    axisZ->setTickCount(3);
    axisR->setColor(QColor("#E53935"));
    axisTh->setColor(QColor("#43A047"));
    axisZ->setColor(QColor("#26C6DA"));
    layer->setAxisX(axisR);
    layer->setAxisY(axisTh);
    layer->setAxisZ(axisZ);
    filter->layer = layer;

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

    // 相机动画（R5 viewCube 主状态：position/lookAt 派生只读）：
    // 驱动 viewCubeCenter 沿螺旋移动（lookAt=盒中心跟随）+ yaw 环绕旋转 → 相机沿路径飞行
    QChartCamera3D* cam = w->camera3D();
    QPropertyAnimation::KeyValues centerKeys;
    const qreal keyAngles[] = { 0.0, 240.0, 480.0, 720.0 };
    const qreal keyFracs[] = { 0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0 };
    for (int i = 0; i < 4; ++i)
        centerKeys.append({ keyFracs[i],
                            QVariant::fromValue(helixPoint(1.0, H, T, keyAngles[i])) });   // 数据螺旋半径 1

    auto* animCenter = new QPropertyAnimation(cam, "viewCubeCenter", w);
    animCenter->setKeyValues(centerKeys);
    animCenter->setDuration(12000);
    animCenter->setLoopCount(-1);
    animCenter->start();

    auto* animYaw = new QPropertyAnimation(cam, "yaw", w);
    animYaw->setStartValue(0.0);
    animYaw->setEndValue(360.0);
    animYaw->setDuration(12000);
    animYaw->setLoopCount(-1);
    animYaw->start();

    qDebug() << "相机动画已启动（viewCubeCenter 沿螺旋 + yaw 环绕，12s 循环；R5 viewCube 模型）";
    return w;
}
