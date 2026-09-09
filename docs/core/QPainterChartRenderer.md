# QPainterChartRenderer Documentation

## Brief Introduction:
QPainterChartRenderer 是 **CPU/QPainter 后端渲染器**（继承 QChartRenderer；v2 修订：批次 B1 起 **3D 分支恢复**，2D/3D 双路径按相机类型分发）。步骤 2 在 CPU 完成：`transformNumericToCartesian` 逐图元按类型调 `projection->toCartesian`（Rect/Ellipse 恒等投影直算 cartRect，否则退化为 4 顶点 Polygon）；`cullAndResolveLabels` 精确裁剪并解析绑定/自由标签（R6 零面积盒微扩、R5/F1 越界守卫保留）。步骤 3/4 分发：`dynamic_cast<QChartCamera*>` → **2D 分支**（drawPrimitives2D/drawLabels2D：顶点 project → QPainter）；`dynamic_cast<QChartCamera3D*>` → **3D 分支**（drawPrimitives3D：**painter's algorithm**——逐可见图元取相机投影 depth（Point 本体 / Line 中点 / Polygon·Path·Mesh=QCube(verts).center()），按 depth 降序（远→近）排序后投影绘制，非有限/越界跳过；drawLabels3D：非有限/plotArea 外跳过 + 共享 drawLabel）；未知相机类型 qWarning。裁剪：`isPrimitiveVisible` 2D→viewRect 精确测试；3D→`isPrimitiveVisible3D`（图元包围盒 × 相机 viewCube：Point=contains、Line=端点为盒的 intersects、Rect/Ellipse=z0 薄片近似（qWarning 注明未实现精确裁剪）、顶点类=AABB intersects）。头文件含 QCube.h（3D 包围盒）。

## Constant Variables:
None.

## Member Variables:
None.（private 无成员；复用基类 m_viewDirty/m_visibilityCache）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QPainterChartRenderer` | 构造（default） | 无 | public | — | QChartAbstractWidget（m_cpuRenderer）、测试栈对象 | — |
| — | `~QPainterChartRenderer` | 析构（default override） | 无 | public | — | — | — |
| `void` | `transformNumericToCartesian` | 覆写：proj 空返回；按 Type——Point/Line 变换端点；Rect/Ellipse：恒等投影直算 cartRect，否则 type 改写 Polygon + 4 角变换；Polygon/Path/三角族逐点变换 + cartIndices=numIndices | `QChartScene& scene` | protected | — | `QChartRenderer::render`（m_viewDirty） | `QChartAbstractProjection` |
| `void` | `cullAndResolveLabels` | 覆写：visibility.resize(N)；逐图元 isPrimitiveVisible（按相机 2D/3D）；可见回填 lastVisibleIndex[sourceId]（0≤sourceId<size 守卫）；绑定标签锚=图元 cartA/可见性继承；自由标签按 sourceId 组最后可见图元 | `QChartScene& scene` | protected | — | `QChartRenderer::render` | — |
| `void` | `drawPrimitives` | 覆写：device/camera 空返回；QPainter（抗锯齿+clipRect）；**2D → drawPrimitives2D；QChartCamera3D → drawPrimitives3D；否则 qWarning** | `QChartScene& scene, QPaintDevice* device, const QVector<bool>& visibility` | protected | — | `QChartRenderer::render` | `QChartCamera` <br> `QChartCamera3D` |
| `void` | `drawPrimitives2D` | （private）2D 逐图元像素绘制（v1 语义：Point 圆半径 markerSize/2、Line/Rect/Ellipse/Polygon/Path/三角族投影绘制） | `QPainter& painter, const QChartScene& scene, const QChartCamera* cam2d` | private | — | drawPrimitives（2D 分支） | `QChartCamera` |
| `void` | `drawPrimitives3D` | （private，批次 B1 恢复）painter 算法：①可见索引收集 → ②逐图元 depth=cam3d->project(...).depth（Point 本体 / Line 中点 / Rect·Ellipse 跳过 / Polygon·Path·三角族=QCube(cartVerts).center()）写 prim.depth → ③按 depth 降序排序（远→近）→ ④逐图元投影绘制（非有限屏幕跳过；逐类型绘制经 cam3d->project） | `QPainter& painter, QChartScene& scene, const QChartCamera3D* cam3d` | private | — | drawPrimitives（3D 分支） | `QChartCamera3D` <br> `QCube` |
| `void` | `drawLabels` | 覆写：同 drawPrimitives 分派（2D/3D/未知警告） | `QChartScene& scene, QPaintDevice* device` | protected | — | `QChartRenderer::render` | — |
| `void` | `drawLabels2D` | （private）2D 标签（v1 语义：project + plotArea 内 + 共享 drawLabel） | `QPainter&, const QChartScene&, const QChartCamera*` | private | — | drawLabels（2D 分支） | — |
| `void` | `drawLabels3D` | （private，批次 B1 恢复）3D 标签：visible → cam3d->project；**非有限屏幕坐标/plotArea 外跳过** → 共享 drawLabel | `QPainter& painter, const QChartScene& scene, const QChartCamera3D* cam3d` | private | — | drawLabels（3D 分支） | `QChartCamera3D` |
| `bool` | `isPrimitiveVisible` | （private）camera 空 → true；2D → isPrimitiveVisible2D(viewRect)；3D → isPrimitiveVisible3D(**cam3d->viewCube()**)；未知 → true | `const QChartPrimitive& prim, const QChartAbstractCamera* camera` | private | `true`/`false` | cullAndResolveLabels | — |
| `bool` | `isPrimitiveVisible2D` | （private）2D 精确裁剪（v1 语义：点含/线边相交/盒相交/退化盒微扩 ±1e-6） | `const QChartPrimitive& prim, const QRectF& viewRect` | private | `true`/`false` | isPrimitiveVisible | — |
| `bool` | `isPrimitiveVisible3D` | （private，批次 B1 恢复）3D 裁剪：Point=viewCube.contains(cartA)；Line=端点盒 intersects；Rect/Ellipse=z0 薄片盒（qWarning 注明近似）；Polygon/Path/三角族=顶点 AABB intersects；空顶点 false | `const QChartPrimitive& prim, const QCube& viewCube` | private | `true`/`false` | isPrimitiveVisible | `QCube` |

Notes:
- 3D 通路数据流（实测）：QChartLayer3D::collectPrimitives 产出 Numeric 图元（scene3D）→ 本类 transform（scene3D.projection=projection3D->toCartesian）→ cull（viewCube）→ drawPrimitives3D 排序绘制；scene3D.camera 恒为 layer3D 的 m_camera3D。
- 3D 深度：depth=视图深度（−viewZ），排序降序=远→近（painter 算法；GL 端深度批次语义后续批次，见阶段记录 v2 §7 informational①）。
- v1 版"3D 路径 #if0 后置"表述已过时（批次 B1 恢复：头文件声明与 .cpp 定义均已启用）；其余 v1 语义（F1/R6/R9 引用等）不变。

## Overrided Qt Events:
无（非 QWidget）。

## Signals:
None.（非 QObject）
