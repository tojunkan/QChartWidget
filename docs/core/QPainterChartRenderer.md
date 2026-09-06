# QPainterChartRenderer Documentation

## Brief Introduction:
QPainterChartRenderer 是 **CPU/QPainter 后端渲染器**（继承 QChartRenderer，S0 主渲染路径）。步骤 2 在 CPU 完成：`transformNumericToCartesian` 逐图元按类型调 `projection->toCartesian`（Rect/Ellipse 恒等投影直算 cartRect，否则**退化转换**为 4 顶点 Polygon 再变换）；`cullAndResolveLabels` 精确裁剪（点=viewRect.contains、线=端点/边相交、盒=相交、顶点类=包围盒相交，**零面积盒微扩 1e-6 判交**——R6 修复）并解析绑定/自由标签。步骤 3/4 经 `dynamic_cast<const QChartCamera*>` 走 **2D 分支**（drawPrimitives2D/drawLabels2D）：图元逐顶点 `camera->project` → QPainter 绘制；标签走共享 `drawLabel` 排版。**3D 路径（drawPrimitives3D/drawLabels3D/isPrimitiveVisible3D 等 #if 0 块）随 3D 阶段恢复**——3D 相机 typeinfo/moc 未入 S0，非 2D 相机时仅 qWarning 后跳过。

## Constant Variables:
None.

## Member Variables:
None.（private 无成员；复用基类 m_viewDirty/m_visibilityCache）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QPainterChartRenderer` | 构造（default） | 无 | public | — | 测试（栈对象 `QPainterChartRenderer renderer; renderer.render(...)`） | — |
| — | `~QPainterChartRenderer` | 析构（default override） | 无 | public | — | — | — |
| `void` | `transformNumericToCartesian` | 覆写：proj 空返回；逐图元按 Type——Point/Line 变换端点；Rect/Ellipse：isIdentityMapping → 对角点变换成 cartRect，否则改写 type=Polygon + numVerts 4 角 + cartVerts 变换；Polygon/Path/三角族：cartVerts.resize 逐点变换 + cartIndices=numIndices | `QChartScene& scene` | protected | — | `QChartRenderer::render`（m_viewDirty 时） | `QChartAbstractProjection` |
| `void` | `cullAndResolveLabels` | 覆写：visibility.resize(N)；逐图元 `isPrimitiveVisible(prim, camera)`；可见图元回填 `lastVisibleIndex[sourceId]`（**F1 守卫：仅 0≤sourceId<size 写入**——防直接装配图元 sourceId=-1 越界）；绑定标签：锚=图元 cartA、visible=该图元可见性；自由标签：sourceId 组最后可见图元锚定，无则不可见 | `QChartScene& scene` | protected | — | `QChartRenderer::render` | `QChartScene` |
| `void` | `drawPrimitives` | 覆写：device/camera 空返回；建 QPainter（抗锯齿 + clipRect(plotArea)）；`dynamic_cast` 2D → drawPrimitives2D；否则 qWarning（3D 阶段恢复） | `QChartScene& scene, QPaintDevice* device, const QVector<bool>& visibility` | protected | — | `QChartRenderer::render` | `QChartCamera` |
| `void` | `drawLabels` | 覆写：同 drawPrimitives 分派（2D → drawLabels2D；非 2D 相机 qWarning） | `QChartScene& scene, QPaintDevice* device` | protected | — | `QChartRenderer::render` | `QChartCamera` |
| `bool` | `isPrimitiveVisible` | （private）camera 空 → true；2D → `isPrimitiveVisible2D(prim, cam2d->viewRect())`；非 2D → true（3D viewCube 裁剪随 3D 恢复） | `const QChartPrimitive& prim, const QChartAbstractCamera* camera` | private | `true`/`false` | `cullAndResolveLabels` 内部 | `QChartCamera` |
| `bool` | `isPrimitiveVisible2D` | （private）2D 精确裁剪：Point=viewRect 含点；Line=端点含或与四边 BoundedIntersection；Rect/Ellipse=viewRect.intersects(cartRect)；Polygon/Path/三角族=顶点 AABB 与 viewRect 相交（**宽或高≤0 的退化盒 adjust ±1e-6 后判交**——R6，防水平/垂直轴脊被误裁）；空顶点 false | `const QChartPrimitive& prim, const QRectF& viewRect` | private | `true`/`false` | `isPrimitiveVisible` 内部 | — |
| `void` | `drawPrimitives2D` | （private）按 m_visibilityCache 循环：Point → brush 画圆（半径=markerSize×0.5）；Line → 两点投影连线；Rect/Ellipse → 对角投影画矩形/椭圆（fill）；Polygon → 顶点序列投影 drawPolygon(fill)；Path → drawPolyline（NoBrush）；三角族：Mesh 有索引按 3 索引一组、Fan/Strip 连续 3 顶点一组 drawPolygon | `QPainter& painter, const QChartScene& scene, const QChartCamera* cam2d` | private | — | `drawPrimitives` 内部 | `QChartCamera` |
| `void` | `drawLabels2D` | （private）循环 scene.labels：visible 才处理；`cam2d->project(cartesianAnchor)` 出屏外跳过；共享 `drawLabel` 排版绘制 | `QPainter& painter, const QChartScene& scene, const QChartCamera* cam2d` | private | — | `drawLabels` 内部 | `QChartCamera` |

Notes:
- 头注释契约：S0 CPU 渲染器只含 2D 路径；3D 相关私有方法声明已注释（drawPrimitives3D/drawLabels3D/isPrimitiveVisible3D 的 #if 0 定义保留在 .cpp，待 3D 阶段恢复启用，QCube 引用随之恢复）。
- 像素语义：轴脊/刻度/标签像素断言（unit 6/6、矩阵 CPU 8/8）全部经本后端 offscreen 实测通过。

## Overrided Qt Events:
无（非 QWidget）。

## Signals:
None.（非 QObject）
