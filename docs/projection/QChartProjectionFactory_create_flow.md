# QChartProjectionFactory_create_flow.md —— 分派表

> t55 核心函数 flow · projection 模块（include/projection/QChartProjectionFactory.h `create`，header-only 内联）

## 控制流（调用图）

```
QChartWidget::setProjection / demo（构造默认投影）/ 测试
  └─ QChartProjectionFactory::create(CoordinateSystem type)
       ├─ case Cartesian   → make_unique<QCartesianProjection>()   + qCDebug(logFactory)
       ├─ case Polar       → make_unique<QPolarProjection>()       + qCDebug(logFactory)
       ├─ case Functional  → qWarning「Cannot create QFunctionalProjection
       │                      without mapping functions — use createFunctional()」→ nullptr
       └─ default（未知枚举）→ qWarning「Unknown CoordinateSystem」→ make_unique<QCartesianProjection>()（回退）

  QChartProjectionFactory::createFunctional(forward, backward, defaultBounds, dataToView, viewToData, name0, name1)
    └─ make_unique<QFunctionalProjection>(...)                        # lambda 版，无分派
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `CoordinateSystem type`（Cartesian/Polar/Functional/未知） |
| 出参 | `std::unique_ptr<QChartProjection>`（工厂所有权转移给调用方） |
| 状态变更 | 无（纯静态） |
| 所有权 | unique_ptr 独占——QChartWidget::m_projection 持有；替换时旧对象析构 |

**分派语义要点**：
- **Functional 是契约性拒绝**（非回退）：create(Functional) 无法提供 lambda → 返回 nullptr + qWarning——调用方必须改用 createFunctional；若调用方直接 `setProjection(nullptr)` 语义由 Widget 处理。
- **未知枚举是宽容回退**：防枚举扩展破坏——回退 Cartesian + qWarning（宁可画笛卡尔不崩溃）。
- 日志分类：logFactory（QChartDebug.h 声明）。

## 时序（触发时机与先后）

1. **构造期/切换期**：QChartWidget 首次 setProjection（或用户/demo 切换投影类型）时调用——一次调用即得完整投影对象。
2. **切换后**：Widget 同步坐标系到所有 Layer（setProjection → layer setCoordinateSystem(type)）→ invalidate → 重绘。
3. **动画场景不走工厂**：投影切换动画用 QInterpolatedProjection 合成（a/b 非持有），不重新 create。

## 关联

- Called By：QChartWidget::setProjection 默认路径/demo/测试；createFunctional 被 ProjectionToolKit（utils）调用。
- 相关决策：D11（QChart 命名规范）、design_notes §Projection 统一性。
