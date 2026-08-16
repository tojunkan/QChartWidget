# 图例 设计（Phase 1）

> 状态：定稿（拆分稿，待用户确认）。本文件只覆盖图例；主题/调色板见 `design_theme.md`（§2 override 颜色模型为本功能文字色的公共机制），导出见 `design_export.md`。
> 决策依据：t5 问卷 B1=a, B2=a, B3=a, B4=a。

## 0. 用户已确认的决策速览（图例）

| 题 | 决策 |
|---|---|
| B1 | 图例 = plotArea 内 overlay（四角可选），Phase 1 不做外置布局 |
| B2 | 图例 = 独立 `QChartLegend` 类 |
| B3 | 图例内容 = 汇总所有 layer 的所有 series（跳过空 name） |
| B4 | 图例点击 = 切换系列 visible |

## 1. 背景：QChartScene 是什么

`QChartScene` 是 Phase 0 已存在的「场景快照」结构体（现定义于 `QChartRenderer.h:18`，当前名为 `ChartScene`）：渲染器的入参，`QChartWidget` 在 `paintEvent` 组装、`renderer->render(scene, device)` 消费。Phase 1 只对它做两件事：① 纯改名 `ChartScene → QChartScene`（字段与语义不变）；② 加字段。本功能给它加 `legend / legendItems` 两个字段，让渲染器在画完所有 series 后绘制图例，也让图例随导出一起出现。

## 2. QChartLegend 类（B2：独立类；overlay，非 QWidget）

```cpp
// QChartLegend.h（Q_OBJECT —— QObject，由 renderer 在 plotArea 内绘制，交互由 widget 转发）
class QChartLegend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment NOTIFY alignmentChanged)
public:
    explicit QChartLegend(QObject* parent = nullptr);

    bool isVisible() const;  void setVisible(bool);
    // alignment 用 Qt::AlignLeft|AlignTop（左上，默认）/ AlignRight|AlignTop /
    //            AlignLeft|AlignBottom / AlignRight|AlignBottom 四角
    Qt::Alignment alignment() const;  void setAlignment(Qt::Alignment);

    // 文字色：override 模式（主题推 textColor 作默认；模型见 design_theme.md §2）
    void setTextColor(const QColor&);  void setThemeTextColor(const QColor&);
    void clearTextColor();  QColor textColor() const;

    // 绘制（单列竖向：色块 + 名字；隐藏系列降透明度）
    void draw(QPainter*, const QRectF& plotArea, const QList<QChartSeries*>& items) const;
    // 命中：返回命中的 series，未命中返回 nullptr
    QChartSeries* seriesAt(const QPointF& pos, const QRectF& plotArea,
                           const QList<QChartSeries*>& items) const;
    // 边界（供 widget 判点击、供将来外置布局预留）
    QRectF boundingRect(const QRectF& plotArea, const QList<QChartSeries*>& items) const;

signals:
    void visibleChanged(); void alignmentChanged();
};
```

关键点：
- `items` 由 widget 组装（B3：汇总所有 layer 的 series、跳过空 name、按 add 顺序），图例自身不反向依赖 widget/layer。
- `draw()` 读 `series->color()`（已按 `design_theme.md` §2 的 override 模型解析）、`series->name()`、`series->isVisible()`；隐藏项用半透明色块/文字表示。
- Phase 1 只做单列竖向；横向/分页留待后续。

## 3. QChartWidget 集成

```cpp
// 持有 + 暴露
QChartLegend* legend() const;
void setLegendVisible(bool);  bool isLegendVisible() const;
void setLegendAlignment(Qt::Alignment);

QList<QChartSeries*> m_legendItems;   // paint 前按「layer 顺序 + add 顺序」重建，跳过空 name
```

- `paintEvent`：`scene.legend = m_legend; scene.legendItems = m_legendItems;`（`QChartScene` 字段见 `design_export.md` §1.1）。
- 渲染：`QPainterChartRenderer::drawForeground` 在画完所有 layer 的 series 后，`if (legend && legend->isVisible()) legend->draw(p, plotArea, legendItems)`（clip 到 plotArea）。
- **点击切换可见性（B4）信号链路**：
  - `mousePressEvent` 中，**先于 pan 分支**判断：若图例可见，`s = legend->seriesAt(e->pos(), m_plotArea, m_legendItems)`；命中则 `s->setVisible(!s->isVisible())` 并 `return`（不进入 pan）。
  - `setVisible` → 既有 `QChartSeries::visibleChanged` → 已连接的 `invalidateForeground()`（QChartWidget.cpp:66）自动重绘系列 + 图例。**无需新信号接线**，链路已存在。
- `m_legendItems` 重建时机：`seriesAdded` / `seriesRemoved` / `series nameChanged`（name 变化会影响是否被跳过）。重建后 `invalidateForeground()`。

## 4. 文件与构建（本功能部分）

| 文件 | 说明 | Q_OBJECT / moc |
|---|---|---|
| `QChartLegend.h` / `QChartLegend.cpp` | 图例类 | **有**（QObject） |

跨文档共享前提（只做一次）：engineer 首步把现有 `ChartScene` 重命名为 `QChartScene`（`QChartRenderer.h:18` 纯改名，字段与语义不变，不破坏 moc/构建）；本功能随后给它加 `legend / legendItems` 字段。

CMakeLists.txt 改动（**不动 target 结构 / 不动 moc 所有权约定**）：`set(QCHART_SOURCES ...)` 追加 `QChartLegend.cpp`（`.h` 由库目标 `AUTOMOC ON` 自动 moc，`QChartLegend` 的 moc 唯一编入静态库，符合约定）。

## 5. 测试计划（可独立断言）

遵循既有 TestUnit 约定：`tests/test_qchartlegend.h`（Q_OBJECT 类声明）+ `.cpp` 实现，接入 `CMakeLists`（QCHART_TEST_HEADERS + QChartTests 源）与 `TestUnit/main.cpp`（include + `QTest::qExec` 一行）。

### 5.1 图例（test_qchartlegend）
- `boundingRect` 落在 plotArea 内且四角锚点正确（四角各测一次）。
- `seriesAt`：构造 items，点击色块区域命中正确 series，点击空白返回 nullptr。
- items 过滤：空 name series 不产生 item（由 widget 组装逻辑测试，或给图例一个辅助过滤器）。
- 绘制非空：`QPainterChartRenderer::renderUncached` 到 QImage，比较图例 visible/隐藏时图像不同（像素采样）。

### 5.2 图例交互集成（test_legend_interact）
- 构造 widget + 1 layer + 2 series + 图例可见；模拟点击图例项坐标 → 对应 `series->isVisible()` 翻转；再次点击复原。
- 点击 plotArea 空白不触发切换（pan 逻辑不被图例误伤）。

## 6. 实现任务拆分（本功能部分，逐 task reviewer 审查）

> 排期更新（用户决定）：图例**合并为一个实现任务 + 一个审查任务**——`task-legend`（QChartLegend 类 + 绘制 + B4 点击交互 + §5 全部测试）+ `rev-legend`（合并审查）。原拆分如下仅供参考：
> 每个 task 完成即交 reviewer（D9），通过才进下一个。

1. **task-legend-draw**：`QChartLegend` 类（四角 overlay 绘制 + boundingRect + seriesAt + textColor override）接入 CMake；`QChartScene.legend/legendItems` + renderer drawForeground 末尾画图例 + widget 组装 legendItems；单测 test_qchartlegend（锚点/命中/过滤/绘制非空）。依赖：task-theme-widget（图例读已解析色，见 `design_theme.md` §7）。
2. **task-legend-interact**：B4 点击切换可见性（mousePressEvent 图例优先分支）；单测点击切换。依赖：task-legend-draw。
