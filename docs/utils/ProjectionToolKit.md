# ProjectionToolKit Documentation

## Brief Introduction:
函数投影工具箱（header-only inline 自由函数，QChartProjectionFactory::createFunctional 之上的一层即用封装）：10 个复变/几何投影（恒等/Power2/Exp/Log/Sqrt/Mobius/Joukowski/Sin/鱼眼/漩涡）——每个函数返回 `std::unique_ptr<QChartProjection>`（QFunctionalProjection 实例）。文档结构按队长指示调整为**函数清单表**（自由函数）。demo_swirl 等使用。

## Constant Variables:
None.（各函数默认参数如 f=1.0/strength=0.5/radius=5.0 为函数级）

## Member Variables:
None.（无状态命名空间级函数）

## Function List (header-only 自由函数):

| Return Value Type | Name | Description | Parameters | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `std::unique_ptr<QChartProjection>` | `createIdentityProjection` | 恒等映射（Cartesian 语义；任意范围）。 | 无 | `unique_ptr<QFunctionalProjection>` | demo/用户 | `QFunctionalProjection` |
| `std::unique_ptr<QChartProjection>` | `createPower2Projection` | 幂函数 `w=z²=(x²−y², 2xy)`（主平方根 backward）。 | 无 | 同上 | 用户 | 同上 |
| `std::unique_ptr<QChartProjection>` | `createExpProjection` | 指数 `w=e^z=(eˣcos y, eˣsin y)`（原点奇点 NaN）。 | 无 | 同上 | 用户 | 同上 |
| `std::unique_ptr<QChartProjection>` | `createLogProjection` | 对数 `w=Log z=(ln\|z\|, arg z)`（z=0 奇点 NaN）。 | 无 | 同上 | 用户 | 同上 |
| `std::unique_ptr<QChartProjection>` | `createSqrtProjection` | 平方根主分支 `w=√z`（右半平面）。 | 无 | 同上 | 用户 | 同上 |
| `std::unique_ptr<QChartProjection>` | `createMobiusProjection` | 莫比乌斯变换 `w=(az+b)/(cz+d)`（奇点 cz+d=0 → NaN）。 | `qreal a=1, b=0, c=0.5, d=1` | 同上 | 用户 | 同上 |
| `std::unique_ptr<QChartProjection>` | `createJoukowskiProjection` | 茹科夫斯基 `w=z+1/z`（z=0 奇点 NaN）。 | 无 | 同上 | 用户 | 同上 |
| `std::unique_ptr<QChartProjection>` | `createSinProjection` | 正弦 `w=sin z`（backward 返回 NaN——仅演示）。 | 无 | 同上 | 用户 | 同上 |
| `std::unique_ptr<QChartProjection>` | `createFisheyeEquidistantProjection` | 等距鱼眼（二维入射角→图像平面；等距逆公式）。 | `qreal f=1.0` | 同上 | 用户 | 同上 |
| `std::unique_ptr<QChartProjection>` | `createSwirlProjection` | 漩涡扭曲（极坐标旋转；r 超界保持原样）。 | `qreal strength=0.5` <br> `qreal radius=5.0` | 同上 | demo（swirl）/用户 | 同上 |

Notes:
- **奇点策略**：各投影 backward 在奇点返回 NaN（Exp/Log/Joukowski 的 0、Mobius 的 cz+d=0）——调用方跳过（与全库奇点 NaN 约定一致）。
- **反向缺失**：createSinProjection 的 backward 仅演示（返回 NaN）——反向交互/反算不可用（文档化）。
- 每个函数内部经 `QChartProjectionFactory::createFunctional`（forward/backward/defaultBounds/dataToView=null/viewToData=null/轴名）——包络走采样 fallback。
- 非类（无信号/事件）；header-only inline（#pragma once）。

## Overrided Qt Events:
None.

## Signals:
None.（非 QObject）
