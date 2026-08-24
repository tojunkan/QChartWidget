# deepdive_viewRect.md —— viewRect→plotArea 映射与 viewCube 派生链

> t53 核心计算深挖 · core 模块
> 主题：`QChartCamera2D::cartesianToPixel` 全推导 + `QChartCamera3D` viewCube→相机派生链（含 2D 是 3D 退化特例的数学论证）。

---

## 0. 问题背景

2D 的"视窗几何"全部收敛在 `QChartCamera2D`：`viewRect`（View Cartesian 空间，物理长度单位）→ `plotArea`（Pixel 空间，像素）的线性映射是绘制与交互的正反两条链路的枢纽；3D 的对应物是 `QChartCamera3D` 的 viewCube（World 轴对齐盒）与派生相机参数。本 deepdive 给出两条链路的完整推导、源码位置、边界与单测锚点。

## 1. 数学推导：viewRect → plotArea（2D）

### 1.1 正向（View Cartesian → Pixel）

设 viewRect = `(vl, vt, vw, vh)`，plotArea = `(pl, pt, pw, ph)`（像素，**y 向下**）。分两步：

1. **View → ViewNorm**（归一化到 [0,1]²）：
   ```
   nx = (cx − vl) / vw
   ny = (cy − vt) / vh
   ```
2. **ViewNorm → Pixel**（线性 + **y 翻转**：View 的 y 向上，屏幕 y 向下）：
   ```
   px = pl + nx · pw
   py = pb − ny · ph        （pb = pt + ph = plotArea.bottom()）
   ```

合成（代码即此式，`src/core/QChartCamera.cpp` `QChartCamera2D::cartesianToPixel`）：
```
px = pl + pw · (cx − vl) / vw
py = (pt + ph) − ph · (cy − vt) / vh
```
即：**x 同向线性插值、y 反向线性插值**。等价矩阵（2×3 仿射）：
```
[px]   [ pw/vw       0      pl − (pw/vw)·vl ] [cx]
[py] = [   0     −ph/vh   (pt+ph) + (ph/vh)·vt ] [cy]
[ 1]   [   0        0              1         ] [ 1]
```

### 1.2 反向（Pixel → View Cartesian，交互链）

逆线性（`pixelToCartesian`）：
```
nx = (px − pl) / pw
ny = (pb − py) / ph
cx = vl + nx · vw
cy = vt + ny · vh
```
与正向互为精确逆（同一仿射变换求逆），无信息损失（舍入除外）。

### 1.3 fit 几何（映射前的 viewRect 修正）

`fitViewRectToPlotArea(plotArea, strategy)` 只改 viewRect、不反算 dataBounds（`src/core/QChartCamera.cpp`）：

| FitStrategy | 语义 | 操作 |
|---|---|---|
| KeepWidth | 锁 dim0 宽度 | `newH = w / targetAspect`，高度对称扩张 |
| KeepHeight | 锁 dim1 高度 | `newW = h · targetAspect`，宽度对称扩张 |
| KeepCenter | 初始化/布局 | targetAspect > viewAspect → 扩宽，否则扩高（数据完整） |

Fit/Crop 决定"扩张较小维（数据完整）"还是"收缩较大维（裁掉超出）"；Fixed 强制 `fixedAspectRatio()` 忽略 plotArea。返回 true 表示 viewRect 实际被修改（调用方据此决定是否重算 dataBounds）。

## 2. 数学推导：viewCube → 相机派生（3D）

### 2.1 主状态（R5/D21，`include/core/QChartCamera3D.h`）

- `viewCube = {min, max}`（World 轴对齐盒）；`center = (min+max)/2`；`size = max−min`。
- `orientation = (yaw, pitch)`（默认 45°, 30°）；`fovY`（默认 45°）。
- 派生公式（`src/core/QChartCamera3D.cpp`）：

```
radius   = |size| / 2                                # 半对角线（保守拟合）
d        = radius / tan(fovY / 2)                    # 相机到盒中心距离（保守拟合，头注释备精确拟合）
lookAt   = viewCubeCenter()
frame():  yaw 绕世界 up(0,1,0) 旋转 forward₀=(0,0,−1) → f
          right = normalize(f × up)（退化兜底 |r|²<1e-12 → (1,0,0)）
          pitch 绕 right 旋转 f、up₀ → forward f、up u（均 normalized）
position = lookAt − forward · d
near     = max(0.01, d − 1.5·radius)
far      = d + 1.5·radius
viewMatrix         = QMatrix4x4::lookAt(position, lookAt, up)
projectionMatrix   = perspective(fovY, aspect, near, far)     # 透视
                   = orthographic(±size/2 的 x/y，near, far)  # 正交：viewCube 即投影盒
viewProjectionMatrix = projectionMatrix(aspect) · viewMatrix  # World → Clip
```

### 2.2 2D 是 3D 的退化特例（D-3D-2 硬验收）

正交模式 + 恒等映射（`QChartCartesianProjection3D`，`isIdentityMapping()==true`）下：
- 正交投影盒 = viewCube（R5：删除独立 orthographicBox），盒半尺寸 = `size/2`；
- 正交 `orthoMatrix` 等价于 2D 的 ViewNorm 线性映射（无透视除法、无深度压缩）；
- 恒等映射下 `toWorld ≡ fromWorld ≡ 恒等`，Numeric ≡ World。

于是"3D 正交俯视 ≡ 2D cartesianToPixel"直接成立——单测用像素断言锁死（`TestQChartCamera3D` 对照 `QChartCamera2D::cartesianToPixel`，见 §5）。

## 3. 源码位置

| 符号 | 文件 |
|---|---|
| `QChartCamera2D::cartesianToPixel` / `pixelToCartesian`（静态 + 实例版） | src/core/QChartCamera.cpp |
| `QChartCamera2D::fitViewRectToPlotArea`（Fit/Crop/Fixed + 三策略） | src/core/QChartCamera.cpp |
| `QChartCamera3D::frame` / `radius` / `distance` / `position` / `nearPlane` / `farPlane` | src/core/QChartCamera3D.cpp |
| `QChartCamera3D::projectionMatrix` / `viewProjectionMatrix` / `project` | src/core/QChartCamera3D.cpp |
| `QChartMath::perspectiveMatrix` / `orthographicMatrix` / `clipToScreen` / `viewDepth` | include/utils/QChartMath.h |

## 4. 边界与陷阱

1. **y 翻转一致性**：`cartesianToPixel` 的 y 翻转（`plotArea.bottom() − ny·h`）必须与 3D `clipToScreen`（NDC y 向上 → 屏幕 y 向下）一致——单测有对照断言（test_qchartmath 与 test_qchartcamera 双向锁定）。
2. **透视 w≤0**：投影点位于相机后方时 `clip.w ≤ 0`，`clipToScreen` 返回 NaN 哨兵，调用方（emitLine/collectPrimitives）跳过该点/段；绝不能把负 w 当正常点。
3. **pitch clamp ±89°**：`setPitch` 夹取防万向锁（退化兜底 `right.lengthSquared()<1e-12` 理论不可达但保留）。
4. **viewCube 零尺寸**：`orbit` 对零尺寸盒 no-op（防除零）；`dolly` factor<1 = 盒缩小 = 内容放大（与 2D zoom 同构）。
5. **Fixed 模式**：忽略 plotArea 长宽比，强制 `fixedAspectRatio()`——映射可能裁切，文档注明。
6. **fit 返回语义**：fit 修改了 viewRect 才返回 true，调用方才重算 dataBounds（避免无效重算风暴）。

## 5. 单测对照

| 锚点 | 断言 |
|---|---|
| test_qchartcamera | `cartesianToPixel/pixelToCartesian` 往返（含 y 翻转、边界像素）、fit 三策略几何、center/zoom 属性语义 |
| test_qchartcamera3d | viewCube 状态→派生（position/lookAt/up/near/far）、orbit 不移动 viewCube、pitch clamp、**正交俯视 ≡ 2D cartesianToPixel**（D-3D-2） |
| test_qchartmath | `clipToScreen` y 翻转与 2D 一致、`viewDepth`（−viewZ 越大越远）方向、透视/正交矩阵性质、`projectBatch` 与逐点投影一致 |

> 回归口径：ctest 180 PASS + 2 SKIP 全绿；修改本推导涉及的任一公式必须重跑三类测试（任一断言失败即锁死回归）。
