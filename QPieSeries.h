#ifndef QPIESERIES_H
#define QPIESERIES_H

#include "QAbstractSeries.h"
#include <QVector>
#include <QColor>
#include <QString>
#include <QVariantAnimation>
#include <QPointer>

struct PieSlice {
    QString label; qreal value; QColor color;
    bool exploded = false; qreal explodeTarget = 0, explodeOffset = 0, explodeFactor = 0.15;
    QPointer<QVariantAnimation> anim;
};

class QPieSeries : public QAbstractSeries
{
    Q_OBJECT
    Q_PROPERTY(qreal holeSize READ holeSize WRITE setHoleSize)
    Q_PROPERTY(qreal startAngle READ startAngle WRITE setStartAngle)

public:
    explicit QPieSeries(const QString& name = {}, QObject* parent = nullptr);

    void appendSlice(const QString& label, qreal value, const QColor& color = QColor());
    int sliceCount() const { return m_slices.size(); }
    const PieSlice& sliceAt(int i) const { return m_slices[i]; }

    qreal totalValue() const;
    void setHoleSize(qreal s) { m_holeSize = qBound(0.0, s, 0.99); }
    qreal holeSize() const { return m_holeSize; }
    void setStartAngle(qreal deg) { m_startAngle = deg; }
    qreal startAngle() const { return m_startAngle; }
    void setSliceExploded(int i, bool on);
    void setSliceExplodeFactor(int i, qreal f);

    // 点击命中
    int sliceAtAngle(qreal angleDeg, qreal radiusNorm) const;


private:
    QVector<PieSlice> m_slices;
    qreal m_holeSize = 0.35;
    qreal m_startAngle = 90;  // 12 点钟
};

#endif
