// QChartPlotAreaLayout.h —— 直通式布局：把 GL 宿主的几何应用交给 Qt 布局阶段（t82 第一步）
//
// 背景（用户已批准的第一步修复）：Windows 侧 resize 崩溃定位到“**在绘制回调里改 GL 子控件几何**”
// 这条重入路径（paintEvent → relayout() → layoutGlHost() → 子控件 setGeometry；页堆下为必崩的
// use-after-free，块属 Intel GL 驱动）。用户裁定用 QLayout 把子控件几何交给 Qt 的**布局阶段**，
// 从绘制阶段彻底剥离（而不是用定时器推迟）。
//
// 本布局的语义（故意极小、直通）：
//   · 只管理**一个**目标子控件（GL 宿主）；
//   · setPlotArea(rect) 记录宿主应当占据的矩形并 invalidate()（随后 Qt 会投递 LayoutRequest，
//     在**事件阶段**调用 setGeometry()）；
//   · setGeometry(Qt 传入的 rect) **忽略传入 rect**：始终按保存的 plotArea 摆放宿主；
//     并在“宿主为空 / 宿主不可见 / 几何未变”时**直接返回**（不触碰子控件 —— 这就是
//     “同一几何重复触发布局不再调用 setGeometry”的可计数保护）。
//   · 不声明 Q_OBJECT（无 moc 依赖；不引入信号槽/元对象）。
//
// 边界（本步不做）：plotArea 的**计算**时机与内容脏模型（轴范围/刻度/样式/网格模式 → 重收集）
// 一概不动；把 plotArea 计算也收进布局属“批次五”单独立项。
#ifndef QCHARTPLOTAREALAYOUT_H
#define QCHARTPLOTAREALAYOUT_H

#include <QLayout>
#include <QRect>

class QWidget;
class QLayoutItem;

class QChartPlotAreaLayout : public QLayout
{
public:
    /// host = 被摆放的目标子控件（可为 nullptr，稍后 setHost 挂上）；parent = 布局宿主（外层 widget）
    explicit QChartPlotAreaLayout(QWidget* host, QWidget* parent = nullptr);
    ~QChartPlotAreaLayout() override;

    /// 目标几何（逻辑坐标；由 widget 的 relayout() 计算后喂入）。变化时 invalidate()。
    void setPlotArea(const QRect& plotArea);
    QRect plotArea() const { return m_plotArea; }

    /// 挂上/摘除目标子控件（切后端用；nullptr = 摘除）。摘除只释放布局项，不销毁控件。
    void setHost(QWidget* host);
    QWidget* host() const { return m_host; }

    // ---- QLayout 必须实现（直通：至多一个 item）----
    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;

    /// 宿主"由隐藏变可见"时补一次布局请求（不可见期间被跳过的几何需自兜底；仍在事件阶段应用）
    bool eventFilter(QObject* watched, QEvent* event) override;

    // ---- 诊断计数（测试/取证用；t82 验收②：几何未变不得重设）----
    int applyCount() const { return m_applyCount; }   // 真正写入子控件几何的次数
    int skipCount() const { return m_skipCount; }     // 因“宿主空/不可见/几何未变”跳过的次数
    void resetCounters() { m_applyCount = 0; m_skipCount = 0; }

private:
    QWidget* m_host = nullptr;
    QRect m_plotArea;
    QLayoutItem* m_item = nullptr;   // 直通：唯一布局项（QLayout 析构负责释放）
    int m_applyCount = 0;
    int m_skipCount = 0;
};

#endif // QCHARTPLOTAREALAYOUT_H
