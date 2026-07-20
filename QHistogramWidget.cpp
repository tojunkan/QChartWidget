#include "QHistogramWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QtMath>
#include <cmath>
#include <algorithm>

// 正态分布 PDF
static qreal normalPdf(qreal x, qreal mu, qreal sigma) {
    if (sigma <= 0) return 0;
    qreal z = (x - mu) / sigma;
    return std::exp(-0.5 * z * z) / (sigma * std::sqrt(2.0 * M_PI));
}

QHistogramWidget::QHistogramWidget(QWidget* parent) : QBarWidget(parent)
{
    setBarWidth(0.95);
    setBarLabelsVisible(false);
    setBarLabelsAngle(-60); // 标签斜着写
    // X 轴换成 QValueAxis
    auto* xAxis = new QValueAxis(this);
    xAxis->setLabelDecimals(1);
    setCategoryAxis(xAxis);
}

void QHistogramWidget::setRawData(const QVector<qreal>& data)
{
    m_rawData = data;
    std::sort(m_rawData.begin(), m_rawData.end());
    qDebug() << "[QHistogramWidget] rawData:" << m_rawData.size() << "points"
             << "range:[" << (m_rawData.isEmpty() ? 0 : m_rawData.first())
             << "," << (m_rawData.isEmpty() ? 0 : m_rawData.last()) << "]";
    computeBins();
}

void QHistogramWidget::setBinCount(int n) { m_binCount = qMax(0, n); }
void QHistogramWidget::setBinWidth(qreal w) { m_binWidth = qMax(0.0, w); }
void QHistogramWidget::setBinMethod(BinMethod m) { m_binMethod = m; }

qreal QHistogramWidget::binWidth() const
{
    if (m_binWidth > 0) return m_binWidth;
    QVector<qreal> edges = computeBinEdges();
    return edges.size() > 1 ? edges[1] - edges[0] : 1;
}

QVector<qreal> QHistogramWidget::computeBinEdges() const
{
    if (m_rawData.isEmpty()) return {};
    qreal dataMin = m_rawData.first();
    qreal dataMax = m_rawData.last();
    qreal range = dataMax - dataMin;

    int nBins = m_binCount;
    if (nBins <= 0) {
        int n = m_rawData.size();
        switch (m_binMethod) {
        case Sturges:
            nBins = qMax(1, (int)std::ceil(std::log2(n) + 1));
            break;
        case FreedmanDiaconis:
        default: {
            // IQR-based
            int q1Idx = n / 4;
            int q3Idx = 3 * n / 4;
            qreal iqr = m_rawData[q3Idx] - m_rawData[q1Idx];
            qreal bw = (iqr > 0) ? 2.0 * iqr / std::cbrt(n) : range / 10;
            nBins = qMax(1, (int)std::ceil(range / bw));
            break;
        }
        case Scott: {
            qreal mean = 0;
            for (qreal v : m_rawData) mean += v;
            mean /= n;
            qreal variance = 0;
            for (qreal v : m_rawData) variance += (v - mean) * (v - mean);
            variance /= (n - 1);
            qreal sd = std::sqrt(variance);
            qreal bw = (sd > 0) ? 3.49 * sd / std::cbrt(n) : range / 10;
            nBins = qMax(1, (int)std::ceil(range / bw));
            break;
        }
        }
    }

    qreal binW = (m_binWidth > 0) ? m_binWidth : range / nBins;
    // 边界扩展一点确保全覆盖
    qreal start = dataMin - binW * 0.001;
    int actualBins = qMax(1, (int)std::ceil((dataMax - start) / binW));

    QVector<qreal> edges;
    edges.reserve(actualBins + 1);
    for (int i = 0; i <= actualBins; ++i)
        edges.append(start + i * binW);

    qDebug() << "[QHistogramWidget] bins:" << actualBins
             << "method=" << m_binMethod
             << "binW=" << binW
             << "edges:" << edges;
    return edges;
}

QVector<qreal> QHistogramWidget::computeBinCenters() const
{
    QVector<qreal> edges = computeBinEdges();
    QVector<qreal> centers;
    centers.reserve(edges.size() - 1);
    for (int i = 0; i < edges.size() - 1; ++i)
        centers.append((edges[i] + edges[i+1]) / 2);
    return centers;
}

QVector<qreal> QHistogramWidget::computeDensity() const
{
    QVector<qreal> edges = computeBinEdges();
    QVector<qreal> freq(edges.size() - 1, 0);
    if (m_rawData.isEmpty()) return freq;

    for (qreal v : m_rawData) {
        for (int i = 0; i < edges.size() - 1; ++i) {
            if (v >= edges[i] && v < edges[i+1]) {
                freq[i]++;
                break;
            }
        }
    }
    // 归一化为密度 = 频数 / (总数 * 箱宽)
    qreal total = m_rawData.size();
    qreal bw = binWidth();
    for (qreal& f : freq) f /= (total * bw);
    return freq;
}

void QHistogramWidget::computeBins()
{
    if (m_rawData.isEmpty()) return;

    QVector<qreal> edges = computeBinEdges();
    int nBins = edges.size() - 1;
    if (nBins <= 0) return;

    // 计算频数
    QVector<qreal> freq(nBins, 0);
    for (qreal v : m_rawData) {
        for (int i = 0; i < nBins; ++i) {
            if (v >= edges[i] && v < edges[i+1]) {
                freq[i]++;
                break;
            }
        }
    }

    // 构建标签（bin 中心值）和 BarSet
    QVector<qreal> centers = computeBinCenters();
    QStringList labels;
    for (int i = 0; i < nBins; ++i) {
        labels.append(QString::number(centers[i], 'f', 1));
    }

    clear(); // 清空旧 BarSet 和分类
    setCategories(labels);
    setCategoryValues(centers);  // 值定位

    auto* set = new QBarSet("频数", this);
    QList<qreal> vals;
    for (qreal f : freq) vals.append(f);
    set->setValues(QVector<qreal>(vals.begin(), vals.end()));
    set->setColor(QColor("#42A5F5"));
    addBarSet(set);

    // Y 轴范围
    qreal maxFreq = *std::max_element(freq.begin(), freq.end());
    valueAxis()->setMin(0);
    valueAxis()->setMax(maxFreq * 1.1);

    // X 轴范围
    qreal dataMin = edges.first(), dataMax = edges.last();
    auto* xAxis = qobject_cast<QValueAxis*>(categoryAxis());
    if (xAxis) {
        xAxis->setMin(dataMin);
        xAxis->setMax(dataMax);
    }

    qDebug() << "[QHistogramWidget] computeBins: bins=" << nBins
             << "maxFreq=" << maxFreq << "xRange=[" << dataMin << "," << dataMax << "]";
}

void QHistogramWidget::drawCurve(QPainter* p, const QRectF& area,
                                  const QVector<QPointF>& points,
                                  const QColor& color)
{
    if (points.size() < 2) return;
    p->save();
    QPen pen(color, 2);
    p->setPen(pen);
    p->setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.moveTo(points.first());
    for (int i = 1; i < points.size(); ++i)
        path.lineTo(points[i]);
    p->drawPath(path);
    p->restore();
}

void QHistogramWidget::paintOverlay(QPainter* p, const QRectF& area)
{
    if (!m_densityVisible && !m_normalVisible) return;
    if (m_rawData.isEmpty()) return;

    qreal valLen = (orientation() == Vertical) ? area.height() : area.width();
    QVector<qreal> edges = computeBinEdges();
    QVector<qreal> centers = computeBinCenters();
    int nBins = edges.size() - 1;

    // 密度曲线
    if (m_densityVisible) {
        QVector<qreal> density = computeDensity();
        QVector<QPointF> pts;
        for (int i = 0; i < nBins; ++i) {
            qreal x = (orientation() == Vertical)
                ? area.left() + categoryAxis()->mapToPixel(i, area.width())
                : area.bottom() - categoryAxis()->mapToPixel(i, area.height());
            qreal y = area.bottom() - valueAxis()->mapToPixel(density[i], valLen);
            pts.append(QPointF(x, y));
        }
        drawCurve(p, area, pts, m_densityColor);
    }

    // 正态分布曲线
    if (m_normalVisible) {
        int n = m_rawData.size();
        qreal mu = 0, sigma = 0;
        for (qreal v : m_rawData) mu += v;
        mu /= n;
        for (qreal v : m_rawData) sigma += (v - mu) * (v - mu);
        sigma = (n > 1) ? std::sqrt(sigma / (n - 1)) : 1;

        // 在 bin 范围上采样画平滑曲线
        qreal xMin = edges.first(), xMax = edges.last();
        int samples = 100;
        QVector<QPointF> pts;
        for (int i = 0; i <= samples; ++i) {
            qreal x = xMin + (xMax - xMin) * i / samples;
            qreal pdf = normalPdf(x, mu, sigma);
            qreal px = (orientation() == Vertical)
                ? area.left() + (x - xMin) / (xMax - xMin) * area.width()
                : area.bottom() - (x - xMin) / (xMax - xMin) * area.height();
            qreal py = area.bottom() - valueAxis()->mapToPixel(pdf, valLen);
            pts.append(QPointF(px, py));
        }
        drawCurve(p, area, pts, m_normalColor);
    }
}
