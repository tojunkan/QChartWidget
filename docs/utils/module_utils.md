# module_utils.md —— utils 模块

> 属于 t53 文档套件；配套 deepdive：`docs/utils/deepdive_math.md`（QChartMath 纯函数集）。
> 注意：`ProjectionToolKit.h` 在镜像骨架中位于 `include/utils/`（t52 沿用），但其组合对象在 projection 模块。

## 1. 职责与边界

utils = **叶子工具集**：3D 数学纯函数（QChartMath）、日志分类（QChartDebug）、Data 空间值类型（QDataPoint/QDataPoint3D/QDataRect）、函数投影工具箱（ProjectionToolKit）。全部无 Q_OBJECT、无 moc；除 ProjectionToolKit 外不依赖任何库内模块（纯叶子）。

- 依赖：ProjectionToolKit → `projection`（QFunctionalProjection/QChartProjectionFactory）；其余无。
- 被依赖：全库（QChartMath 被 core/axes/series 使用；QChartDebug 被各模块日志使用；值类型被 series 使用）。

## 2. 文件与类清单

| 文件 | 内容 | Q_OBJECT |
|---|---|---|
| include/utils/QChartMath.h | `namespace QChartMath`（header-only inline 纯函数） | — |
| include/utils/QChartDebug.h | 14 个 `Q_DECLARE_LOGGING_CATEGORY` 分类声明 | — |
| include/utils/QDataPoint.h | `QDataPoint{x,y: QVariant}`（2D Data 点） | — |
| include/utils/QDataPoint3D.h | `QDataPoint3D{x,y,z: QVariant}`（3D Data 点；定案：新建类不扩展 QDataPoint，design_3d §6.1） | — |
| include/utils/QDataRect.h | `QDataRect`（Data 空间矩形，QBarSeries 用） | — |
| include/utils/ProjectionToolKit.h | `createIdentityProjection/createPower2Projection/createExpProjection/createLogProjection`（函数投影工具箱） | — |

## 3. 公共 API 一览

**QChartMath（纯函数，inline）**
- Clip→NDC→Screen 显式拆分：`clipToNdc(clip)`（w≤0 → NaN 哨兵）、`ndcToScreen(ndc, plotArea)`（y 翻转：`bottom − (ndc.y+1)/2·h`，与 2D cartesianToPixel 一致）、`clipToScreen(clip, plotArea)`（组合，含 w≤0 检查）。
- 矩阵辅助：`perspectiveMatrix(fovYDeg, aspect, near, far)`、`orthographicMatrix(l,r,b,t,near,far)`（对 Qt 封装，集中约定）。
- 深度：`viewDepth(viewMatrix, worldPoint) = −viewZ`（越大越远，painter 排序键）。
- 批量投影（Phase 3 预留，签名不变）：`projectBatch(viewProj, view, plotArea, world, outScreen, outDepth)`（逐点投影，两数组对齐，w≤0 → NaN 槽位保留）。
- 反投影（Phase 3 射线拾取预留）：`unproject(viewProj, clipPos)`（逆 viewProj ÷w；w≤0/不可逆 → NaN）。

**QChartDebug**：14 个日志分类（logAxis/logWidget/logLayer/logSeries/logProjection/logFactory/logRender + 轴族分类 + 3 个 verbose 分类默认关），`QT_LOGGING_RULES` 环境变量控制（D8：`*` 通配符只能出现在模式末尾）。

**值类型**：`QDataPoint{x(),y(),setX,setY}`；`QDataPoint3D{x(),y(),z()}`；`QDataRect`（QBarSeries 柱数据）。

**ProjectionToolKit**：`createIdentityProjection()`（恒等）、`createPower2Projection()`、`createExpProjection()`、`createLogProjection()`——基于 `QChartProjectionFactory::createFunctional` 的即用函数投影（demo_swirl 使用）。

## 4. 信号槽

无（非 QObject）。

## 5. 核心机制

1. **纯函数可单测**：QChartMath 全部 inline 无状态——TestQChartMath 直接断言公式/哨兵/对齐，无 GL/Widget 依赖。
2. **w≤0 哨兵约定**：相机后方/近平面外的点产 NaN，调用方跳过——全库 3D 投影统一遵守。
3. **y 翻转一致性**：`ndcToScreen` 与 2D `cartesianToPixel` 同向（View 上 → 像素上），单测双向锁定。
4. **Phase 3 预留接口**：projectBatch/unproject 签名在 GPU 批量投影/射线拾取落地时不变（D-3D-10）。
5. **日志分类纪律**：每帧细节走 `*Verbose` 分类（默认关），避免刷屏；测试/demo 用 `QT_LOGGING_RULES` 按需开。

## 6. 核心函数一览表（含文件位置）

| 函数 | 职责 | 调用方 | 文件 |
|---|---|---|---|
| `QChartMath::clipToNdc/ndcToScreen/clipToScreen` | Clip→NDC→Screen 拆分（w≤0 NaN） | QChartCamera3D::project / Layer3D | include/utils/QChartMath.h |
| `QChartMath::perspectiveMatrix/orthographicMatrix` | 矩阵构造（集中约定） | QChartCamera3D::projectionMatrix | include/utils/QChartMath.h |
| `QChartMath::viewDepth` | 深度排序键（−viewZ） | Renderer 排序 / emitLine | include/utils/QChartMath.h |
| `QChartMath::projectBatch` | World 批量投影（两数组对齐） | Phase 3 预转换预留 | include/utils/QChartMath.h |
| `QChartMath::unproject` | 反投影（射线拾取预留） | Phase 3 预留 | include/utils/QChartMath.h |
| `ProjectionToolKit::createIdentity/Power2/Exp/LogProjection` | 即用函数投影 | demo（swirl 等） | include/utils/ProjectionToolKit.h |

## 7. 设计文档对应

- QChartMath：`docs/design/design_3d.md`（§3 3D 数学层、⚠ w≤0 NaN、y 翻转与 2D 一致）。
- 反投影/射线拾取预留：`docs/design/design_3d_axes.md`（§2.3）。
- 批量投影/GPU 预留：`docs/design/design_phase3.md`（§9）。
- QDataPoint3D 定案：`docs/design/design_3d.md`（§6.1）。
- 决策：D8（日志规则）、D-3D-1（QChartMath）、D-3D-10（Phase 3 批量预留）。
