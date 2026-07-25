#ifndef QHISTOGRAMSERIES_H
#define QHISTOGRAMSERIES_H

#include "QBarSeries.h"
#include <QVector>
#include <QString>

class QHistogramSeries : public QBarSeries
{
    Q_OBJECT

public:
    // 可选直方图 bin 宽度算法
    enum BinMethod {
        Sturges,            // 斯特吉斯公式：log2(n)+1
        FreedmanDiaconis,   // Freedman-Diaconis 规则：2*IQR/n^(1/3) （默认）
        Scott,              // Scott 规则：3.5*σ/n^(1/3)
        Fixed               // 用户固定宽度
    };

    explicit QHistogramSeries(const QString& name = {}, QObject* parent = nullptr);

    // 设置原始数据（会自动排序并计算直方图）
    void setRawData(const QVector<qreal>& data);

    // 获取原始数据（已排序）
    const QVector<qreal>& rawData() const { return m_rawData; }

    // 手动强制重新计算（在修改了 bin 方法或固定宽度后调用）
    void computeBins();

    // 计算当前 bin 边界（基于选定的算法）
    QVector<qreal> binEdges() const;

    // 计算当前 bin 中心（用于分类标签）
    QVector<qreal> binCenters() const;

    // 获取各 bin 的频率（计数）
    QVector<qreal> frequencies() const { return m_freqs; }

    // ---- 新增配置接口 ----
    void setBinMethod(BinMethod method) { m_binMethod = method; }
    BinMethod binMethod() const { return m_binMethod; }

    void setFixedBinWidth(qreal width) { m_fixedBinWidth = width; }
    qreal fixedBinWidth() const { return m_fixedBinWidth; }

private:
    // 计算四分位距（依赖 m_rawData 已排序）
    qreal interquartileRange() const;

private:
    QVector<qreal> m_rawData;   // 原始数据（排序后）
    QVector<qreal> m_freqs;     // 每个 bin 的频率
    int m_binCount = 0;         // 用户可指定固定 bin 数（目前未使用，保留扩展）
    BinMethod m_binMethod = FreedmanDiaconis;  // 默认推荐算法
    qreal m_fixedBinWidth = 0.0;               // 仅当 method == Fixed 时有效
};

#endif // QHISTOGRAMSERIES_H