# QChartSeries3D Documentation

## Brief Introduction:
3D 系列基类（D15/D28，: QChartSeries 白捡 name/visible/opacity/color/主题色/图例）。**两层数据组织**：Data 层存储 + World 层渲染时产生（全链闭包 ProjectFn3D 内完成，系列零耦合）。**Phase 3 双存储（t51，A3 内存预算必要条件）**：数值型路径 → `m_numericCache`（float3 权威，12B/点，QVector<QVector3D>）；QVariant 路径 → `m_points`（QDataPoint3D 列表权威，Phase 2 边界不变）；**首个 QVariant 路径 append 使 numericCacheActive()==false 回退物化**。`points()/at()/count()` API 语义与 Phase 2 完全一致（numeric-only 时 QVariant 按需物化）。worldCache（VBO 源）由 QChartLayer3D 渲染时填充。**draw 重载隐藏红线**：3D draw(projectFn) 与基类 2D draw(toPixel) 签名不同 → 基类桩 = qWarning + no-op（误经 QChartSeries* 调用响亮失败）。子类：散点/线/曲面。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QVector<QDataPoint3D>` | `m_points` | QVariant 路径权威列表（Data 空间三元组；混合/回退时权威）。 | `QVector<QDataPoint3D>` | 空 | `QDataPoint3D` |
| `QVector<QVector3D>` | `m_numericCache` | **float3 权威缓存**（数值型路径，12B/点；GL 直读免 QVariant 解包，D28）。 | `QVector<QVector3D>` | 空 | — |
| `bool` | `m_numericCacheActive` | float3 激活标记（全数值型 append → true；首个 QVariant → false 回退；clear 复位 true）。 | `true`/`false` | `true` | — |
| `QVector<QDataPoint3D>` | `m_pointsView` | numeric-only 时的按需物化视图（points() 懒构建）。 | `QVector<QDataPoint3D>` | 空 | `QDataPoint3D` |
| `bool` | `m_pointsViewValid` | 物化视图有效性（数据变更置 false）。 | `true`/`false` | `false` | — |
| `QVector<QVector3D>` | `m_worldCache` | World 缓存（VBO 源；Layer3D 渲染时填充；series 零耦合——本类不持 Axis/Projection）。 | `QVector<QVector3D>` | 空 | `QChartLayer3D` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartSeries3D` | 构造函数。 | `const QString& name={}` <br> `QObject* parent=nullptr` | public | — | 子类构造 | — |
| `int` | `count` | 数据量（numericCacheActive ? numericCache.size : m_points.size）。 | 无 | public override | `int` | 渲染/测试 | — |
| `const QVector<QDataPoint3D>&` | `points` | 数据访问（numeric-only → 按需物化视图；否则 m_points 权威）。 | 无 | public | `QVector<QDataPoint3D>` | 测试/外部 | `QDataPoint3D` |
| `QDataPoint3D` | `at` | 单点访问（numeric-only → float3 **单点物化**；否则 m_points）。 | `int i` | public | `QDataPoint3D` | collectPrimitives（全链闭包喂点） | `QDataPoint3D` |
| `void` | `append` | 追加 QDataPoint3D（numericCacheActive → 先物化回退再追加，顺序语义保持）。 | `const QDataPoint3D& pt` | public | — | 用户 | `QDataPoint3D` |
| `void` | `append` | **数值型便捷**：qreal×3 → float3 增量维护（numericCacheActive 时）。 | `qreal x, y, z` | public | — | 用户/demo（QValueAxis 场景） | — |
| `void` | `append` | QVariant×3 追加（首个 QVariant → 回退物化）。 | `QVariant x, y, z` | public | — | 用户（任意 Axis 类型） | — |
| `void` | `insert` | 插入（numericCacheActive → 先物化回退再插入，与 replace 一致）。 | `int index` <br> `const QDataPoint3D& pt` | public | — | 用户 | `QDataPoint3D` |
| `void` | `remove` | 移除（numeric-only → **缓存增量维护**免回退；否则 m_points）。 | `int index` | public | — | 用户 | — |
| `void` | `replace` | 替换（numericCacheActive → 物化回退再替换——replace 失效定案）。 | `int index` <br> `const QDataPoint3D& pt` | public | — | 用户 | `QDataPoint3D` |
| `void` | `clear` | 清空（两缓存 + 视图失效 + numericCacheActive 复位 true）。 | 无 | public | — | 用户 | — |
| `void` | `setPoints` | 整批替换（QVariant 路径）。 | `const QVector<QDataPoint3D>& pts` | public | — | 用户 | `QDataPoint3D` |
| `const QVector<QVector3D>&` | `numericCache` | float3 访问器（GL 渲染直读）。 | 无 | public | `QVector<QVector3D>` | `QOpenGLChartRenderer`（VBO 源） | — |
| `bool` | `numericCacheActive` | float3 激活状态访问器。 | 无 | public | `true`/`false` | 渲染/测试 | — |
| `const QVector<QVector3D>&` | `worldCache` | World 缓存访问器（const）。 | 无 | public | `QVector<QVector3D>` | 渲染（VBO 打包） | — |
| `QVector<QVector3D>&` | `worldCache` | World 缓存可变引用（Layer3D 填充入口，内部）。 | 无 | public | `QVector<QVector3D>` | `QChartLayer3D`（collectPrimitives 直算） | `QChartLayer3D` |
| `void` | `collectPrimitives` | **纯虚**：图元收集（projectFn 闭包；type/a/b/depth/dataIndex/color/markerSize/penWidth 已填；不排序不绘制，D-3D-9）。 | `const ProjectFn3D& projectFn` <br> `QVector<QChartPrimitive>& out` | public pure virtual | — | Renderer（collectScene）/Layer3D | `QChartPrimitive` <br> `ProjectFn3D` |
| `void` | `draw` | **3D 签名**：直接绘制入口（painter's algorithm 关闭/单系列调试）。 | `QPainter*` <br> `const ProjectFn3D&` <br> `const DrawContext3D* ctx=nullptr` | public virtual | — | 调试/demo | — |
| `void` | `draw` | **基类 2D 签名桩**：qWarning + no-op（重载隐藏防护——误经 QChartSeries* 调用响亮失败）。 | `QPainter*` <br> `std::function<QPointF(QVariant,QVariant)>` <br> `const DrawContext*` | public override | — | （不应被调用） | — |
| `void` | `materializeToQVariant` | 私有：float3 → QDataPoint3D 一次性物化（回退路径；清缓存 + 置 inactive + 视图失效）。 | 无 | private | — | `append(QDataPoint3D/QVariant)`/`insert`/`replace` | `QDataPoint3D` |

Notes:
- **双存储全语义（激活/物化/失效）**：docs/series/QChartSeries3D_append_at_flow.md（★用户内存关切核心）。
- **图元规则**：散点 1 顶点/点、线 n−1 段、曲面 rows·(cols−1)+cols·(rows−1)——见各子类 doc 与 deepdive_projectFn3D。
- 共享类型 `ProjectFn3D = std::function<QChartProjectedPoint(const QDataPoint3D&)>`（本头定义；Layer3D 组装注入）。

## Overrided Qt Events:
None.（QObject）

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `dataChanged` | 数据变化（append/insert/remove/replace/clear/setPoints）。 | — | `QChartLayer3D`（hookSeriesDirty → worldCache 置脏，add/removeSeries3D 时接线） | `QChartLayer3D` |

Notes:
- Connected slots 实测：QChartLayer3D::hookSeriesDirty（src/layers/3d/QChartLayer3D.cpp）——3D 系列数据变化 → worldCache 重建；2D 侧基类属性信号仍连 QChartWidget（invalidateForeground）。
