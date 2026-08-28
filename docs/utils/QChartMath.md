# QChartMath Documentation

## Brief Introduction:
3D 数学纯函数集（D-3D-1）：`namespace QChartMath`，**header-only inline 自由函数**（非类，无 Q_OBJECT，无 .cpp——CMake QCHART_SOURCES 零改动）。职责：**Clip → NDC → Screen 显式拆分**（Phase 3 GL 顶点着色器同用）、矩阵构造辅助（frustum 参数集中约定）、深度辅助、批量投影入口（Phase 3 GPU 批量/预转换时签名不变，D-3D-10）、反投影（Phase 3 射线拾取预留）。全部纯函数无状态、可单测（TestQChartMath）。文档结构按队长指示调整为**函数清单表**（自由函数，列名照范式）。

## Constant Variables:
None.（namespace 无常量；函数内局部默认参数见各函数）

## Member Variables:
None.（无状态命名空间）

## Function List (namespace QChartMath 自由函数):

| Return Value Type | Name | Description | Parameters | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `QVector3D` | `clipToNdc` | Clip → NDC：`÷w`；**w≤0（相机背后/近平面外）→ NaN 哨兵**。 | `const QVector4D& clip` | `QVector3D`/NaN | `clipToScreen`/`projectBatch` | — |
| `QPointF` | `ndcToScreen` | NDC → Screen：`x=left+(ndc.x+1)/2·w`；`y=bottom−(ndc.y+1)/2·h`（**y 翻转**与 2D cartesianToPixel 一致）。 | `const QVector3D& ndc` <br> `const QRectF& plotArea` | `QPointF` | `clipToScreen`/`projectBatch` | `QChartCamera2D`（一致性对照） |
| `QPointF` | `clipToScreen` | **组合**：Clip → Screen（含 w≤0 → NaN 检查）。 | `const QVector4D& clip` <br> `const QRectF& plotArea` | `QPointF`/NaN | `QChartCamera3D::project`/Layer3D/GL 路径 | `QChartCamera3D` <br> `QChartLayer3D` |
| `QMatrix4x4` | `perspectiveMatrix` | 透视矩阵（QMatrix4x4::perspective 封装，frustum 参数集中约定）。 | `qreal fovYDeg, qreal aspect` <br> `qreal nearP, qreal farP` | `QMatrix4x4` | `QChartCamera3D::projectionMatrix` | `QChartCamera3D` |
| `QMatrix4x4` | `orthographicMatrix` | 正交矩阵（QMatrix4x4::ortho 封装）。 | `qreal left, right, bottom, top` <br> `qreal nearP, qreal farP` | `QMatrix4x4` | `QChartCamera3D::projectionMatrix`（正交模式） | `QChartCamera3D` |
| `qreal` | `viewDepth` | 视图空间深度 = **−viewZ**（相机前方正；越大越远——painter 排序键）。 | `const QMatrix4x4& viewMatrix` <br> `const QVector3D& worldPoint` | `qreal` | Renderer 排序/emitLine/`projectBatch` | — |
| `void` | `projectBatch` | **World 批量投影**：逐点 clipToScreen/viewDepth；**两数组逐元素对齐**（w≤0 → NaN 槽位保留，不压缩）；Phase 3 换 GPU 批量时签名不变。 | `const QMatrix4x4& viewProj` <br> `const QMatrix4x4& view` <br> `const QRectF& plotArea` <br> `const QVector<QVector3D>& world` <br> `QVector<QPointF>* outScreen` <br> `QVector<qreal>* outDepth` | —（出参数组） | Phase 3 预转换预留（当前单测覆盖） | — |
| `QVector3D` | `unproject` | 反投影（Phase 3 射线拾取预留）：逆 viewProj ÷w；**w≤0 或矩阵不可逆 → NaN**。 | `const QMatrix4x4& viewProj` <br> `const QVector4D& clipPos` | `QVector3D`/NaN | Phase 3 预留（当前无库内调用） | — |

Notes:
- **w≤0 哨兵约定**：全库 3D 投影统一遵守（相机后方点 NaN，调用方跳过——不画、不进包围盒）。
- **y 翻转一致性**：ndcToScreen 与 2D cartesianToPixel 同向（View 上 → 像素上）——正交俯视 ≡ 2D 硬验收（D-3D-2）。
- 完整推导（Clip→NDC→Screen 拆分/y 翻转/深度/批量/反投影）：docs/utils/deepdive_math.md；clipToScreen 全链：docs/utils/QChartMath_clipToScreen_flow.md；projectBatch：docs/utils/QChartMath_projectBatch_flow.md。
- 非类：无构造函数/信号/事件；全部 inline 纯函数（header-only 红线：新增函数必须 inline，否则需加 .cpp）。

## Overrided Qt Events:
None.（非类）

## Signals:
None.（非 QObject）
