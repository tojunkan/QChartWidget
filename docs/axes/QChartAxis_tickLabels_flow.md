# QChartAxis_tickLabels_flow.md —— 标签格式化管道

> t55 核心函数 flow · axes 模块
> 主题：`QValueAxis::tickLabels`（三态管道）与各子类的格式化差异（QLogAxis 科学计数/QDateTimeAxis 时间格式/QBarCategoryAxis 类别名），以及 2D/3D 的调用路径。

## 控制流（调用图）

```
2D：QPainterChartRenderer::drawBackground → drawAtEdge/drawAtPosition
      └─ axis->tickLabels(ticks)（tickValues 之后）
3D：QChartLayer3D::collectPrimitives
      └─ QChartAxes3D::tickLabelTexts(dim, lo, hi)
           └─ axis->tickLabels(axis->tickValues(lo, hi))      # 委托链（两跳）

子类实现：
QValueAxis::tickLabels(ticks)
  ├─ m_labelFormat 非空 → QString::asprintf(format, v)          # ① printf 格式（如 "%.2f°"）
  ├─ else m_labelPrecision ≥ 0 → QString::number(v, 'f', precision)   # ② 固定小数位
  └─ else → QString::number(v, 'f', 8) 去尾零 → 去尾点            # ③ 自动去零

QLogAxis::tickLabels(ticks)
  ├─ value = m_base^t（t 为 Numeric 刻度）
  ├─ value ≥1e4 或 ≤1e-3 → 科学计数 "%1e%2"（系数 'g',3 + 指数）
  └─ else → QString::number(value, 'g', 4)

QDateTimeAxis::tickLabels(ticks)
  └─ QDateTime::fromMSecsSinceEpoch(t).toString(m_format)

QBarCategoryAxis::tickLabels(ticks)
  └─ categories[t]（索引取整）
```

## 数据流（入参/出参/状态变更）

| 子类 | 入参 | 格式化依据 | 出参 |
|---|---|---|---|
| QValueAxis | `QVector<qreal> ticks` | m_labelFormat > m_labelPrecision > 自动去零（优先级降序） | `QStringList`（与 ticks 等长） |
| QLogAxis | 同 | 数值量级（≥1e4/≤1e-3 科学计数） | `QStringList` |
| QDateTimeAxis | 同 | m_format | `QStringList` |
| QBarCategoryAxis | 同 | categories 表 | `QStringList` |

状态变更：无（纯函数）。

## 时序（触发时机与先后）

1. **紧随 tickValues**：同一次绘制/收集内先 ticks 后 labels（QChartAxes3D::tickLabelTexts 内部两跳串行）。
2. **2D**：每绘制帧（范围变化 → 重绘 → 重格式化）。
3. **3D**：collectPrimitives 内（labels 出参 → billboard 标签，GL/Painter overlay 同源）。

## 边界与陷阱

1. **自动去零**：'f',8 后 chomp 尾零与孤立小数点（"5.00000000"→"5"）——浮点 8 位精度上限内保证无科学计数伪影。
2. **printf 格式注入**：asprintf 用用户格式串——非法格式串可能截断/未定义（Qt asprintf 安全返回；文档提示用户格式合法性）。
3. **QLogAxis 指数显示**：`%1e%2` 拼接（系数 3 位有效 + 整数指数）——t 为 log 值取 floor 作指数（负指数正确）。
4. **QDateTimeAxis 月近似**：chooseStep 的月毫秒近似 → 跨月标签可能落在近月位置（已知近似）。

## 关联

- Called By：QPainterChartRenderer（2D）/QChartAxes3D::tickLabelTexts（3D）。
- 上游：docs/axes/QChartAxis_tickValues_flow.md（刻度生成）；下游：billboard 标签绘制（QChartTextLabel，QPainterChartRenderer::drawLabels / GlHost overlay）。
- 相关决策：design_notes §Axis（标签格式化第三件事）。
