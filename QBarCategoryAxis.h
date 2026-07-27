#pragma once
#ifndef QBARCATEGORYAXIS_H
#define QBARCATEGORYAXIS_H

#include "QChartAxis.h"
#include <QStringList>

class QBarCategoryAxis : public QChartAxis {
    Q_OBJECT
        Q_PROPERTY(QStringList categories READ categories WRITE setCategories)

public:
    explicit QBarCategoryAxis(QObject* parent = nullptr,
        Qt::Alignment alignment = Qt::AlignBottom);

    CoordinateSystem coordinateSystem() const override;

    // ---------- 分类数据管理 ----------
    QStringList categories() const { return m_categories; }

    // ① 全量替换（用户做批量复杂操作时用这个，只触发一次信号）
    // 注意：set会自动触发update，这是为了防止把QStringList暴露给用户以后忘记加update
    void setCategories(const QStringList& categories);

    // ② 核心原子操作（覆盖 95% 的使用场景）
    void append(const QString& category);
    void insert(int index, const QString& category);
    void removeAt(int index);     // 按索引删（对应你的 int 版本）
    void clear();

    // ---------- 重写基类接口 ----------
    void setMin(qreal v) override;
    void setMax(qreal v) override;

    // ---------- 核心映射 ----------
    qreal valueToNormalized(qreal value) const override;
    qreal normalizedToValue(qreal norm) const override;

    // ---------- 刻度 ----------
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
    QVector<qreal> subTickValues() const override { return QVector<qreal>(); }
    QStringList subTickLabels() const override { return QStringList(); }

private:
    void updateRange(); // 内部统一更新

private:
    QStringList m_categories;
};

#endif // QBARCATEGORYAXIS_H