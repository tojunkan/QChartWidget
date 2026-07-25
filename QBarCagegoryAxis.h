#pragma once
// ===== QBarCategoryAxis =====
class QBarCategoryAxis : public QChartAxis {
    Q_OBJECT
        Q_PROPERTY(QStringList categories READ categories WRITE setCategories NOTIFY categoriesChanged)
public:
    explicit QBarCategoryAxis(QObject* p = nullptr);
    qreal mapToPixel(qreal v, qreal len) const override;
    qreal pixelToValue(qreal p, qreal len) const override;
    QStringList categories() const { return m_categories; }
    void setCategories(const QStringList& cats);
    void append(const QString& cat); void insert(int idx, const QString& cat); void remove(const QString& cat); void clear();
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
signals: void categoriesChanged();
private: QStringList m_categories;
};

