# 设计笔记

## 五空间模型

| # | 空间名 | 类型 | 存放/负责 | 用途 |
|---|---|---|---|---|
| 1 | Data | **任意类型**（Axis 子类决定） | Series 存储 | 用户提供的数据源 |
| 2 | Numeric | **qreal** | Axis::toNumeric() 产出 | 跨类型统一数字，喂给 Projection |
| 3 | View (Cartesian) | **QPointF**（物理长度单位） | Widget::m_viewRect | Pan/Zoom、空间变换 |
| 4 | ViewNorm | **QPointF**（[0,1]²） | Widget 内部 | 线性归一化 → 像素映射 |
| 5 | Pixel | **QPointF**（像素坐标） | QPainter | 最终绘制 |

### Data ≠ Numeric

**Data 和 Numeric 即使碰巧都是 qreal，也是两个不同的空间。** Data 是用户领域的值（带语义），Numeric 是数值化后的纯计算数字。

| Axis 子类 | Data 类型 | toNumeric(Data) → qreal | fromNumeric(qreal) → Data |
|---|---|---|---|
| QValueAxis | qreal | 恒等 | 恒等 |
| QLogAxis | qreal（>0） | log10(v) | pow(10, v) |
| QDateTimeAxis | QDateTime | toMSecsSinceEpoch() | QDateTime::fromMSecsSinceEpoch(v) |
| QBarCategoryAxis | QString | 类别索引 | categories[index] |

QValueAxis 的 Data 碰巧是 qreal，但语义上仍是独立的——不能直接把 Data 当 Numeric 用，因为同一条链路里另一个 Axis 可能是 QLogAxis，它的 Numeric 是 log 变换后的值。每个 Axis 定义自己一维的 Data↔Numeric 语义。

### 不存在 DataNorm 空间

旧设计中 Axis 将数据归一化到 [0,1] 是绕路——Projection 内部反手又从 dataBounds 反算回数据值。新设计不经过归一化。

---

## 核心链路

### 正向（绘制）

```
Data ──[Axis::toNumeric]──► Numeric ──[Projection::toCartesian]──► View Cartesian ──[线性]──► ViewNorm ──[线性]──► Pixel
```

### 反向（鼠标交互）

```
Pixel ──[逆线性]──► ViewNorm ──[逆线性]──► View Cartesian ──[Proj::fromCartesian]──► Numeric ──[Axis::fromNumeric]──► Data
```

### 边框轴（捷径，仅 Cartesian）

边框轴不需要走 Projection：Numeric 直接线性插值到 plotArea 边缘 → Pixel。无 View Cartesian 环节。

---

## Axis：数值化 + 刻度生成 + 标签格式化

Axis 不管坐标映射。它只做三件事：

1. **数值化**：`toNumeric(data) → qreal` / `fromNumeric(qreal) → data`（跨类型统一）
2. **刻度生成**：`tickValues(rangeMin, rangeMax) → QVector<qreal>`（在给定范围内生成合适刻度）
3. **标签格式化**：`tickLabels(ticks) → QStringList`

Axis 不拥有、不存储 min/max 作为数据映射的基准。Axis 没有 `valueToNormalized`、`normalizedToValue`、`pan`、`zoom`、`coordinateSystem`。

### min/max 语法糖（仅 Cartesian）

Axis 保留 `setMin/setMax/setRange` 接口，但**只作为便捷接口**，在 Cartesian 下映射到 Widget 的 viewRect 对应维度。非 Cartesian 下无效果（或 warning）。

```
// 用户的便捷接口：
angleAxis->setRange(0, 360);   // Cartesian: 映射到 viewRect dim0
// 等效于：
widget->setDataRangeX(0, 360); // 实际操作 viewRect
```

### Axis 子类

| 子类 | toNumeric | tickValues | tickLabels |
|---|---|---|---|
| QValueAxis | 恒等 | niceStep(rangeMin, rangeMax) | 数字（精度可配） |
| QLogAxis | log10(v) | log-space niceStep | 科学计数 |
| QDateTimeAxis | epoch秒 | 时间范围切分 | 日期格式 |
| QBarCategoryAxis | 类别→index | index/(n-1) | 类别名称 |

### 构造

```cpp
explicit QChartAxis(QObject* parent = nullptr,
                    Qt::Alignment alignment = Qt::AlignBottom);
```
默认 AlignBottom —— 边框轴是最常见、最简单的场景。

---

## 边框轴 vs 数据主脊

**边框轴（AlignBottom/Top/Left/Right）**：
- 画在 plotArea 边缘，**仅 Cartesian 有效**
- 不依赖 Projection、不需要另一个 Axis、不需要 offset
- 独立工作，单一维度线性插值

**数据主脊（AlignHCenter/VCenter）**：
- 画在 plotArea 内部，**所有坐标系有效**
- 依赖 Projection（轴在通用坐标系下会弯曲）
- 需要另一个 Axis 提供 offset（另一维度的数据值）来确定位置

### draw() 拆分为两个函数

```cpp
// 边框轴：画在 plotArea 对应边缘
void drawAtEdge(QPainter*, const DrawContext&, bool axisLine, bool labels, bool ticks) const;

// 数据主脊：画在 offset 指定位置（offset 来自另一个 Axis 的 tick 数据值）
void drawAtPosition(QPainter*, const DrawContext&, qreal offset, bool axisLine, bool labels, bool ticks) const;
```

调用方明确自己要哪种：
- Widget drawBackground: `drawAtEdge()` 给边框轴，`drawAtPosition()` 给数据主脊（offset=默认值或 0）
- Geometry drawGrid: `drawAtPosition()` 给每个 tick 值作 offset，仅画轴线不带标签刻度

---

## DrawContext

```cpp
struct DrawContext {
    QRectF plotArea;                  // 绘图区像素矩形
    QRectF dataBounds;                // 当前可见 Numeric 范围: (dim0Min, dim1Min, dim0Span, dim1Span)
    QRectF viewRect;                  // View Cartesian 空间的视窗矩形
    const QChartProjection* projection;
};
```

---

## Projection 统一性

QChartWidget 持有唯一的 Projection。所有组件共享。Axis 不存储 Projection 引用，通过 DrawContext 参数获取。

### Projection API

```cpp
class QChartProjection {
public:
    // 数值 → 中间 Cartesian（无需 dataBounds）
    virtual QPointF toCartesian(qreal num0, qreal num1) const = 0;
    virtual QPointF fromCartesian(qreal x, qreal y) const = 0;

    // dataBounds ↔ viewRect 双向转换
    virtual QRectF computeDataBounds(const QRectF& viewRect) const = 0;
    virtual QRectF computeViewRect(const QRectF& dataBounds) const = 0;

    // 默认可视范围（初始 viewRect 来源）
    virtual QRectF defaultDataBounds() const;

    // 采样数据空间曲线、投影后连接成 QPainterPath（自动处理不连续点）
    QPainterPath createPath(std::function<QPointF(qreal)> dataCurve,
                            int segments = 64) const;
    //   dataCurve(t∈[0,1]) → (num0, num1)，经 toCartesian 连成 path
};
```

- Cartesian：toCartesian/fromCartesian 恒等，computeDataBounds/computeViewRect 恒等
- Polar：需要沿矩形四边采样求极值
- Functional：由用户 Lambda 提供

---

## viewRect 与 dataBounds

### 驱动关系

- **Pan / Zoom** → 操作 viewRect（View Cartesian 空间） → 重算 dataBounds
- **setRange**（语法糖）→ 操作 dataBounds（Data 空间，仅 Cartesian 有效） → 重算 viewRect

### 初始值

Widget 构造时无 viewRect → 调用 `projection->defaultDataBounds()` → 算出初始 viewRect。

---

## 内存 Axis vs 屏幕线：一对多

一个内存 Axis 对象对应屏幕多条线。每次 `drawAtPosition()` 的 offset 不同，就画在不同位置。网格就是：取一个 Axis 的 tick 值做 offset，反复以另一个 Axis 画数据主脊。

Axis 不存储"画第几条线"的状态。每次调用无状态。

---

## Grid 绘制（Geometry 负责）

```cpp
// Geometry::drawGrid(painter, projection, viewRect, dataBounds):
// tickValues 返回 Numeric 空间的值，直接用作 offset，无需再 toNumeric
for (auto tickX : axisX->tickValues(dataBounds.left(), dataBounds.right())) {
    axisY->drawAtPosition(painter, ctx, tickX, /*line*/true, /*labels*/false, /*ticks*/false);
}
for (auto tickY : axisY->tickValues(dataBounds.top(), dataBounds.bottom())) {
    axisX->drawAtPosition(painter, ctx, tickY, /*line*/true, /*labels*/false, /*ticks*/false);
}
```

网格就是数据主脊只画线、不画标签和刻度。

---

## Series 绘制

Geometry 组装 `toPixel` 函数，Series 使用它画形状：

```cpp
// Geometry 组装：
auto toPixel = [&](qreal dx, qreal dy) -> QPointF {
    QPointF cart = projection->toCartesian(axisX->toNumeric(dx), axisY->toNumeric(dy));
    return widget->cartesianToPixel(cart);
};
series->draw(painter, toPixel);
```

Series 零耦合于 Axis/Projection/Widget 类型。

### 两次裁剪

1. **Data 空间粗筛**：dataBounds 内 → 排除明显不可见的数据点
2. **View Cartesian 精筛**：投影后落在 viewRect 内 → 画

---

## Pan / Zoom（QChartWidget）

Pan/zoom 操作 View Cartesian 空间的 viewRect，不涉及 Data 空间或 Axis。

- Pan: `viewRect.translate(dx, dy)`，dx/dy 由像素位移经线性映射反推到 Cartesian 空间
- Zoom: 以指定 Cartesian 点为中心缩放 viewRect，factor 由滚轮角度算出

操作后重算 dataBounds = projection->computeDataBounds(viewRect)，触发重绘。

---

## 弯曲轴上的刻度标记

对于数据主脊在 Polar 等坐标系下的弯曲路径上，不做法向量计算——直接以**点状标记**（drawPoint）替代线条 tick，避免复杂的局部法向量推导。

---

## 坐标有序性

`(dim0, dim1) ≠ (dim1, dim0)`。命名沿用 axisX/axisY 惯例（"X" = dim0，"Y" = dim1）。对应关系：

| | Cartesian | Polar |
|---|---|---|
| axisX (dim0) | 水平 | 角度 |
| axisY (dim1) | 垂直 | 径向 |

用户绑定错误时 Geometry 的验证逻辑应 Warning。

---

## 特殊值处理（NaN / Inf）

投影链上任意环节都可能产生 NaN 或 Inf（Polar 极点处的 fromCartesian、QLogAxis 的 toNumeric(0)→-inf、用户传入非法数据等）。

### 各环节的职责

| 环节 | 遇到 NaN/Inf 输入 | 输出 |
|---|---|---|
| toNumeric | 非法 Data（QLogAxis 收到 0、QBarCategoryAxis 收到未知类别） | 返回 NaN 并 qWarning |
| toCartesian | 任意 operands 为 NaN/Inf | 计算结果 → NaN 自然传播 |
| fromCartesian | 有效 Cartesian 输入（极点除外） | 有效 Numeric；**Polar 极点返回 (NaN, 0)** |
| fromNumeric | NaN | 见下表 |
| createPath | 采样点 toCartesian 返回 NaN | **断开路径**，moveTo 重开（已有实现） |
| toPixel (lambda) | toCartesian 返回含 NaN 的点 | **跳过该点**，不画 |

### fromNumeric 特殊值策略（每个子类自行定义）

| Axis 子类 | fromNumeric(NaN) | fromNumeric(-inf) | fromNumeric(+inf) |
|---|---|---|---|
| QValueAxis | 返回 NaN（直通） | 返回 -inf（直通） | 返回 +inf（直通） |
| QLogAxis | 返回 0（最接近 log(0) 的值） | 返回 0 | 返回 +inf |
| QDateTimeAxis | 返回无效 QDateTime | 返回 epoch=0 的 QDateTime | 返回 epoch=+inf 约值 |
| QBarCategoryAxis | 返回空 QString / -1 | 返回 -1 | 返回 -1 |

### 绘制时的统一策略

**所有绘制路径（Series 数据点、网格线、轴线）遇到 toPixel 返回 NaN 的点：跳过不画。** 这是唯一的、统一的上游保护——不做 clamp、不做修正、不静默吞掉。NaN 就是"不存在"，跳过即可。

---

## 待删除

- QAngleAxis → QValueAxis(min=0, max=360)，标签加"°"后缀
- QRadialAxis → QValueAxis(min=0, max=1)
- Axis::pan() / Axis::zoom()
- Axis::valueToNormalized() / Axis::normalizedToValue()
- Axis::coordinateSystem()
- Axis 的 `m_min / m_max` 作为数据归一化基准（保留作为语法糖字段）
