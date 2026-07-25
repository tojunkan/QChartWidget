#include "QHistogramSeries.h"
#include <QDebug>
#include <QtMath>
#include <algorithm>
#include <numeric>    // for std::accumulate
#include <cmath>

QHistogramSeries::QHistogramSeries(const QString& n, QObject* p)
    : QBarSeries(n, p)
{
    m_barWidth = 0.95;   // 与原始保持一致
}

void QHistogramSeries::setRawData(const QVector<qreal>& d)
{
    m_rawData = d;
    std::sort(m_rawData.begin(), m_rawData.end());
    computeBins();
}

// ----- 辅助：四分位距 -----
qreal QHistogramSeries::interquartileRange() const
{
    const int n = m_rawData.size();
    if (n < 4) return 0.0;   // 样本太少时无法可靠计算

    // 因为 m_rawData 已排序，直接取位置
    double q1 = m_rawData[static_cast<int>(n * 0.25)];
    double q3 = m_rawData[static_cast<int>(n * 0.75)];
    return q3 - q1;
}

// ----- 计算 bin 边界 -----
QVector<qreal> QHistogramSeries::binEdges() const
{
    if (m_rawData.isEmpty())
        return {};

    qreal dMin = m_rawData.first();
    qreal dMax = m_rawData.last();

    // 所有数据相等 → 构造一个简单范围
    if (qFuzzyCompare(dMin, dMax)) {
        return { dMin - 0.5, dMax + 0.5 };
    }

    const int n = m_rawData.size();
    qreal binWidth = 0.0;
    int binCount = 0;

    switch (m_binMethod) {
    case FreedmanDiaconis: {
        qreal iqr = interquartileRange();
        if (iqr > 0) {
            binWidth = 2.0 * iqr / std::pow(static_cast<qreal>(n), 1.0 / 3.0);
        }
        else {
            // IQR=0 → 回退到 Scott 规则（更稳健）
            qreal mean = std::accumulate(m_rawData.begin(), m_rawData.end(), 0.0) / n;
            qreal sqSum = 0.0;
            for (qreal v : m_rawData)
                sqSum += (v - mean) * (v - mean);
            qreal sigma = std::sqrt(sqSum / n);
            binWidth = 3.5 * sigma / std::pow(static_cast<qreal>(n), 1.0 / 3.0);
        }
        if (binWidth <= 0)   // 极端情况
            binWidth = (dMax - dMin) / std::max(1, n / 5);
        binCount = static_cast<int>(std::ceil((dMax - dMin) / binWidth));
        break;
    }
    case Scott: {
        qreal mean = std::accumulate(m_rawData.begin(), m_rawData.end(), 0.0) / n;
        qreal sqSum = 0.0;
        for (qreal v : m_rawData)
            sqSum += (v - mean) * (v - mean);
        qreal sigma = std::sqrt(sqSum / n);
        binWidth = 3.5 * sigma / std::pow(static_cast<qreal>(n), 1.0 / 3.0);
        if (binWidth <= 0)
            binWidth = (dMax - dMin) / std::max(1, n / 5);
        binCount = static_cast<int>(std::ceil((dMax - dMin) / binWidth));
        break;
    }
    case Fixed: {
        binWidth = (m_fixedBinWidth > 0) ? m_fixedBinWidth : (dMax - dMin) / 10.0;
        if (binWidth <= 0) binWidth = 1.0;
        binCount = static_cast<int>(std::ceil((dMax - dMin) / binWidth));
        break;
    }
    case Sturges:
    default: {
        binCount = std::max(1, static_cast<int>(std::ceil(std::log2(static_cast<qreal>(n)) + 1.0)));
        binWidth = (dMax - dMin) / binCount;
        break;
    }
    }

    // 安全防护
    if (binCount < 1) binCount = 1;
    if (binWidth <= 0) binWidth = (dMax - dMin) / std::max(1, n);

    // 生成边界
    QVector<qreal> edges;
    edges.reserve(binCount + 1);
    for (int i = 0; i <= binCount; ++i)
        edges.append(dMin + i * binWidth);

    // 确保最大值被包含（由于浮点误差）
    if (edges.last() < dMax)
        edges.append(dMax);

    return edges;
}

// ----- 计算 bin 中心（用于标签）-----
QVector<qreal> QHistogramSeries::binCenters() const
{
    auto edges = binEdges();
    QVector<qreal> centers;
    centers.reserve(edges.size() - 1);
    for (int i = 0; i < edges.size() - 1; ++i)
        centers.append((edges[i] + edges[i + 1]) / 2.0);
    return centers;
}

// ----- 核心：统计频数 -----
void QHistogramSeries::computeBins()
{
    if (m_rawData.isEmpty()) {
        m_freqs.clear();
        return;
    }

    auto edges = binEdges();
    int nBins = edges.size() - 1;
    if (nBins <= 0) return;

    m_freqs.resize(nBins);
    m_freqs.fill(0);

    // 将每个数据点分配到对应的 bin
    for (qreal v : m_rawData) {
        for (int i = 0; i < nBins; ++i) {
            if (v >= edges[i] && v < edges[i + 1]) {
                m_freqs[i]++;
                break;
            }
        }
    }

    // 可选：记录最大频率用于调试
    qreal maxF = 0.0;
    for (qreal f : m_freqs)
        if (f > maxF) maxF = f;

    // 更新条形图的分类标签（使用 bin 中心）
    auto centers = binCenters();
    QStringList cats;
    for (qreal c : centers)
        cats.append(QString::number(c, 'f', 1));   // 保留一位小数
    setCategories(cats);

    // 清理旧的 QBarSet 并新建一个表示频率
    qDeleteAll(m_barSets);
    m_barSets.clear();

    auto* bs = new QBarSet("频数", this);
    QVector<qreal> vals;
    for (qreal f : m_freqs)
        vals.append(f);
    bs->setValues(vals);
    bs->setColor(QColor("#42A5F5"));
    m_barSets.append(bs);
    bs->setParent(this);

    qDebug() << "[Histogram] bins:" << nBins << " maxF:" << maxF << " freqs:" << m_freqs;
    emit dataChanged();
}