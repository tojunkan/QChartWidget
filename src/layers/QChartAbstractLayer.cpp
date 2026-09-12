// QChartAbstractLayer.cpp —— 图层抽象基类实现（4a：维度无关公共部分）
#include "QChartAbstractLayer.h"

QChartAbstractLayer::QChartAbstractLayer(QObject* parent)
    : QObject(parent)
{
}

QChartAbstractLayer::~QChartAbstractLayer() = default;

// 默认不占边距（维度无关空实现；二维/三维层按自身轴绑定覆写）
void QChartAbstractLayer::borderAxisSizeHint(const QFont& font, qreal& left, qreal& top,
                                             qreal& right, qreal& bottom) const
{
    Q_UNUSED(font);
    Q_UNUSED(left);
    Q_UNUSED(top);
    Q_UNUSED(right);
    Q_UNUSED(bottom);
}
