# module_series.md —— series 模块

> 属于 t53 文档套件；配套 deepdive：`docs/series/deepdive_projectFn3D.md`（ProjectFn3D 全链闭包与 float3 权威存储）。
> 2D 与 3D 同属本模块（对仗代码目录 `include/series/{2d,3d}`），单篇分节。

## 1. 职责与边界

series = **系列家族**。核心分工（D15 红线延续）：**Series 只存 Data，零耦合**——不持 Axis/Projection/Widget/相机引用；绘制所需的映射（2D `toPixel` 闭包 / 3D `ProjectFn3D` 闭包）由上层组装注入。

- 依赖：`core`（3D 系列 include QChartCamera3D/QChartRenderer 的类型）、`utils`（QDataPoint/QDataPoint3D/QDataRect）。
- 被依赖：`layers`（QChartLayer include QChartSeries；QChartLayer3D 遍历 series3DList）、`core`（QChartWidget 增删系列；QChartHitTester 命中）、`animation`（.cpp include QXYSeries/QBarSeries）。
- 3D 子类归属：散点/线/曲面在 `series/3d`；2D 六个系列在 `series/2d`；基类 QChartSeries 在 `series/`。

## 2. 文件与类清单

| 文件 | 类 | Q_OBJECT | 信号 | Q_PROPERTY |
|---|---|---|---|---|
| include/series/QChartSeries.h + src/series/QChartSeries.cpp | `QChartSeries`（基类） | ✓ | nameChanged / visibleChanged / opacityChanged / colorChanged | 4（name/visible/opacity/color） |
| include/series/2d/QXYSeries.h + src/series/2d/QXYSeries.cpp | `QXYSeries`（2D 折线基类） | ✓ | dataChanged / renderOverrideChanged | — |
| include/series/2d/QLineSeries.h + src/series/2d/QLineSeries.cpp | `QLineSeries` | ✓ | dataChanged（继承） | 2（pen 相关） |
| include/series/2d/QScatterSeries.h + src/series/2d/QScatterSeries.cpp | `QScatterSeries` | ✓ | dataChanged | 2（markerSize/markerShape） |
| include/series/2d/QPolygonSeries.h + src/series/2d/QPolygonSeries.cpp | `QPolygonSeries` | ✓ | — | — |
| include/series/2d/QBarSeries.h + src/series/2d/QBarSeries.cpp | `QBarSeries` | ✓ | dataChanged / renderOverrideChanged | — |
| include/series/2d/QRegionSeries.h + src/series/2d/QRegionSeries.cpp | `QRegionSeries` | ✓ | — | — |
| include/series/3d/QChartSeries3D.h + src/series/3d/QChartSeries3D.cpp | `QChartSeries3D`（3D 基类） | ✓ | dataChanged | — |
| include/series/3d/QChartLineSeries3D.h + src/series/3d/QChartLineSeries3D.cpp | `QChartLineSeries3D` | ✓ | dataChanged | 2 |
| include/series/3d/QChartScatterSeries3D.h + src/series/3d/QChartScatterSeries3D.cpp | `QChartScatterSeries3D` | ✓ | dataChanged | 1（markerSize） |
| include/series/3d/QChartSurfaceSeries.h + src/series/3d/QChartSurfaceSeries.cpp | `QChartSurfaceSeries`（曲面线框，行主序网格） | ✓ | dataChanged | — |

## 3. 公共 API 一览

**QChartSeries（基类，2D/3D 共用）**
- 属性：`name/visible/opacity/color`（D12：`setColor` 显式 override、`setThemeColor` 主题默认、`clearColor` 回退）。
- 数据：`count()`（纯虚）。
- 绘制/命中：`draw(painter, toPixel 闭包, ctx)`（2D 签名）、`hitTest(pixel, toPixel, ctx)`。

**2D 子类**
- `QXYSeries`：`QVector<QDataPoint>` 权威；append/insert/remove/replace/clear + setRenderOverride(QList<QPointF>)/clearRenderOverride（绘制覆盖，通知 renderOverrideChanged）；`hitTest` = 像素 in 折线多边形。
- `QLineSeries`（折线）、`QScatterSeries`（点 + markerSize/markerShape）、`QPolygonSeries`（多边形）、`QBarSeries`（QDataRect 柱 + setRenderOverride(QList<QRectF>)）、`QRegionSeries`（区域）。
- 2D 系列绘制闭包：`toPixel(QVariant, QVariant)`（Numeric→Pixel 全链，Layer 组装注入）。

**QChartSeries3D（3D 基类）**
- 数据（Data 空间）：`points()/at()/count()`（API 语义与 Phase 2 完全一致）；`append(qreal×3)` 便捷重载 / `append(QVariant×3)` / `append(QDataPoint3D)` / `insert/remove/replace/clear/setPoints`。
- **float3 权威存储**（D28/t51）：`numericCache()`（12B/点，仅全数值型 append 激活）；`numericCacheActive()`（QVariant 路径/混合 → false 回退物化；clear 复位）。
- **World 缓存**（VBO 源，§9）：`worldCache()`（Layer3D 渲染时填充；series 零耦合——本类不持 Axis/Projection）。
- **图元收集**：`collectPrimitives(projectFn, out)`（纯虚：填 type/a/b/depth/dataIndex/color/markerSize/penWidth，不排序不绘制，D-3D-9）。
- `draw(painter, projectFn, ctx3D)`（3D 签名）；**重载隐藏红线**：基类 2D `draw` 桩 = qWarning + no-op，误经 `QChartSeries*` 调用 3D 系列会响亮失败而非静默画错。

**3D 子类**：`QChartScatterSeries3D`（点 + markerSize）、`QChartLineSeries3D`（折线 + 2 属性）、`QChartSurfaceSeries`（曲面线框，行主序网格）。

## 4. 信号槽表（谁连谁）

| 发送方 | 信号 | 接收方 | 动作 |
|---|---|---|---|
| QChartSeries | colorChanged / opacityChanged / visibleChanged / nameChanged | QChartWidget（seriesAdded 时连线） | invalidateForeground() |
| QXYSeries / QBarSeries | renderOverrideChanged | QChartWidget | invalidateForeground() |
| QChartSeries3D | dataChanged | QChartLayer3D / Widget3D | worldCache 置脏 → collectPrimitives 重建（视图变化槽重绘） |

## 5. 核心机制

1. **系列零耦合**（D15）：2D 靠 `toPixel` 闭包、3D 靠 `ProjectFn3D` 闭包注入（Layer/Widget 组装），系列不持任何映射对象引用——reviewer grep 验证点。
2. **双存储**（D28/t51）：数值型路径 float3 权威（12B/点）直读 GL；QVariant 路径 QDataPoint3D 列表权威；**首个 QVariant append 使 numericCacheActive()==false**，此前 float3 一次性物化回退；append/remove 增量维护、clear 复位、replace/insert 失效回退。
3. **World 缓存由 Layer3D 填充**：数值型 = `toWorld(numericCache)`；曲面 = `toWorld(toNumeric(grid))`；混合系列空；投影/数据变化才重建（数据变化本类自清 + Layer3D 置脏）。
4. **2D hitTest**：像素 in 多边形（QChartHitTester 统一入口，D27）。
5. **重载隐藏保护**：3D draw 与基类 2D draw 签名不同 → 隐藏而非覆盖；基类桩响亮失败防误用。

## 6. 核心函数一览表（含文件位置）

| 函数 | 职责 | 调用方 | 文件 |
|---|---|---|---|
| `QChartSeries::draw/hitTest`（2D 签名） | 2D 绘制/命中入口 | QPainterChartRenderer / QChartHitTester | src/series/QChartSeries.cpp |
| `QXYSeries::append/insert/replace/remove` | 数据维护（增量失效通知） | 用户/demo/动画 | src/series/2d/QXYSeries.cpp |
| `QXYSeries::setRenderOverride/clearRenderOverride` | 绘制覆盖（性能路径） | QChartWidget 交互 | src/series/2d/QXYSeries.cpp |
| `QChartSeries3D::append(qreal×3)` | 数值型路径 → float3 增量维护 | 用户/demo | src/series/3d/QChartSeries3D.cpp |
| `QChartSeries3D::append(QVariant×3)` | QVariant 路径 → 回退物化 | 用户/demo（任意 Axis 类型） | src/series/3d/QChartSeries3D.cpp |
| `QChartSeries3D::numericCache/numericCacheActive` | float3 权威存储访问 | QOpenGLChartRenderer（VBO 源） | src/series/3d/QChartSeries3D.cpp |
| `QChartSeries3D::collectPrimitives`（各子类） | 图元收集（depth/dataIndex 已填） | QChartLayer3D / GL buildBatches | src/series/3d/QChartSeries3D.cpp + 子类 |
| `QChartSurfaceSeries::collectPrimitives` | 曲面线框图元（行主序网格） | 同上 | src/series/3d/QChartSurfaceSeries.cpp |

## 7. 设计文档对应

- 2D 系列/两次裁剪/绘制：`docs/design/design_notes.md`（§Series 绘制、§Grid 绘制）。
- 3D 系列两层数据组织：`docs/design/design_3d.md`（§6 3D 系列、§6.1 QDataPoint3D 定案、§6.5 曲面线框）。
- float3/worldCache：`docs/design/design_phase3.md`（§9 数值预转换缓存）。
- 决策：D15（全链闭包）、D19（渲染边界数值型）、D28（内存预算与 float3）。
