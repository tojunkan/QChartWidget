## QPainter 图表库重构需求文档

### 1. 目标

从当前"铁板一块"的 `Widget` 设计，重构为 "容器（`ChartWidget`）+ 坐标系统（`Layer`）+ 数据系列（`Series`）+ 独立轴（`Axis`）" 四层架构。

实现高效缓存、灵活组合（双轴、多图层）、事件清晰分发、局部更新能力。

---

### 2. 整体架构

```text
┌─────────────────────────────────────────────┐
│              QChartWidget                    │
│  - 唯一绘图区（所有几何体共享 plotArea）   │
│  - 拥有所有 Axis（QList，负责 delete）      │
│  - 管理所有 Layer、Series                │
│  - 双层缓存（背景 = 轴标签+网格，           │
│    前景 = 数据+高亮+tooltip）               │
│  - 事件分发（按 Z-order 逆序穿透）         │
└─────────────────────────────────────────────┘
          │                    │
          │ owns               │ owns
          ▼                    ▼
┌─────────────────┐  ┌─────────────────────┐
│  QChartLayer │  │   QAbstractAxis     │
│  - 持有两个轴   │  │   - 标注了所有权    │
│   引用（裸指针）│  │    属于 ChartWidget │
│  - 坐标映射     │  │   - 范围/刻度/标签  │
│  - 绘制网格     │  │   - 发射 rangeChanged│
│  - 持有 Series  │  │   - sizeHint() 告知 │
│   列表（前景）  │  │     边距需求        │
├─────────────────┤  └─────────────────────┘
│ QCartesian      │
│ QPolar ← 新     │
└─────────────────┘
          │
          │ holds (Z-ordered list)
          ▼
┌─────────────────────────────────────────────┐
│            QAbstractSeries                   │
│  - 数据存储（点/柱/饼片等）                 │
│  - draw() 中执行视口裁剪                    │
│  - hitTest() 通过 m_layer 做坐标转换     │
│  - 预留图例接口：drawLegendMarker /         │
│    legendVisible / legendType               │
└─────────────────────────────────────────────┘
```

---

### 3. 模块职责与实现要点

**3.1 QChartWidget（继承 QWidget）**
- 成员：
  - `QList<QChartLayer*> m_geometries`（顺序即 `z-order`，后添加在上层）
  - `QList<QAbstractAxis*> m_axes`（**所有轴，ChartWidget 拥有所有权**）
  - `QPixmap m_backgroundCache, m_foregroundCache`
  - `bool m_backgroundDirty, m_foregroundDirty`
- 布局：`resizeEvent` 中遍历所有轴，调用 `axis->sizeHint(font)` 计算边距，为每个 `Layer` 分配 `plotArea`（所有 Layer 共享同一个 plotArea（重叠图层））。
- 绘制流程（见第4节）。
- 事件处理（见第5节）。

**3.2 QAbstractAxis（基类，QObject）**

**所有权**：轴由 `QChartWidget` 创建并持有（`m_axes` 列表 + `delete`）。Layer 只持有轴引用（裸指针），不负责生命周期。

使用流程：
```cpp
auto* axisX = new QValueAxis;
chartWidget->addAxis(axisX);        // 注册所有权
layer->setAxisX(axisX);          // 仅引用
```

- 子类：`QValueAxis`, `QLogAxis`, `QDateTimeAxis`, `QCategoryAxis`。

- 接口：
  - `setMin(qreal)`, `setMax(qreal)`（发射 `rangeChanged` 信号）
  - `setRange(qreal min, qreal max)`（内部调用 setMin+setMax）
  - `mapToPixel(qreal value) const` / `pixelToValue(qreal pixel) const`（基于已知的 `axisLength` 参数化）
  - `tickValues() -> QVector<qreal>`, `tickLabels() -> QStringList`
  - `virtual QSizeF sizeHint(const QFont& font) const = 0;`  返回该轴在给定字体下，刻度标签和标题所需的空间（宽度或高度），用于 `ChartWidget` 的 `resizeEvent` 计算边距。
  - `virtual void draw(QPainter* painter, const QRectF& plotArea, Qt::Alignment alignment) const = 0;`  在 `plotArea` 外侧绘制刻度线、标签、轴线和标题。`alignment` 指示该轴位于 `Left`、`Right`、`Top` 或 `Bottom`。

**绘制归属**：轴由 `ChartWidget` 统一管理，绘制时在 `drawBackground` 中直接调用 `axis->draw()`，不经过 `Layer`。这确保多轴（如左右双Y轴）能正确布局并绘制在 `plotArea` 两侧。

**异常值保护**：
- 所有映射函数开头执行 `if (!std::isfinite(value)) return 0.0;`。
- 对 `QLogAxis`：若输入值 `<= 0`，直接返回 `0` 或边界值。
- 对 `QBarCategoryAxis`：`pixelToValue` 返回最近的 category index。
- `QDateTimeAxis` 映射基于 epoch 秒（`qreal`），范围在 1970~3000 年内精度安全。

**3.3 QChartLayer（基类，QObject）**
- 职责：组合两个轴引用，形成完整坐标系，持有 Series 列表。

- 成员：
  - `QAbstractAxis* m_axisX{nullptr}, * m_axisY{nullptr}`（**裸指针，不持有所有权**）
  - `QRectF m_plotArea`（由 `ChartWidget` 在 layout 阶段赋值）
  - `QList<QAbstractSeries*> m_series`（**持有所有权**）
- 接口：
  - `mapToPixel(QPointF dataPos) -> QPointF`, `mapFromPixel(QPointF pixelPos) -> QPointF`
  - `drawGrid(QPainter*)`（绘制网格线 — 在 `drawBackground` 中调用）
  - `drawSeries(QPainter*)`（遍历 `m_series` 调用其 `draw` — 在 `drawForeground` 中调用）
  - `hitTest(QPointF pixelPos) -> QPair<QAbstractSeries*, int>`（遍历 Series 做命中检测）
- 子类：
  - `QCartesianLayer`：直角坐标，X/Y 均为数值轴。
  - `QPolarLayer`：极坐标，角度轴（`QAngleAxis` 或 `QCategoryAxis`）+ 径向轴（`QValueAxis`）。支持饼图、雷达图、极坐标散点等。

**3.4 QAbstractSeries（基类，QObject）**
- 数据容器（由子类定义：`QVector<QXYPoint>` 或 `QBarSet` 或 `Slice` 等）
- 成员：
  - `QChartLayer* m_layer`（关联，绘图和 hitTest 时用于坐标映射）
- 接口：
  - `draw(QPainter*)`（使用 `m_layer->mapToPixel()` 映射坐标，绘制数据）
  - `hitTest(QPointF dataPos) -> bool`（在数据坐标空间做命中检测）
  - `legendText()`, `legendColor()`
- 子类：
  - `QLineSeries`, `QScatterSeries`：使用 `QCartesianLayer`
  - `QBarSeries`：使用 `QCartesianLayer`（X=分类轴, Y=数值轴）
  - `QPieSeries`：使用 `QPolarLayer`（角度轴定义扇区，径向轴定义半径/内径）
  - `QHistogramSeries`：继承 `QBarSeries`，增加分箱逻辑

**3.5 布局规则**

- 一个 `QChartWidget` 只有一个绘图区，所有 Layer 共享同一个 `plotArea`（重叠绘制，类似图层）。
- `plotArea` 由 `resizeEvent` 统一计算，边距分配规则：
  - 遍历所有轴，根据其 `alignment`（`Left`/`Right`/`Top`/`Bottom`）分组。
  - 对于 `Left` 或 `Right` 轴，调用 `axis->sizeHint(font)` 得到所需宽度，累加到左或右边距。
  - 对于 `Top` 或 `Bottom` 轴，累加所需高度到顶部或底部边距。
  - 若轴有标题，额外考虑标题空间（在 `sizeHint` 中一并计算）。
  - `plotArea` = `rect()` 减去总边距。
- 图层顺序（Z-order）：`m_geometries` 列表顺序，**后添加的在最上层**。

---

### 4. 绘制流程（含双缓存）

**4.1 静态绘制流程**

```cpp
void paintEvent(QPaintEvent*) {
    QPainter painter(this);

    // ----- 背景层（轴标签 + 网格线）-----
    if (m_backgroundDirty || m_backgroundCache.isNull()) {
        m_backgroundCache = QPixmap(size() * devicePixelRatioF());
        m_backgroundCache.setDevicePixelRatio(devicePixelRatioF());
        m_backgroundCache.fill(Qt::transparent);
        QPainter bg(&m_backgroundCache);
        drawBackground(&bg);
        m_backgroundDirty = false;
    }
    painter.drawPixmap(0, 0, m_backgroundCache);

    // ----- 前景层（所有系列数据 + 动态元素）-----
    if (m_foregroundDirty || m_foregroundCache.isNull()) {
        m_foregroundCache = QPixmap(size() * devicePixelRatioF());
        m_foregroundCache.setDevicePixelRatio(devicePixelRatioF());
        m_foregroundCache.fill(Qt::transparent);
        QPainter fg(&m_foregroundCache);
        drawForeground(&fg);
        m_foregroundDirty = false;
    }
    painter.drawPixmap(0, 0, m_foregroundCache);
}
```

`drawBackground` 执行顺序：
1. 绘制背景色（fillRect）
2. **轴绘制**：遍历 `m_axes`，调用 `axis->draw(painter, plotArea, alignment)`（ChartWidget 直接调，不经过 Layer）
3. **网格绘制**：遍历 `m_geometries`，调用 `geom->drawGrid(painter)`（网格依赖 Layer 的 X/Y 轴组合，走 Layer）

`drawForeground` 执行顺序：
1. **Series 数据**：遍历 `m_geometries`，调用 `geom->drawSeries(painter)`
2. **动态叠加层**：当前高亮点、选区框、动画过渡、Tooltip 等

**4.2 动画机制**

动画不直接操作 `QPainter`，而是驱动**数据模型**发生变化：
- **数值动画**：`QVariantAnimation` 修改 Series 的数据值。
- **范围动画**：`QVariantAnimation` 修改 Axis 的 `min/max`（如缩放动画）。

流程：
1. 动画 `valueChanged` 槽中修改 Series 或 Axis。
2. 修改触发脏标记（Axis 的 `setMin/Max` 发射 `rangeChanged`，连接 `m_backgroundDirty = true`）。
3. `paintEvent` 根据最新数据/范围重建缓存。

缓存策略：
- 数据值动画：仅置脏 **前景缓存**。
- 轴范围动画：同时置脏 **背景 + 前景**。

**4.3 缓存控制**

```cpp
void setCachingEnabled(bool enabled);
bool isCachingEnabled() const;
```
- 禁用缓存时，`paintEvent` 直接绘制到 Widget 表面，每帧全量绘制。
- 默认启用双缓存。
- 缓存尺寸跟随 `devicePixelRatioF()`，确保高 DPI。
- `resizeEvent` 中检测到尺寸变化时，释放旧位图并置脏两个缓存。

---

### 5. 事件处理与交互逻辑

**5.1 鼠标操作映射**

| 操作 | 行为 |
|------|------|
| 左键拖拽 | 平移（Pan）— 分解为水平和垂直位移，更新所有同方向轴范围 |
| 滚轮滚动 | 以鼠标位置为中心缩放（Zoom）— 同时缩放 X 和 Y 轴 |
| Ctrl + 左键拖拽 | 矩形缩放（RubberBand Zoom）— **预留，初版不实现** |
| 悬停 | 高亮最近的数据点，显示 Tooltip |

**5.2 平移与缩放实现**

- **平移（Pan）**：
  1. 鼠标按下记录 `pressPos`，移动时计算像素位移 `delta`。
  2. 通过当前命中的 `Layer` 将 `delta` 转换为数据增量：
     - `dDataX = axisX->pixelToValue(delta.x()) - axisX->pixelToValue(0)`
     - `newMin = min - dDataX`
  3. 同步更新所有同方向轴（遍历相同 alignment 的轴）。
  4. 轴 `rangeChanged` 信号触发 `ChartWidget` 置脏重绘。

- **缩放（Zoom）**：
  1. 将鼠标像素位置转为数据坐标 `(cx, cy)`。
  2. `newMin = cx - (cx - oldMin) * factor`
  3. 若新范围小于 `minimumRange`，拒绝更新。
  4. 同步更新所有同方向轴。

**扩展预留**：`setActive(bool)` 使某些轴不参与联动更新。初版不实现，默认全部同步。

**5.3 几何体命中与事件传递**
- 鼠标事件到达时，逆序遍历 `m_geometries`（后添加在上层）。
- 检查 `geom->plotArea().contains(mousePos)` → 调用 `geom->hitTest(mousePos)`。
- 命中则停止遍历，发射 `seriesHovered` / `seriesClicked`。
- 未命中继续向下层传递。
- 不可见 Layer/Series（`isVisible() == false`）跳过。

**5.4 线程安全模型**

- 所有组件必须运行在主线程。
- 工作线程构建数据后通过信号（`Qt::QueuedConnection`）传递给主线程的 Series。
- 高频实时数据建议使用双缓冲（交换指针），而非逐点信号。

---

### 6. 缓存失效策略
- 数据变更（Series 增/删/改）→ `m_foregroundDirty = true`
- 轴范围变化 → `m_backgroundDirty = true, m_foregroundDirty = true`
- 悬停/动画 → `m_foregroundDirty = true`
- 窗口大小变化 → 两个缓存都置脏（需重新分配位图）

---

### 7. 扩展性设计
- 新增轴类型：继承 `QAbstractAxis`，实现刻度生成和标签绘制。
- 新增几何体：继承 `QChartLayer`，实现 `mapToPixel`, `mapFromPixel`, `drawGrid`。
- 新增系列：继承 `QAbstractSeries`，实现 `draw` 和 `hitTest`。

**极坐标体系**：
- `QPolarLayer` 组合角度轴 + 径向轴，形成完整的极坐标空间。
- `QPieSeries`：角度轴定扇区范围，径向轴定半径和内径。爆炸、起始角度退化为极坐标下的样式属性。命中测试、动画、图例全部自动继承 Polar。
- 角度轴和径向轴默认 `setVisible(false)` 以隐藏刻度（饼图场景），雷达图场景可开启。
- 未来应用：雷达图（`QRadarSeries`）、极坐标散点（`QScatterSeries + QPolarLayer`）。

**图例预留接口**：

```cpp
class QAbstractSeries : public QObject {
public:
    virtual void drawLegendMarker(QPainter* p, const QRectF& rect) const = 0;
    virtual bool isLegendVisible() const;
    virtual void setLegendVisible(bool on);
    enum LegendType { LineLegend, ScatterLegend, BarLegend, PieLegend };
    virtual LegendType legendType() const = 0;
signals:
    void legendVisibilityChanged();
};
```
- `drawLegendMarker`：折线图画线+点，散点图画圆，柱状图画矩形，饼图画小扇形。
- 绘制循环中检查 `!series->isLegendVisible()` 跳过隐藏系列，仅置脏前景缓存。
- `legendType()` 让图例组件决定装饰样式。

---

### 8. 迁移路径

采用**直接重写**策略，不保留旧 API 兼容层：

1. **提取轴类**：`QValueAxis` 等从原 Widget 剥离，继承 `QAbstractAxis`。
2. **实现坐标系**：`QCartesianLayer`, `QPolarLayer`。
3. **实现 Series**：`QBarSeries`, `QLineSeries`, `QScatterSeries`, `QPieSeries`, `QHistogramSeries`。
4. **实现 QChartWidget**：整合缓存、布局、事件分发。
5. **重写 main.cpp**：所有 Demo 用新 API。
6. **删除旧类**。

---

### 9. 性能目标

- 数据点 ≤ 10,000 时，全量重绘 < 16ms（60fps）。
- **视口裁剪（Viewport Culling）为强制要求**：
  - 每个 `Series::draw()` 检查数据包围盒与 `plotArea` 的交集。
  - 推荐 `painter->setClipRect(layer->plotArea())` 启用硬件裁剪，初版统一使用。
- 悬停/动画仅重绘前景层，开销 < 1ms。
- 缓存位图内存 ≤ 2 × 控件像素 × 4 字节（RGBA），1920×1080 下约 16MB。

---

### 10. 交付物清单
- 头文件：`QChartWidget.h`, `QAbstractAxis.h`, `QChartLayer.h`, `QAbstractSeries.h` 及子类。
- 实现文件：对应 .cpp。
- 示例 `main.cpp` 展示多图表组合。
