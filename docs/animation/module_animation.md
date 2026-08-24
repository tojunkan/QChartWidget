# module_animation.md —— animation 模块

> 属于 t53 文档套件；配套 deepdive：`docs/animation/deepdive_animation.md`（动画机制：QPropertyAnimation 优先 + 自定义动画原理）。
> 全部类在 `include/animation/` + `src/animation/`（无 2d/3d 子目录）。

## 1. 职责与边界

animation = **动画家族**（基于 `QAbstractAnimation` 的自定义动画）。核心原则（D3）：**标量属性动画优先用 QPropertyAnimation**（颜色/透明度等 Q_PROPERTY + NOTIFY → invalidate）；只有"单属性表达不了"的才写自定义动画——逐点 morph（QBarAnimation/QNumericSeriesAnimation）、投影间插值（QProjectionSwitchAnimation）、相机路径（QViewRectAnimation）。

- 依赖（.cpp 级）：`core`（QChartWidget）、`projection`（QChartProjection/QInterpolatedProjection）、`series`（QXYSeries/QBarSeries）。
- 被依赖：demo（Test/demos/*）、TestUnit（动画相关测试）。
- 头级仅依赖 Qt + 基类 QChartAnimation（无跨模块 include——依赖全部在 .cpp，见 overview §6 依赖图）。

## 2. 文件与类清单

| 文件 | 类 | Q_OBJECT | 动画对象 |
|---|---|---|---|
| QChartAnimation.h/.cpp | `QChartAnimation : QAbstractAnimation`（基类） | ✓ | —（抽象 animate(alpha)） |
| QBarAnimation.h/.cpp | `QBarAnimation` | ✓ | QBarSeries（柱数值 rect 逐点 morph） |
| QNumericSeriesAnimation.h/.cpp | `QNumericSeriesAnimation` | ✓ | QXYSeries（折线数值点逐点 morph） |
| QViewRectAnimation.h/.cpp | `QViewRectAnimation` | ✓ | QChartWidget（viewRect：相机位置+缩放） |
| QProjectionSwitchAnimation.h/.cpp | `QProjectionSwitchAnimation` | ✓ | QChartWidget（投影切换：经 QInterpolatedProjection 插值） |

## 3. 公共 API 一览

**QChartAnimation（基类）**
- `setDuration(ms)`、`setEasingCurve(QEasingCurve)`。
- `updateCurrentTime(currentTime)`（Qt 驱动每帧）→ `animate(easedAlpha)`（纯虚，α∈[0,1] 已含缓动）。
- 约定：**Qt 动画驱动每帧**；子类只实现 `animate(qreal easedAlpha)`。

**QBarAnimation**
- `setTargetSeries(QBarSeries*)`、`setSourceRects(numericRects)`、`setTargetRects(...)`；`setGenerator(Generator)`（数据生成器模式，demo_sort 用）。

**QNumericSeriesAnimation**
- `setTargetSeries(QXYSeries*)`、`setSourcePoints(numericPts)`、`setTargetPoints(...)`；`setGenerator(Generator)`。

**QViewRectAnimation**
- `setTargetWidget(QChartWidget*)`、`setTargetViewRect(target)`（必设）、`setWaypoint(center)`（可选：相机中心弧线经过点）、`setSizeCurve(std::function<qreal(qreal)>)`（可选：视图宽度 vs α）；`setGenerator`。

**QProjectionSwitchAnimation**
- `setTargetWidget(QChartWidget*)`、`setTargetProjection(unique_ptr<QChartProjection> dst)`；`updateState`（动画结束时把插值投影替换为最终投影）。

## 4. 信号槽

动画类本身无自定义信号（继承 QAbstractAnimation 的 finished/stateChanged）。连接在 demo 侧：`finished → 收尾（如投影最终化）`；Widget 侧由属性/重绘通知驱动（QPropertyAnimation 场景：Q_PROPERTY NOTIFY → invalidate）。

## 5. 核心机制

1. **统一动画骨架**：`updateCurrentTime → animate(easedAlpha)`——Qt 负责时间轴与缓动，子类只做"α 到几何"的映射（幂等、可重入）。
2. **逐点 morph**（QBarAnimation/QNumericSeriesAnimation）：源/目标同构数值数组之间按 α 线性插值（逐点），每帧整批更新（非每帧重采样）。
3. **相机路径**（QViewRectAnimation）：viewRect 从当前到 target 的插值；可选 waypoint（中心弧线）与 sizeCurve（宽度曲线）——"单属性表达不了"的典型自定义动画。
4. **投影切换**（QProjectionSwitchAnimation）：持有 `QInterpolatedProjection{a,b,α}`，动画期间 widget 的 Projection 被替换为插值投影；结束 `updateState` 把最终投影落定（避免残留插值投影）。
5. **Generator 模式**：setGenerator 提供每帧数据生成函数（demo_sort 冒泡排序演示）——动画帧驱动数据而非数据驱动动画。

## 6. 核心函数一览表（含文件位置）

| 函数 | 职责 | 调用方 | 文件 |
|---|---|---|---|
| `QChartAnimation::updateCurrentTime` | 每帧驱动 → animate(缓动后 α) | Qt 动画框架 | src/animation/QChartAnimation.cpp |
| `QBarAnimation::animate(α)` | 柱 rect 逐点插值 → 系列更新 | 动画框架 | src/animation/QBarAnimation.cpp |
| `QNumericSeriesAnimation::animate(α)` | 折线数值点逐点插值 | 动画框架 | src/animation/QNumericSeriesAnimation.cpp |
| `QViewRectAnimation::animate(α)` | viewRect 插值（waypoint/sizeCurve 生效） | 动画框架 | src/animation/QViewRectAnimation.cpp |
| `QProjectionSwitchAnimation::animate(α)` | 插值投影 α 推进 | 动画框架 | src/animation/QProjectionSwitchAnimation.cpp |
| `QProjectionSwitchAnimation::updateState` | 结束落定最终投影 | Qt 动画框架 | src/animation/QProjectionSwitchAnimation.cpp |

## 7. 设计文档对应

- D3（动画优先 QPropertyAnimation）：`docs/design/design_notes.md`（§Pan/Zoom 相关）与 ROADMAP 决策记录。
- 投影切换/插值投影：`docs/design/design_notes.md`（§Projection 统一性）与 QInterpolatedProjection 头注释。
- demo 用法：Test/demos/demo_sort.cpp（Generator）、demo_camera.cpp（viewRect 动画）、demo_swirl.cpp（投影切换）。
