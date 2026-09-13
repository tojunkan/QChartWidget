// QChartPlotAreaLayout.cpp —— 直通式布局实现（见头文件注释：把 GL 宿主几何应用移出绘制阶段）
#include "QChartPlotAreaLayout.h"

#include <QWidget>
#include <QWidgetItem>
#include <QSize>
#include <QEvent>

QChartPlotAreaLayout::QChartPlotAreaLayout(QWidget* host, QWidget* parent)
    : QLayout(parent)
{
    setHost(host);
}

QChartPlotAreaLayout::~QChartPlotAreaLayout()
{
    // QLayout 析构会释放布局项；这里只清指针，避免悬挂（控件不归布局所有）
    m_item = nullptr;
    m_host = nullptr;
}

void QChartPlotAreaLayout::setPlotArea(const QRect& plotArea)
{
    if (m_plotArea == plotArea) return;
    m_plotArea = plotArea;
    invalidate();   // 交给 Qt：下一个布局阶段（事件阶段）调用 setGeometry()
}

void QChartPlotAreaLayout::setHost(QWidget* host)
{
    if (m_host == host) return;
    // 摘除旧项（布局项归本布局所有：takeAt 后由调用方释放）
    while (QLayoutItem* item = takeAt(0))
        delete item;
    if (m_host)
        m_host->removeEventFilter(this);
    m_host = host;
    if (m_host) {
        addItem(new QWidgetItem(m_host));
        // 宿主"由隐藏变可见"时补一次布局请求：否则此前因不可见被跳过的几何可能一直不生效
        // （Qt 的布局激活通常伴随 show/resize，但宿主可见性是本布局的跳过条件，需自兜底）。
        m_host->installEventFilter(this);
    }
    invalidate();
}

bool QChartPlotAreaLayout::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_host && event->type() == QEvent::Show)
        invalidate();   // 仍在布局阶段（事件阶段）应用几何，不在绘制回调内
    return QLayout::eventFilter(watched, event);
}

void QChartPlotAreaLayout::addItem(QLayoutItem* item)
{
    if (!item) return;
    if (m_item == item) return;
    delete m_item;          // 直通布局：只保留一个 item
    m_item = item;
    invalidate();
}

int QChartPlotAreaLayout::count() const
{
    return m_item ? 1 : 0;
}

QLayoutItem* QChartPlotAreaLayout::itemAt(int index) const
{
    return (index == 0) ? m_item : nullptr;
}

QLayoutItem* QChartPlotAreaLayout::takeAt(int index)
{
    if (index != 0 || !m_item) return nullptr;
    QLayoutItem* item = m_item;
    m_item = nullptr;
    invalidate();
    return item;
}

QSize QChartPlotAreaLayout::sizeHint() const
{
    return m_host ? m_host->sizeHint() : QSize(0, 0);
}

QSize QChartPlotAreaLayout::minimumSize() const
{
    return m_host ? m_host->minimumSize() : QSize(0, 0);
}

void QChartPlotAreaLayout::setGeometry(const QRect& rect)
{
    Q_UNUSED(rect);   // ★ 直通：忽略 Qt 传入的 rect，始终按保存的 plotArea 摆放宿主

    // 宿主为空 / 不可见 / 几何未变 → 直接返回（不触碰子控件 ⇒ 绘制期/空闲期的重复布局不会重设几何）
    if (!m_host || !m_host->isVisible() || m_host->geometry() == m_plotArea) {
        ++m_skipCount;
        return;
    }

    // 几何应用只发生在 Qt 的布局阶段（事件阶段）；不在此处触发任何重绘/重收集
    m_host->setGeometry(m_plotArea);
    ++m_applyCount;
}
