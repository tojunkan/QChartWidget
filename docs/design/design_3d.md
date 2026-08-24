# design_3d.md —— Phase 2 3D 数学先行 设计文档

> **读者**：engineer（照此实现）、reviewer（照此审查）、captain（排期）。
> **依据**：用户拍板的 D-3D-1 ~ D-3D-14（见 t2 任务描述）+ 两个确认点（Projection3D 家族按 D-3D-5 执行；QDataPoint 保持 QVariant、数值预转换缓存记入 Phase 3 性能项）。
> **范围**：只做数学库 + 3D 相机 + 3D 系列 + 线框渲染 + demo；不碰 GPU（D-3D-13）。
> **红线遵守**：本文档为新增文件，不改任何代码/CMake/既有文档。
> **给 engineer 的约定**：所有 API 签名即最终形态；标 ★ 的为本设计定案（决策未拍死、由本设计补定）；标 ⚠ 的为需要注意的坑。
>
> **修订记录（实现期间，队长统一安排回写）**：
> - R1（闭包签名定案）：`collectPrimitives`/`draw` 输入改 Data(QDataPoint3D) 的全链闭包 `ProjectFn3D`（Layer3D 组装：toNumeric×3→toWorld→project；系列只存 Data、零耦合）；QChartPrimitive 删 `worldAnchor` 加 `dataIndex`；线段深度=端点 depth 均值；dataIndex=线段起点索引；曲面 worldCache 由 Layer3D 直算填充。
> - R2（排序方向修正）：depth=−viewZ **越大越远**；painter's algorithm 按 depth **降序**（远→近）绘制（§3/§7.3/§7.4/§11 同根因一并修）。
> - R3（Phase 2 渲染边界）：渲染正确性保证限于数值型 Data（闭包内 QVariant→qreal 恒等）；非数值 Axis 渲染转换记入 Phase 3，与「数值预转换缓存」性能项合并。
> - R4（补充 API）：`QChartCamera3D::orthographicBox()/setOrthographicBox()`（正交模式投影盒，§2.3 硬验收「正交盒=viewRect」工程必需；非 Q_PROPERTY）。**R5 起废弃**（R5 删除独立 orthographicBox 状态）。
> - R5（viewCube 主状态相机模型，用户拍板）：QChartCamera3D 状态 = viewCube（World 空间轴对齐盒，2D viewRect 的 3D 对标物，与相机无关）+ orientation（yaw/pitch 绕盒中心）+ fovY（固定参数默认 45°）；position/lookAt/up/fovY/near/far **派生只读**（setter 移除）；Q_PROPERTY 迁移为 viewCube center/size + yaw/pitch + fovY；**删除 orthographicBox 独立状态**（正交模式 viewCube 即投影盒，D-3D-2 硬验收直接成立，§2.3）；dolly=缩放 viewCube（2D zoom 同构）、pan=平移 viewCube、orbit=绕盒中心旋转朝向（D-3D-3 的 Q_PROPERTY 条款随此修订）。§2.3/§4.2/§8.3/§10/§11.1 同步。
> - R6（交互与采样定案，用户拍板）：①**平移不做鼠标手势**（三方向手势语义未定）——viewCube 平移只经 API（panViewCube/setViewCubeCenter，代码/动画驱动）；②viewCube→dataBounds 反算用 **5×5×5=125 点网格采样**（通用坐标系极值不在角上）；③**笛卡尔快速通道** `isIdentityMapping()`（Cartesian3D=true → 反算免采样、图元免 toWorld）；④拖拽=转相机角度（orientation），viewCube 不动（§8.3 硬约束）。

---

## 1. 决策清单确认（D-3D-1 ~ D-3D-14）

| # | 决策 | 本设计落点 |
|---|---|---|
| D-3D-1 | 数学层用 Qt QVector3D/QMatrix4x4（弃用的是 2D QMatrix），新增轻量 QChartMath.h | §3 |
| D-3D-2 | 3D 链路 World→Camera→Clip→NDC→Pixel；2D 五空间链路一行不动；单测锁退化一致性 | §2 |
| D-3D-3 | 相机三件套：基类 QChartCamera + QChartCamera2D（现有实现搬入）+ QChartCamera3D；**R5 修订：QChartCamera3D 状态=viewCube(World 盒)+orientation(yaw/pitch)+fovY，position/lookAt/up/fovY/near/far 派生只读，Q_PROPERTY=viewCube center/size+yaw/pitch+fovY**；动画分家 | §4 |
| D-3D-4 | 交互归 Widget 事件层 → 调 Camera 几何运算；Camera 不碰事件 | §8 |
| D-3D-5 | Projection3D 家族（基类 + 4 子类，与 2D 家族同构）；奇点 NaN 策略延续；2D 投影家族零改动 | §5 |
| D-3D-6 | QChartSeries3D : QChartSeries（Data=QVariant 三元组）+ 散点/线/曲面三子类；draw 注入 projectFn 延续零耦合；2D QXYSeries 不动 | §6 |
| D-3D-7 | 双 Widget 联动：★定案「信号互发 (u,v)」（不共享 Series），论证见 §9 | §9 |
| D-3D-8 | Widget 形态：★定案「QChartWidget3D : QChartWidget 子类」，论证见 §8 | §8 |
| D-3D-9 | Renderer 3D 子路径：QChartScene 增 3D 段；图元列表 + 全局深度排序（远→近）；2D overlay 后画 | §7 |
| D-3D-10 | Phase 3 预留：viewProjectionMatrix + Clip→NDC→Screen 纯函数 + 批量投影入口 + 图元列表=命令缓冲雏形；性能项：数值预转换缓存；std::variant 不采纳 | §3/§7/§12 |
| D-3D-11 | demo 节奏 散点→线→曲面；argv 增入口；交互=左键 orbit + 滚轮 dolly（平移暂不做鼠标手势，仅 API，用户定案） | §10 |
| D-3D-12 | 新增 4 测试类；旧 69 例零回归；同名 ctest 全量跑 | §11 |
| D-3D-13 | 范围边界：不做 3D 轴刻度/射线拾取/hover/光照；可选辅助网格地板 | §7/§13 |
| D-3D-14 | QChart 前缀命名；QChartLineSeries3D（不用 Curve）；2D 相机改名 QChartCamera2D（机械改名） | §4/§6 |

---

## 2. 3D 链路与 World 定义（D-3D-2）

### 2.1 3D 渲染链路（新）

```
Data ──[Axis::toNumeric]──► Numeric ──[Projection3D::toWorld]──► World(x,y,z)
     ──[Camera::viewProjectionMatrix]──► Clip ──[÷w]──► NDC ──[视口]──► Pixel
```

- **World = 3D 场景空间**：物体所在的三维坐标空间（对应 2D 的 View Cartesian，物理长度单位）。所有 3D 系列/图元/相机 fit 都在 World 空间表达。
- **Numeric = 每维经 Axis::toNumeric 后的 qreal**（跨类型统一：QValueAxis 恒等、QDateTimeAxis epoch 等）。3D 下维度数 = 3（dim0/dim1/dim2，命名沿用 axisX/axisY/axisZ 惯例 = 参数曲面的 u/v/w）。
- **Clip/NDC/Screen**：Clip 为齐次坐标（QVector4D，w 承载透视除法）；NDC ∈ [-1,1]³；Screen 为 plotArea 像素坐标。
- **NaN/Inf 策略延续 2D**：链路任意环节产出 NaN/Inf → 该点/线段跳过不画（统一上游保护，不做 clamp/修正）。

### 2.2 五空间对照（2D 现状 vs 3D 新增）

| 环节 | 2D（现状，一行不动） | 3D（新增） |
|---|---|---|
| Data | QVariant（QDataPoint） | QVariant 三元组（QDataPoint3D，§6.1） |
| Numeric | qreal ×2（Axis::toNumeric） | qreal ×3（Axis::toNumeric） |
| 场景空间 | View Cartesian（QPointF/QRectF） | World（QVector3D） |
| 归一化 | ViewNorm [0,1]²（线性内联） | Clip→NDC（矩阵 + ÷w，QChartMath 纯函数） |
| 像素 | Pixel（QPointF） | Pixel（QPointF + 深度） |

### 2.3 退化一致性（硬验收，单测锁死，R5 新形态）

**3D 正交俯视相机 ≡ 2D cartesianToPixel（viewCube 主状态模型下直接成立）**：`QChartCamera3D` 配置——Orthographic 模式 + viewCube = {x∈viewRect 范围, y∈viewRect 范围, z 覆盖数据平面}（R5：**正交模式 viewCube 即投影盒**，无独立 orthographicBox）+ orientation=俯视（forward=(0,0,−1)）→ 对任意 (x,y,0) 的投影结果必须等于 `QChartCamera2D::cartesianToPixel(viewRect, plotArea, x, y)`（含 y 轴翻转约定，见 §3.2）。

**论证（R5）**：正交模式下 `viewProjectionMatrix = ortho(viewCube) · lookAt(...)`，World→Clip→NDC→Screen 就是 **viewCube→plotArea 的线性映射**（z 仅决定深度、不影响屏幕 xy），与 2D 的 viewRect→plotArea 线性映射同构——因此 ≡ cartesianToPixel **直接成立**，无需任何特判。这是 D-3D-2 的验收单测（test_qchartcamera3d，§11.1）。

---

## 3. 3D 数学层 QChartMath.h（D-3D-1）

**形态**：header-only（`#pragma once` + inline），放工作区根目录。**不新增 .cpp → CMake QCHART_SOURCES 零改动**；不依赖新 Qt 模块（QVector3D/QMatrix4x4 属 QtGui，库已链接）。

```cpp
// QChartMath.h —— 3D 数学纯函数（inline，无 Q_OBJECT）
#pragma once
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QRectF>
#include <QPointF>

namespace QChartMath {

    // ===== Clip → NDC → Screen（显式拆分，可测可复用；Phase 3 GL 同用）=====
    /// Clip → NDC：÷w。w<=0（相机背后/近平面外）→ 返回 NaN 哨兵（调用方跳过）
    inline QVector3D clipToNdc(const QVector4D& clip);
    /// NDC → Screen：x: left+(ndc.x+1)/2*w；y: bottom-(ndc.y+1)/2*h（y 翻转，与 2D 一致）
    inline QPointF ndcToScreen(const QVector3D& ndc, const QRectF& plotArea);
    /// Clip → Screen 组合（含 w<=0 → NaN 检查）
    inline QPointF clipToScreen(const QVector4D& clip, const QRectF& plotArea);

    // ===== 矩阵构造辅助（frustum 参数）=====
    /// 透视：fovY 度、aspect、near/far（对 QMatrix4x4::perspective 的封装，集中约定）
    inline QMatrix4x4 perspectiveMatrix(qreal fovYDeg, qreal aspect,
                                        qreal nearP, qreal farP);
    /// 正交：视口盒 + near/far（对 QMatrix4x4::ortho 的封装）
    inline QMatrix4x4 orthographicMatrix(qreal left, qreal right,
                                         qreal bottom, qreal top,
                                         qreal nearP, qreal farP);

    // ===== 深度辅助 =====
    /// 视图空间深度 = -viewZ（相机前方为正，越大越远；painter 排序键）
    inline qreal viewDepth(const QMatrix4x4& viewMatrix, const QVector3D& worldPoint);

    // ===== 批量投影（Phase 3 预留入口，D-3D-10；Phase 2 实现并单测）=====
    /// World 批量 → 屏幕点数组 + 深度数组（逐点调用 clipToScreen/viewDepth；
    /// Phase 3 换 GPU 批量/预转换时签名不变）
    inline void projectBatch(const QMatrix4x4& viewProj, const QMatrix4x4& view,
                             const QRectF& plotArea,
                             const QVector<QVector3D>& world,
                             QVector<QPointF>* outScreen, QVector<qreal>* outDepth);
}
```

⚠ **实现细节约定**：
- `clipToNdc`：`w <= 0` 返回 `QVector3D(qQNaN(), qQNaN(), qQNaN())`（不画）。
- `ndcToScreen` 的 y 翻转必须与 2D `cartesianToPixel` 一致（View 上→像素下），否则 §2.3 一致性单测不过。
- `projectBatch` 中 depth 用 `viewDepth(view, worldPoint)` 计算（排序键），与屏幕点数组对齐。

---

## 4. 相机家族（D-3D-3 / D-3D-14）

### 4.1 文件与类结构

| 文件 | 内容 |
|---|---|
| `QChartCamera.h` / `QChartCamera.cpp` | **保留文件名**：基类 `QChartCamera` + `QChartCamera2D`（现有实现整体搬入）+ 文件级枚举 `ViewRectFitMode`（不动）。include 关系零变动 |
| `QChartCamera3D.h` / `QChartCamera3D.cpp` | **新增**：3D 相机 |

```cpp
// QChartCamera.h —— 基类 + 2D 相机
enum class ViewRectFitMode { Stretch, Fit, Crop, Fixed };   // 文件级，不动

/// 相机基类：共同信号（2D/3D 视图状态变化都发 viewChanged）
class QChartCamera : public QObject {
    Q_OBJECT
public:
    explicit QChartCamera(QObject* parent = nullptr);
    ~QChartCamera() override;
signals:
    void viewChanged();
};

/// 2D 相机 = 现有 QChartCamera 整体搬入（行为零变化）：
/// viewRect + fit 策略 + center/zoom 属性 + cartesianToPixel/pixelToCartesian + pan/zoom 几何
class QChartCamera2D : public QChartCamera {
    Q_OBJECT
    Q_PROPERTY(QRectF viewRect READ viewRect WRITE setViewRect NOTIFY viewChanged)
    Q_PROPERTY(QPointF center READ center WRITE setCenter NOTIFY viewChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewChanged)
public:
    explicit QChartCamera2D(QObject* parent = nullptr);
    // —— 以下成员与现有 QChartCamera 逐字相同（搬入，不改行为）——
    QRectF viewRect() const;  void setViewRect(const QRectF& r);
    QPointF center() const;   void setCenter(const QPointF& c);
    qreal zoom() const;       void setZoom(qreal z);
    void panViewCartesian(qreal dx, qreal dy);
    void zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY);
    ViewRectFitMode fitMode() const;  void setFitMode(ViewRectFitMode mode);
    qreal fixedAspectRatio() const;   void setFixedAspectRatio(qreal ratio);
    enum class FitStrategy { KeepWidth, KeepHeight, KeepCenter };
    bool fitViewRectToPlotArea(const QRectF& plotArea, FitStrategy strategy);
    static QPointF cartesianToPixel(const QRectF& viewRect, const QRectF& plotArea,
                                    qreal cx, qreal cy);
    static QPointF pixelToCartesian(const QRectF& viewRect, const QRectF& plotArea,
                                    const QPointF& pixel);
    QPointF cartesianToPixel(const QRectF& plotArea, qreal cx, qreal cy) const;
    QPointF pixelToCartesian(const QRectF& plotArea, const QPointF& pixel) const;
};
```

⚠ **FitStrategy 随类改名**：`QChartCamera::FitStrategy` → `QChartCamera2D::FitStrategy`，引用点见 §4.4 改名映射表。

### 4.2 QChartCamera3D（viewCube 主状态，R5 用户拍板）

```cpp
// QChartCamera3D.h —— viewCube 主状态相机（R5，用户拍板；D-3D-3 的 Q_PROPERTY 条款随此修订）
// 状态：viewCube（World 空间轴对齐盒 {min,max}，2D viewRect 的 3D 对标物，与相机无关）
//      + orientation（yaw/pitch，绕盒中心）+ fovY（固定用户参数，默认 45°）。
// 派生（相机 = 纯映射器）：lookAt=盒中心；d=radius/tan(fovY/2)（radius=半对角线，保守拟合）；
//      forward/up = R(yaw,pitch)·(0,0,−1)/(0,1,0)；position = lookAt − forward·d；
//      near = max(0.01, d − 1.5·radius)、far = d + 1.5·radius；
//      viewProjectionMatrix = perspective(fovY,aspect,near,far) · lookAt(position,lookAt,up)。
class QChartCamera3D : public QChartCamera {
    Q_OBJECT
    Q_PROPERTY(QVector3D viewCubeCenter READ viewCubeCenter WRITE setViewCubeCenter NOTIFY viewChanged)
    Q_PROPERTY(QVector3D viewCubeSize READ viewCubeSize WRITE setViewCubeSize NOTIFY viewChanged)
    Q_PROPERTY(qreal yaw READ yaw WRITE setYaw NOTIFY viewChanged)
    Q_PROPERTY(qreal pitch READ pitch WRITE setPitch NOTIFY viewChanged)
    Q_PROPERTY(qreal fovY READ fovY WRITE setFovY NOTIFY viewChanged)
public:
    explicit QChartCamera3D(QObject* parent = nullptr);

    // ===== 主状态：viewCube（World 轴对齐盒；默认 {0,0,0}-{10,10,10}）=====
    QChartWorldBox viewCube() const;         void setViewCube(const QChartWorldBox& box);
    QVector3D viewCubeCenter() const;        void setViewCubeCenter(const QVector3D& c);  // 平移（pan）
    QVector3D viewCubeSize() const;          void setViewCubeSize(const QVector3D& s);    // 缩放（dolly）

    // ===== 主状态：orientation（绕盒中心；默认 yaw 45°/pitch 30° 3/4 视角）=====
    qreal yaw() const;   void setYaw(qreal deg);    // 绕世界 up 轴
    qreal pitch() const; void setPitch(qreal deg);  // 绕右轴；clamp ±89°（防万向锁）

    // ===== 主状态：镜头参数 =====
    qreal fovY() const;  void setFovY(qreal deg);   // (1, 179]，默认 45°

    // ===== 派生（只读；setter 移除，R5）=====
    QVector3D position() const;   // = lookAt − forward·d
    QVector3D lookAt() const;     // = viewCube 中心 (min+max)/2
    QVector3D up() const;         // = R(yaw,pitch)·(0,1,0)
    qreal nearPlane() const;      // = max(0.01, d − 1.5·radius)
    qreal farPlane() const;       // = d + 1.5·radius

    // ===== 投影模式 =====
    enum class ProjectionMode { Perspective, Orthographic };
    ProjectionMode projectionMode() const;  void setProjectionMode(ProjectionMode m);
    // ⚠ R5：删除 orthographicBox 独立状态——正交模式 viewCube 即投影盒（D-3D-2 直接成立，§2.3）

    // ===== 矩阵（纯映射；Phase 3 预留：直接产出合并矩阵，D-3D-10）=====
    QMatrix4x4 viewMatrix() const;                    // QMatrix4x4::lookAt(position, lookAt, up)
    QMatrix4x4 projectionMatrix(qreal aspect) const;  // 透视 perspective(fovY,aspect,near,far) / 正交 ortho(viewCube)
    QMatrix4x4 viewProjectionMatrix(qreal aspect) const;  // World→Clip 合并

    // ===== 交互几何运算（Widget 事件层调用；操作 viewCube 状态；Camera 不碰事件，D-3D-4）=====
    /// orbit：绕盒中心旋转 orientation（yaw 绕世界 up、pitch 绕右轴；pitch clamp ±89°；盒不动）
    void orbit(qreal deltaYawDeg, qreal deltaPitchDeg);
    /// dolly：缩放 viewCube（factor<1 = 盒缩小 = 内容放大；距离随盒尺寸重派生 → 内容 zoom，2D zoom 同构）
    void dolly(qreal factor);
    /// pan：平移 viewCube（dx/dy World 单位；lookAt/position 跟随）
    void panViewCube(qreal dxWorld, qreal dyWorld);

    // ===== fit：初始取景框（A3 链终点）=====
    /// 设置 viewCube = 目标盒（中心=盒中心），orientation/fovY 保持
    void setViewCubeToFit(const QChartWorldBox& box);

    // ===== 投影（供 Layer3D 组装闭包 / Renderer）=====
    QChartProjectedPoint project(const QVector3D& world, const QRectF& plotArea) const;
    //    = viewProjectionMatrix(aspect)*world → clipToScreen + viewDepth

private:
    QChartWorldBox m_viewCube{ QVector3D(0,0,0), QVector3D(10,10,10) };
    qreal m_yaw = 45.0, m_pitch = 30.0;
    qreal m_fovY = 45.0;
    ProjectionMode m_projectionMode = ProjectionMode::Perspective;
};
```

⚠ **距离派生注释（保守拟合定案 + 精确拟合备将来）**：
```cpp
// d = radius / tan(fovY/2)，radius = 半对角线 = |viewCubeSize| / 2   —— 保守拟合（定案）
// 精确拟合（备将来实现，当前不做）：
//   d = max(hx / tan(fovX/2), hy / tan(fovY/2)) − hz
//   其中 hx/hy/hz = 盒半尺寸（x/y/z），fovX = fovY·aspect；考虑盒最近点（−hz 面）到相机距离，
//   使盒恰好填满视口且最近角点贴近近平面；保守拟合已保证盒整体在视锥内，工程够用。
```

配套小结构（定义在 QChartCamera3D.h）：

```cpp
/// World 包围盒（axis-aligned，供 fit 与 batch 用）
struct QChartWorldBox { QVector3D min; QVector3D max; };

/// 投影结果：屏幕点 + 深度（排序键）；w<=0 时 screen 为 NaN
struct QChartProjectedPoint { QPointF screen; qreal depth; };
```

⚠ **orbit 几何（R5：操作 orientation，viewCube 不动——用户定案硬约束）**：yaw 绕世界 up 轴、pitch 绕「右向量 = normalize(cross(forward, up))」旋转 **orientation**（forward/up = R(yaw,pitch)·(0,0,−1)/(0,1,0)）；**viewCube 的 World 位置/大小保持不动**（用户拖拽反映在相机角度，不是挪动盒）；`pitch ∈ [-89°, +89°]`；viewCube 零尺寸时 orbit/dolly 为 no-op（防除零）。**无万向锁** = pitch clamp + 正交化（每步重算 forward/right/up）。

⚠ **动画（D-3D-3）**：position/lookAt 是 QVector3D 属性、fovY 是 qreal 属性，均可被 QPropertyAnimation 驱动（NOTIFY→viewChanged→Widget 重绘）。QVector3D 若 Qt 未内建 QVariantAnimation 插值器，在库初始化处 `qRegisterAnimationInterpolator<QVector3D>(...)` 一行补齐（线性插值即可）。**QViewRectAnimation 留在 2D 不动**；3D 相机动画一律 QPropertyAnimation（D3 精神）。

### 4.3 2D 相机的机械改名（D-3D-14）

`QChartCamera`（2D 类）→ `QChartCamera2D`，基类 `QChartCamera` 只留 QObject + viewChanged。**行为零变化**：类体原样搬入，仅类名与 FitStrategy 归属变化。现有 9 例相机测试仅类名替换（§11）。

### 4.4 改名映射表（完整引用点清单，engineer 逐条执行）

| 文件 | 位置 | 现状 | 改为 |
|---|---|---|---|
| `QChartCamera.h` | 全文 | `class QChartCamera`（2D 实现） | 基类 `QChartCamera` + `class QChartCamera2D`（原类体） |
| `QChartCamera.cpp` | 全文 | `QChartCamera::*` 实现 | `QChartCamera2D::*` 实现；新增基类构造/析构实现 |
| `QChartAxis.h` | L9 include | `#include "QChartCamera.h"` | 不变（文件名保留） |
| `QChartAxis.h` | L42、L53 | `QChartCamera::cartesianToPixel(...)` | `QChartCamera2D::cartesianToPixel(...)` |
| `QChartAxis.cpp` | L31、L43 | 同上 | 同上 |
| `QChartLayer.cpp` | L4 include | `#include "QChartCamera.h"` | 不变 |
| `QChartLayer.cpp` | L80 | `QChartCamera::cartesianToPixel(...)` | `QChartCamera2D::cartesianToPixel(...)` |
| `QChartWidget.h` | L14 include | `#include "QChartCamera.h"` | 不变 |
| `QChartWidget.h` | L151 | `using FitStrategy = QChartCamera::FitStrategy;` | `= QChartCamera2D::FitStrategy;` |
| `QChartWidget.h` | L164 | `std::unique_ptr<QChartCamera> m_camera;` | `std::unique_ptr<QChartCamera2D> m_camera;` |
| `QChartWidget.cpp` | L35 | `make_unique<QChartCamera>()` | `make_unique<QChartCamera2D>()` |
| `QChartWidget.cpp` | L199-204 等 | 经 `m_camera->` 调用 | 不变（指针类型变了，调用点不动） |
| `TestUnit/tests/test_qchartcamera.h/.cpp` | 全文 | `TestQChartCamera`、`QChartCamera`、`QChartCamera::FitStrategy` | `TestQChartCamera2D`、`QChartCamera2D`、`QChartCamera2D::FitStrategy`（9 例逻辑一字不改） |
| `TestUnit/main.cpp` | L32 | `qExec(new TestQChartCamera, ...)` | `qExec(new TestQChartCamera2D, ...)` |
| `CMakeLists.txt` | L29、L96、L116-117 | `QChartCamera.cpp` / `test_qchartcamera.*` 文件名 | 文件名不变 → **零改动**（仅新增文件另加，见 §14） |
| `Test/demos/demo_*.cpp`（8 个） | 各处 | `w->setViewRectFitMode(ViewRectFitMode::X)` | 不变（ViewRectFitMode 文件级枚举未动） |
| `Test/bench/bench_main.cpp` | — | 无直接 QChartCamera 引用 | 零改动 |
| `QChartProjection.h` | L3/L112 注释 | 提到 cartesianToPixel | 注释顺手同步（可选，非必须） |

---

## 5. Projection3D 家族（D-3D-5）

**形态**：与 2D 家族一致，**全部 header-only**（基类 + 子类均为 .h 内联实现）→ 新增文件不进 QCHART_SOURCES，CMake 零改动。

### 5.1 基类 QChartProjection3D

```cpp
// QChartProjection3D.h
class QChartProjection3D {
public:
    QChartProjection3D(QString name0 = "x", QString name1 = "y", QString name2 = "z");
    virtual ~QChartProjection3D() = default;
    QString dimensionName(int dim) const;   // 0/1/2

    // ===== Numeric → World（正向；n2 默认 0 → 2 参数曲面嵌入直接用）=====
    virtual QVector3D toWorld(qreal n0, qreal n1, qreal n2 = 0.0) const = 0;
    // ===== World → Numeric（反向；奇点 NaN 策略延续；未提供反向时为 nullptr 语义 → NaN + qWarning）=====
    virtual QVector3D fromWorld(const QVector3D& w) const = 0;

    // ===== 包围盒（Numeric 盒 → World 盒；采样法，网格默认 16×16×16）=====
    virtual QChartWorldBox computeWorldBounds(const QVector3D& dataMin,
                                              const QVector3D& dataMax) const;
    // ===== 初始值（Widget3D 首次 fit 用）=====
    virtual std::pair<QVector3D, QVector3D> defaultDataBounds() const;

    // ===== 数据曲线 → World 折线（NaN 断路径；返回子路径列表）=====
    /// dataCurve: t∈[0,1] → Numeric 三元组；每段子路径内连续，NaN 处断开
    QVector<QVector<QVector3D>>
    createPath3D(std::function<QVector3D(qreal t)> dataCurve, int segments = 64) const;
};
```

⚠ `computeWorldBounds` 采样法：对 dataMin/dataMax 张成的盒，沿三轴 16 段网格采样 `toWorld`，取有限点 min/max；全 NaN 回退 `dataMin/dataMax`（与 2D FunctionalProjection 的兜底一致）。

### 5.2 子类

| 子类 | toWorld(n0,n1,n2) | fromWorld(x,y,z) | 备注 |
|---|---|---|---|
| `QChartCartesianProjection3D` | 恒等 (n0,n1,n2) | 恒等 | 2D Cartesian 的 3D 对应 |
| `QChartCylindricalProjection3D` | dim0=r, dim1=θ(度), dim2=z → `(r·cosθ, r·sinθ, z)` | r=√(x²+y²)；θ=atan2(y,x)∈[0°,360°)，**r=0 → θ=NaN**；z=z | 名称 ("r","θ","z") |
| `QChartSphericalProjection3D` | dim0=r, dim1=θ(度,方位角), dim2=φ(度,仰角) → `(r·cosφ·cosθ, r·cosφ·sinθ, r·sinφ)` | r=√(x²+y²+z²)；**r=0 → θ/φ=NaN**；θ=atan2(y,x)；φ=asin(z/r) | 名称 ("r","θ","φ")，φ∈[-90°,90°] |
| `QChartFunctionalProjection3D` | 用户 lambda（2→3 嵌入与 3→3 变换统一） | 用户 lambda（可 nullptr） | 名称可配（默认 "u","v","w"） |

```cpp
// QChartFunctionalProjection3D.h —— 构造签名
QChartFunctionalProjection3D(
    std::function<QVector3D(qreal n0, qreal n1, qreal n2)> forward,      // 必传；2→3 嵌入时忽略 n2
    std::function<QVector3D(qreal x, qreal y, qreal z)> backward = nullptr, // nullptr → fromWorld 返回 NaN
    QVector3D defaultDataMin = {0,0,0}, QVector3D defaultDataMax = {1,1,1},
    std::function<QChartWorldBox(const QVector3D&, const QVector3D&)> boundsFn = nullptr, // nullptr→内部采样
    QString name0 = "u", QString name1 = "v", QString name2 = "w");
```

### 5.3 参数化示例（可直接落地，demo 与单测共用）

**莫比乌斯环**（D-3D-5 点名示例；u∈[0°,360°)，v∈[-0.5, 0.5]，环半径 R=1）：

```cpp
auto mobius = std::make_unique<QChartFunctionalProjection3D>(
    [](qreal u, qreal v, qreal /*n2*/) -> QVector3D {
        const qreal R = 1.0;
        const qreal uRad = qDegreesToRadians(u);
        const qreal half = uRad * 0.5;                       // 半扭转
        return QVector3D((R + v * qCos(half)) * qCos(uRad),
                         (R + v * qCos(half)) * qSin(uRad),
                         v * qSin(half));
    },
    nullptr,                                       // 反向不提供
    QVector3D(0.0, -0.5, 0.0), QVector3D(360.0, 0.5, 0.0),
    nullptr, "u", "v", "w");
```

**球面**（双 Widget 联动 demo 用；u∈[0°,360°)，v∈[-90°,90°] 仰角，半径 R=1）：

```cpp
auto sphere = std::make_unique<QChartFunctionalProjection3D>(
    [](qreal u, qreal v, qreal /*n2*/) -> QVector3D {
        const qreal R = 1.0;
        const qreal uRad = qDegreesToRadians(u);
        const qreal vRad = qDegreesToRadians(v);
        return QVector3D(R * qCos(vRad) * qCos(uRad),
                         R * qCos(vRad) * qSin(uRad),
                         R * qSin(vRad));
    },
    nullptr,
    QVector3D(0.0, -90.0, 0.0), QVector3D(360.0, 90.0, 0.0),
    nullptr, "u", "v", "w");
```

⚠ 两个示例都是 2→3 参数曲面嵌入：**数据是 (u,v) 网格，World 由投影在渲染时算出**——与 §6 曲面系列的两层数据组织衔接。

---

## 6. 3D 系列（D-3D-6 / D-3D-10）

### 6.1 QDataPoint3D（★定案：新建类，不扩展 QDataPoint）

**理由**：扩展 QDataPoint 加 z 会改变 2D 数据类的内存/语义并触碰 2D 代码；QDataPoint3D 与 QDataPoint 对称、零影响 2D。

```cpp
// QDataPoint3D.h —— Data 空间 3D 点（QVariant 三元组，任意 Axis 类型可用）
class QDataPoint3D {
public:
    QDataPoint3D(QVariant x = {}, QVariant y = {}, QVariant z = {});
    QVariant x() const;   QVariant y() const;   QVariant z() const;
    void setX(QVariant);  void setY(QVariant);  void setZ(QVariant);
private:
    QVariant m_x, m_y, m_z;
};
```

### 6.2 QChartSeries3D 基类（Data 层）

```cpp
// QChartSeries3D.h —— 3D 系列基类（: QChartSeries，白捡 name/visible/opacity/color/主题色/图例）
class QChartSeries3D : public QChartSeries {
    Q_OBJECT
public:
    explicit QChartSeries3D(const QString& name = {}, QObject* parent = nullptr);

    // ===== 数据（Data 空间：QVariant 三元组；QVector3D 仅渲染时经投影产生）=====
    int count() const override;
    const QVector<QDataPoint3D>& points() const;
    QDataPoint3D at(int i) const;
    void append(const QDataPoint3D& pt);
    void append(qreal x, qreal y, qreal z);            // 便捷（QValueAxis 场景）
    void append(QVariant x, QVariant y, QVariant z);
    void insert(int index, const QDataPoint3D& pt);
    void remove(int index);
    void replace(int index, const QDataPoint3D& pt);
    void clear();
    void setPoints(const QVector<QDataPoint3D>& pts);

    // ===== 图元收集（Renderer 3D 主路径：不排序、不直接绘制，D-3D-9）=====
    /// 全链闭包：QVariant×3 →(axisX/Y/Z::toNumeric)→ qreal×3 →(projection3D::toWorld)→
    /// QVector3D →(camera3D::project)→ QChartProjectedPoint{screen, depth}；由 Layer3D 组装，
    /// 系列只存 Data、零耦合（与 2D toPixel 同构）。
    using ProjectFn3D = std::function<QChartProjectedPoint(const QDataPoint3D&)>;
    virtual void collectPrimitives(const ProjectFn3D& projectFn,
                                   QVector<QChartPrimitive>& out) const = 0;

    // ===== 直接绘制入口（painter's algorithm 关闭/单系列调试；⚠ 名字隐藏基类 draw，见下）=====
    virtual void draw(QPainter* painter, const ProjectFn3D& projectFn,
                      const struct DrawContext3D* ctx = nullptr) const;

signals:
    void dataChanged();
protected:
    QVector<QDataPoint3D> m_points;
};
```

⚠ **draw 名字隐藏**：`QChartSeries3D::draw` 的闭包签名（ProjectFn3D：QDataPoint3D→QChartProjectedPoint）与 `QChartSeries::draw`（QVariant×2→QPointF）不同 → 这是**重载隐藏而非覆盖**。调用方一律持有 `QChartSeries3D*`（经 QChartLayer3D 遍历，§7.1），**绝不通过 `QChartSeries*` 多态调用 3D draw**。文档级红线，reviewer 审查点。

**两层数据组织（D-3D-6 明确）**：
- **Data 层**（存储）：`QVector<QDataPoint3D>`（QVariant 三元组）——任意 Axis 类型（u/v 可为 QDateTime 等）。
- **World 层**（渲染时产生）：`toNumeric ×3 → Projection3D::toWorld → QVector3D`（该链在 Layer3D 组装的全链闭包内完成，系列不持有 Axis/Projection/Camera 任何类型）。散点/曲线按点即时产生；曲面有连续缓存（§6.5）。**数值预转换缓存（append 时 Data→Numeric 一次性转换）不纳入 Phase 2，记入 Phase 3 性能项（D-3D-10）。**

### 6.3 QChartScatterSeries3D

```cpp
class QChartScatterSeries3D : public QChartSeries3D {
    Q_OBJECT
    Q_PROPERTY(qreal markerSize READ markerSize WRITE setMarkerSize NOTIFY markerSizeChanged)
public:
    explicit QChartScatterSeries3D(const QString& name = {}, QObject* parent = nullptr);
    qreal markerSize() const;            // 默认 4.0（px）
    void setMarkerSize(qreal s);
    void collectPrimitives(const ProjectFn3D& projectFn,
                           QVector<QChartPrimitive>& out) const override;  // 每点一个 Point 图元（NaN 跳过）
    void draw(QPainter*, const ProjectFn3D& projectFn,
              const DrawContext3D* = nullptr) const override;              // 无排序直绘
signals:
    void markerSizeChanged();
private:
    qreal m_markerSize = 4.0;
};
```

### 6.4 QChartLineSeries3D（用户点名，不用 Curve）

```cpp
class QChartLineSeries3D : public QChartSeries3D {
    Q_OBJECT
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    Q_PROPERTY(bool cullingEnabled READ isCullingEnabled WRITE setCullingEnabled NOTIFY cullingChanged)
public:
    explicit QChartLineSeries3D(const QString& name = {}, QObject* parent = nullptr);
    qreal lineWidth() const;  void setLineWidth(qreal w);      // 默认 2.0
    bool isCullingEnabled() const; void setCullingEnabled(bool v); // 默认 true；屏外线段跳过（沿用 2D QLineSeries 语义）
    void collectPrimitives(const ProjectFn3D& projectFn,
                           QVector<QChartPrimitive>& out) const override;
    //   相邻有效点成 LineSegment；任一端投影 NaN → 断段（延续 createPath 断路径语义）；
    //   线段深度 = 两端点 depth 均值；dataIndex = 线段起点索引
    void draw(QPainter*, const ProjectFn3D& projectFn,
              const DrawContext3D* = nullptr) const override;
signals:
    void lineWidthChanged();  void cullingChanged();
private:
    qreal m_lineWidth = 2.0;
    bool m_cullingEnabled = true;
};
```

### 6.5 QChartSurfaceSeries（曲面线框，行主序网格）

```cpp
class QChartSurfaceSeries : public QChartSeries3D {
    Q_OBJECT
public:
    explicit QChartSurfaceSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== Data 层：网格（行主序，rows×cols）=====
    void setGrid(int rows, int cols, const QVector<QDataPoint3D>& pts); // 大小须 = rows*cols
    int rows() const;  int cols() const;
    QDataPoint3D gridAt(int row, int col) const;
    /// 便捷：按参数域生成 (u,v) 格点（x=u, y=v, z 未用），u∈[u0,u1]、v∈[v0,v1]
    void setParametricGrid(int rows, int cols,
                           qreal u0, qreal u1, qreal v0, qreal v1);

    // ===== World 层缓存（渲染时由 Layer3D 直算填充：axis toNumeric + projection3D toWorld；连续内存，Phase 3 VBO 直接消费）=====
    const QVector<QVector3D>& worldCache() const;   // 行主序 rows*cols，仅渲染后有效
    QVector<QVector3D>& worldCache();               // Layer3D 填充入口（内部）

    void collectPrimitives(const ProjectFn3D& projectFn,
                           QVector<QChartPrimitive>& out) const override;
    //   线框图元：u 方向 rows×(cols-1) 条 + v 方向 cols×(rows-1) 条 LineSegment；
    //   任一端投影 NaN → 该段跳过；线段深度 = 端点 depth 均值；dataIndex = 线段起点索引
    void draw(QPainter*, const ProjectFn3D& projectFn,
              const DrawContext3D* = nullptr) const override;
signals:
    void gridChanged();
private:
    int m_rows = 0, m_cols = 0;
    QVector<QVector3D> m_worldCache;
};
```

---

## 7. 图层与渲染 3D 路径（D-3D-9）

### 7.1 QChartLayer3D（新，: QChartLayer）

```cpp
// QChartLayer3D.h —— 3D 图层：三轴 + 3D 系列 + 变换闭包组装 + 图元收集
class QChartLayer3D : public QChartLayer {
    Q_OBJECT
public:
    explicit QChartLayer3D(QObject* parent = nullptr);

    void setAxisZ(QChartAxis* a);   QChartAxis* axisZ() const;

    /// 3D 系列管理：**存入基类 m_series**（复用图例/主题/调色板/所有权/析构），
    /// 同时登记到 m_series3D 便于类型化遍历
    void addSeries3D(QChartSeries3D* s);
    void removeSeries3D(QChartSeries3D* s);
    QList<QChartSeries3D*> series3DList() const;

    // ===== 辅助网格地板（可选，D-3D-13；World 线段走同一 3D 路径）=====
    void setGridFloorVisible(bool v);  bool gridFloorVisible() const;
    void setGridFloorHalfSize(qreal half);  // 默认 = worldBounds 外接半径；y=0 平面

    // ===== 图元收集（Renderer 3D 路径调用；不排序、不绘制）=====
    /// 遍历 3D 系列 collectPrimitives + 网格地板；worldCache 在此填充（曲面）
    void collectPrimitives(const QChartCamera3D* cam, const QRectF& plotArea,
                           QVector<QChartPrimitive>& out) const;

protected:
    /// 组装 ProjectFn3D 全链闭包：QVariant×3 → toNumeric0/1/2（axisX/Y/Z 注入）→ qreal×3
    /// → toWorld（projection3D）→ QVector3D → project（camera3D）→ QChartProjectedPoint{screen, depth}
    QChartSeries3D::ProjectFn3D makeProjectFn(const QChartCamera3D* cam,
                                              const QRectF& plotArea) const;

    QChartAxis* m_axisZ = nullptr;
    QList<QChartSeries3D*> m_series3D;
    const QChartProjection3D* m_projection3D = nullptr;  // Widget3D 注入（非持有）
    bool m_gridFloorVisible = false;
    qreal m_gridFloorHalf = 0.0;   // 0 = 自动（worldBounds）
};
```

⚠ **三轴用途**：axisX/axisY/axisZ 只做 `toNumeric`（参数曲面 u/v/w 数值化），**不在 3D 场景绘制轴刻度**（D-3D-13）。2D 侧联动时 axisU/axisV 才用于绘制平面轴（§9）。

### 7.2 QChartScene 3D 段（QChartRenderer.h 扩展）

```cpp
// QChartRenderer.h —— 现有 QChartScene 追加 3D 段（2D 场景保持默认值，零行为变化）
struct QChartScene {
    // ……现有字段原样……
    // ===== 3D 段（新增）=====
    const QChartCamera3D* camera3D = nullptr;      // 非空 = 3D 场景（2D 场景保持 null）
    QList<QChartLayer3D*> layers3D;                // 3D 图层（camera3D 非空时有效）
    QChartWorldBox worldBounds;                    // 当前可见 World 盒（fit/网格地板用）
    bool is3D() const { return camera3D != nullptr; }
};
```

### 7.3 图元列表结构体（D-3D-9 / D-3D-10 ③）

```cpp
// QChartRenderer.h（或 QChartPrimitive.h，定案：放 QChartRenderer.h，与场景同契约）
struct QChartPrimitive {
    enum class Type { Point, LineSegment };
    Type type = Type::Point;
    QPointF a;                // 屏幕坐标：Point 位置 / LineSegment 起点
    QPointF b;                // 屏幕坐标：LineSegment 终点（Point 忽略）
    int dataIndex = -1;       // 系列图元 = 数据点索引（线段 = 起点索引）；网格地板 = -1
    qreal depth = 0.0;        // 排序键：-viewZ（depth 大 = 远、小 = 近；降序 = 远→近）
    qreal markerSize = 4.0;   // Point 标记半径（px）
    QColor color;             // 绘制色（收集时已按系列主题/override 展开）
    qreal penWidth = 1.0;     // 线宽（px）
};
```

**深度排序**：`std::sort(items.begin(), items.end(), [](a,b){ return a.depth > b.depth; })` → **降序 = 远→近**（depth=−viewZ：越大越远），近者后画覆盖远者（painter's algorithm）。排序粒度 = 图元（点/线段），Phase 2 规模（rows×cols 网格 ≈ 万级图元）开销可忽略。

### 7.4 QPainterChartRenderer 3D 子路径（2D 路径零改动）

```cpp
// QPainterChartRenderer.cpp —— drawForeground 开头分支：
if (scene.is3D()) { drawForeground3D(p, scene); return; }   // 2D 路径一行不动
```

`drawForeground3D` 流程：
1. `collect`: 遍历 `scene.layers3D` → `layer->collectPrimitives(scene.camera3D, scene.plotArea, out)`（深度已在收集时算好）。
2. `sort`: 按 `depth` **降序**（远→近，`a.depth > b.depth`）。
3. `draw`: 逐图元绘制——Point 用 `painter->drawEllipse(center, r, r)`（或 drawPoint + markerSize 画圆），LineSegment 用 `drawLine(a, b)`；pen = color + penWidth。
4. **2D overlay 后画**：`drawLegend`（图例）与既有 2D overlay 逻辑在 3D 图元之后执行，不参与深度（D-3D-9）。

⚠ **背景路径**：3D 模式下 `drawBackground` 只填背景色；网格/轴刻度绘制由 2D 的 `scene.axes`/layer 驱动——3D Widget 不向 scene.axes 添加轴（axisU/V/Z 只用于 toNumeric），故 2D 背景路径自然不画轴。**不做 2D/3D 混排**（一 Widget 一 projection，D-3D-9）。

---

## 8. Widget 形态定案（D-3D-8）

### 8.1 定案：QChartWidget3D : QChartWidget 子类

| 维度 | 单类双模式（备选） | **QChartWidget3D 子类（★定案）** |
|---|---|---|
| 交互手势 | 左键在 2D=pan、在 3D=orbit——同一 handler 里 mode 分支，易漏易错 | 子类重写 mouse/wheel handler，2D 类事件代码零触碰 |
| 2D API 语义 | viewRect/cartesianToPixel 在 3D 模式下语义漂移，文档负担重 | 2D API 语义不变；3D API 全部在子类 |
| 零回归（D6） | 2D 类体改大，69 用例回归风险高 | 基类仅两处最小改动（见 §8.2），其余全部新增文件 |
| 类规模 | QChartWidget 爆炸（交互+布局+主题+导出+3D） | 各司其职 |

**结论**：子类。3D 特定 API（camera3D/projection3D/worldToPixel/orbit…）与 3D 交互全部隔离在 `QChartWidget3D`，2D 类保持可证明的零回归。

### 8.2 基类 QChartWidget 的最小改动（仅此两处，均为行为保持）

1. `m_camera` 类型：`QChartCamera` → `QChartCamera2D`（§4.4 改名表，机械）。
2. 场景组装钩子虚化（供 3D 子类注入 3D 段）：
   - `paintEvent` 内 6 行场景组装（现 L372-381）抽成 `protected: virtual QChartScene buildScreenScene() const;`（默认 = 现逻辑）。
   - `buildExportScene` 从 private 改 `protected virtual`（3D 子类可注入 3D 段，导出 3D 场景低成本；验收不要求，见 §13）。

### 8.3 QChartWidget3D API

```cpp
// QChartWidget3D.h
class QChartWidget3D : public QChartWidget {
    Q_OBJECT
public:
    explicit QChartWidget3D(QWidget* parent = nullptr);

    // ===== 3D 相机（构造时内置默认 QChartCamera3D，可替换）=====
    QChartCamera3D* camera3D() const;                       // 非持有
    void setCamera3D(std::unique_ptr<QChartCamera3D> cam);

    // ===== 3D 投影（必须设置；setProjection3D 时自动按 defaultDataBounds fit 相机）=====
    void setProjection3D(std::unique_ptr<QChartProjection3D> proj);
    const QChartProjection3D* projection3D() const;

    // ===== 3D 图层 =====
    void addLayer3D(QChartLayer3D* g);   // 内部：基类 addLayer（复用图例/主题/调色板接线）+ 登记 layers3D
    QList<QChartLayer3D*> layers3D() const;

    // ===== 坐标 =====
    QPointF worldToPixel(const QVector3D& w) const;   // camera3D->project(...).screen
    // ===== fit（R5：A3 链终点 = setViewCubeToFit）=====
    void fitWorld();   // resolveDataBox（A3 链）→ projection3D->computeWorldBounds → camera3D->setViewCubeToFit

    // ===== 交互（重写；D-3D-4：手势 → viewCube 几何；R5）=====
    // ⚠ 交互语义硬约束（用户定案）：**用户拖拽改变的是相机角度（orientation），不是挪动 viewCube**
    //    —— orbit 只旋转朝向（yaw/pitch），viewCube 的 World 位置/大小保持不动。
    // ⚠ 平移暂不提供鼠标手势（用户定案：三方向的手势语义未定）——viewCube 平移只经 API
    //   （panViewCube / setViewCubeCenter，代码或动画驱动）；右键拖拽/pan 状态不实现。
    // 左键拖拽 = orbit（水平=deltaYaw，垂直=deltaPitch，灵敏度可调）→ camera3D->orbit（只转朝向）
    // 滚轮     = dolly（factor=exp(-delta*k)，同 2D 灵敏度约定）→ camera3D->dolly（缩放 viewCube）
protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    QChartScene buildScreenScene() const override;   // 填 camera3D/layers3D/worldBounds（2D 字段留默认）
    QChartScene buildExportScene(QChartExportScope, const QSize&, QSizeF&) const override;

    // ===== 双 Widget 联动信号（§9）=====
signals:
    void uvHovered(qreal u, qreal v);     // 悬停点 (u,v)（Numeric 空间；3D 侧为屏幕近邻命中）
    void uvSelected(qreal u, qreal v);    // 点击选中 (u,v)
    void uvHoveredEnd();

private:
    std::unique_ptr<QChartCamera3D> m_camera3D;
    std::unique_ptr<QChartProjection3D> m_projection3D;
    QList<QChartLayer3D*> m_layers3D;
    // 交互状态：m_orbitDrag/m_lastPos（平移无鼠标手势，R6）
};
```

⚠ 构造时先 `setProjection(默认 QCartesianProjection)` 再走基类 `addLayer` 接线（基类断言需要 m_projection 非空）；该 2D projection 仅用于满足基类流程，**渲染走 camera3D/layers3D 的 3D 段**。

⚠ **3D 悬停/命中（Phase 2 简化版）**：不做射线拾取（D-3D-13）。3D 侧悬停命中 = **屏幕空间近邻**：把系列已投影图元与鼠标像素比较，距离 < 阈值（如 8px）取最近图元 → 其 `dataIndex` → `at(dataIndex)` → 该点 Data (u,v)（线段取起点索引）。仅用于联动高亮，不弹 tooltip（3D tooltip 不在范围）。

---

## 9. 双 Widget 联动机制定案（D-3D-7，用户核心场景）

### 9.1 场景

左：`QChartWidget3D` 显示球面（axisU/axisV + FunctionalProjection3D 球面参数化 + Camera3D 透视）；右：`QChartWidget` 显示同一 (u,v) 域的平面（同一 axisU/axisV + QCartesianProjection）。两图一一对应：左图球面上悬停/点选 → 右图平面高亮同 (u,v)；右图悬停 → 左图球面高亮同 (u,v)。

### 9.2 定案：信号互发 (u,v)（不共享 Series）

**论证**：
1. **Series 单归属约束是硬约束**：`QChartLayer::addSeries` 强制 `setParent(this)`（QObject 单父）、Layer 析构 `qDeleteAll(m_series)`、`removeSeries` 即 delete。共享同一 QChartSeries 对象给两个 Widget：parent 只能归一方，另一方持有悬垂指针（析构/remove 即崩）；且两 Widget 的调色板/图例接线都会写同一 series 的 themeColor/状态，互相打架。
2. **让共享成立需改 2D 语义**：给 QChartLayer 加「非拥有模式」+ 改 addSeries 所有权 + 两 Widget 共用主题状态——触碰 2D Layer/Widget 既有行为，违背 D6 零回归。**不采纳**。
3. **信号互发零侵入**：
   - 两 Widget 各持**同构数据副本**（同一数据源生成：demo 里同一 `(u,v)` 格点 vector 分别 append 两份；或共享外部裸数据容器 `QVector<QDataPoint3D>`，非 QChartSeries）。
   - axisU/axisV 各自配置**相同 range** → 两侧 Numeric 空间 (u,v) 一致——这正是「共享同一组 Axis/Data 与 Numeric 空间」的实质（空间同构，而非对象共享）。
   - 联动信息 = **(u,v) 数值**（Numeric 空间），经信号单向传输：`uvHovered(u,v)` / `uvSelected(u,v)` / `uvHoveredEnd()`。
   - 3D→2D 方向：3D 侧屏幕近邻命中（§8.3）→ 数据点索引 → Data (u,v) → 发信号。
   - 2D→3D 方向：2D 侧 hover/点击 → `pixelToCartesian` → (u,v) → 发信号（2D Widget 侧用同样的三个信号，或复用 `QChartWidget::seriesHovered` 扩展——定案：2D 侧也新增同构信号，或由 demo 用现有 seriesHovered 映射；**实现细节留给 demo，API 统一用 (u,v) 信号**）。
4. **成本**：两份数据副本。Phase 2 曲面网格规模 rows×cols≈64×64，可忽略；若未来数据量巨大，Phase 3 再引入「共享裸数据源对象」（非 QChartSeries 的只读容器），不改变本机制。

### 9.3 高亮实现（Phase 2 简化）

- 每侧一个「高亮标记」：3D 侧用 `QChartScatterSeries3D`（单点、markerSize 8、醒目色，数据 = 当前 (u,v)）；2D 侧用 `QScatterSeries`（同 (u,v)）。
- 收到对方 `uvHovered(u,v)` → 更新标记系列数据 → `invalidateForeground()`。标记系列不参与联动互发（只收不发），避免回环。
- `uvHoveredEnd` → 隐藏标记。

---

## 10. demo 节奏（D-3D-11）

按用户节奏「散点→线→曲面，静态→动态」分 3 个 demo 文件（Test/demos/）：

| 文件 | 内容 | argv |
|---|---|---|
| `demo_scatter3d.cpp` | 3D 散点（球面随机采样点 + 柱面/球面投影切换演示；静态） | `scatter3d` |
| `demo_line3d.cpp` | 3D 参数曲线（螺旋线 u∈[0,720°]，经 CylindricalProjection3D；可加 QPropertyAnimation 驱动 viewCube 属性 center/size/yaw/pitch 做相机路径动画 → 动态，R5） | `line3d` |
| `demo_surface3d.cpp` | 参数曲面（球面/莫比乌斯环按键 'S'/'M' 切换）+ **双 Widget 联动**（左 3D 球面 ↔ 右 (u,v) 平面，悬停/点选互显，§9）+ 辅助网格地板 | `surface3d` |

**Test 侧改动**（不涉库）：
- `test.cpp` demos[] 表签名 `QChartWidget* (*build)()` 放宽为 `QWidget* (*build)()`（QChartWidget* 兼容，现有 build 函数返回值隐式升级，零改动）；argv 提示文案补 `scatter3d line3d surface3d`；无参 = 全部（自动含新 demo）。
- `demo_surface3d` 的 build 返回含 `QHBoxLayout` 双图容器 `QWidget*`（单窗口双图，联动可视）。
- **交互**：3D 侧左键 orbit + 滚轮 dolly（平移无鼠标手势，仅 API——用户定案 R6）；2D 侧沿用既有 pan/zoom。

---

## 11. 测试策略（D-3D-12）

### 11.1 新增 4 个测试类（tests/ 下 test_xxx.h/.cpp，main.cpp 加 4 行 qExec，CMakeLists 测试头手动 moc 列表 + sources 同步——实现阶段）

**TestQChartMath（test_qchartmath）**：
1. `clipToNdc_divideByW`：齐次 (x,y,z,w) → x/w 等；w=0/负 → NaN 哨兵
2. `ndcToScreen_viewport`：(±1,±1) 四角 → plotArea 四角（含 y 翻转）
3. `clipToScreen_roundtrip`：正交矩阵下 world→clip→ndc→screen 与手算一致
4. `perspectiveMatrix_properties`：near 平面 x/y 缩放 = 1/tan(fov/2)，far 比例正确
5. `orthographicMatrix_properties`：盒角点映射正确
6. `viewDepth_viewSpaceZ`：viewMatrix 下前方点 depth>0、越近越大
7. `projectBatch_alignment`：批量结果与逐点一致，两数组对齐
8. Projection3D 家族（并入本类，数学映射归属）：`cylindrical_roundtrip`（r=0 → θ NaN）、`spherical_roundtrip`、`cartesian3d_identity`、`functional3d_mobiusSamples`（采样点 |dist−R| ≤ 带宽容差）、`computeWorldBounds_sampling`（全 NaN 兜底）

**TestQChartCamera3D（test_qchartcamera3d）**（R5：重写为派生不变量断言——驱动 viewCube 主状态，验证派生映射）：
1. `derivedPosition_lookAt`：position = lookAt − forward·d、|position−lookAt| == d == radius/tan(fov/2)、lookAt == 盒中心
2. `orthographicTopDown_equals2D`（**D-3D-2 硬验收**）：正交模式 + viewCube=2D viewRect 范围 + 俯视 → ≡ `QChartCamera2D::cartesianToPixel`（§2.3 论证）
3. `orbit_geometry`：orbit 后 |position−lookAt| == d（绕盒距离不变）、lookAt == 盒中心（不变）、pitch clamp ±89°
4. `dolly_scale`：viewCube 缩放 f 倍 → 距离 d' == f·d（∝ 盒尺寸）、lookAt 不变、同 world 点屏幕坐标外扩（内容放大）
5. `pan_translates`：平移 viewCube → 盒中心/position 同位移、viewMatrix 平移项正确
6. `perspectiveVsOrthographic`：同 world 点两模式屏幕坐标差异符合预期
7. `degenerate_zeroSize`：viewCube 退化为零尺寸 → orbit/dolly no-op、无 NaN
8. `properties_animatable`：viewCubeCenter/viewCubeSize/yaw/pitch/fovY 五 Q_PROPERTY 存在、setter 发 viewChanged；QVector3D 插值器可用（QVariantAnimation 往返）

**TestQChartRenderer3D（test_qchartrenderer3d）**：
1. `line_collectPrimitives_count`：n 点 → n−1 线段；NaN 断段正确
2. `surface_wireframeCount`：rows×cols → rows·(cols−1) + cols·(rows−1) 线段
3. `depthSort_farToNear`：构造已知深度图元 → 按 depth 降序排序后近者在后（后画）
4. `render3d_offscreen_ok`：offscreen QImage 渲染不崩、非空白
5. `render3d_nearCoversFar`：两重叠线段（近红远蓝）→ 顶部像素为近者色
6. `scene_is3D_detection`：camera3D 非空 ↔ is3D()

**TestQChartSurface3D（test_qchartsurface3d）**：
1. `data_variantTriple`：QDataPoint3D append QVariant（qreal/QDateTime）→ count/at/replace 一致
2. `grid_layout`：setGrid 行主序存取、大小校验
3. `parametricGrid_lattice`：(u0,u1)×(v0,v1) 采样值正确
4. `worldCache_filled`：渲染后 worldCache 尺寸 = rows·cols、值 = toWorld(u,v)
5. `scatter_markerSize`：属性存在/生效
6. `line_breakOnNaN`：断段行为
7. `collect_nanSkip`：投影 NaN 图元被跳过

### 11.2 旧 69 例零回归保障（硬指标）

- 2D 库文件改动面 = §4.4 改名表（机械）+ §8.2 两处钩子（行为保持）→ 其余 2D 文件**零改动**。
- 旧测试：除 `test_qchartcamera.*` 仅类名替换（D-3D-3 特批）外，其余 68 例**一行不改**。
- 运行：同一 `QChartTests` target + 同一 ctest（`QT_QPA_PLATFORM=offscreen` 无头）全量跑；Linux 构建 0 error/0 warning。

---

## 12. Phase 3 GPU 预留清单（D-3D-10）

| # | 预留项 | Phase 2 落点 | Phase 3 消费 |
|---|---|---|---|
| 1 | `viewProjectionMatrix(aspect)`（World→Clip 合并矩阵） | QChartCamera3D，§4.2 | GL 顶点着色器直接上传同一矩阵 |
| 2 | Clip→NDC→Screen 纯函数显式拆分 | QChartMath，§3 | 着色器/回读路径复用，单测不变 |
| 3 | 批量投影入口 `projectBatch(...)`（屏幕点 + 深度数组） | QChartMath，§3（Phase 2 实现并单测） | GPU 批量/SIMD 替换 O(N) 逐点投影（基准已证瓶颈，ROADMAP §4） |
| 4 | 图元列表 QChartPrimitive = 命令缓冲雏形 | §7.3 | GL 后端直接消费（Point→GL_POINTS、LineSegment→GL_LINES），painter's algorithm 可整体替换为 z-buffer |
| 5 | 深度数组演进 z-buffer 降级路径 | 图元 depth 字段（§7.3） | 深度缓冲初版可先软件 z-test，再转硬件 |
| 6 | 数值预转换缓存（**性能项，记入 Phase 3**）：append 时 Data→Numeric 一次性转换缓存，绘制路径绕开 QVariant 解包 | 本 Phase **不做**（QDataPoint3D 保持 QVariant，D-3D-10 确认点） | 与批量投影配合，消除 O(N) 无条件 QVariant 转换 |
| 7 | **std::variant 明确不采纳**（理由写入文档）：Data 的 Axis 类型是开放集合（qreal/QDateTime/QString/自定义 Axis），std::variant 要求封闭类型集合、破坏 Data 开放扩展性；QVariant 是库既有选择，与 2D 一致 | — | 决策记录，防止 Phase 3 引入 |
| 8 | 屏幕→射线 `unproject`（拾取预留） | 签名后补（Phase 3 再定） | 3D 拾取/射线求交 |

---

## 13. 风险与遗留

### 13.1 风险与对策

| 风险 | 对策 |
|---|---|
| painter's algorithm 图元级排序在超大网格（256×256）下开销 | Phase 2 demo 用 64×64；排序 O(n log n)、图元数 ~2·rows·cols；bench 记录基线（QChartBench 可扩展 3D 场景，Phase 3 优化输入） |
| QChartCamera2D 改名波及 12+ 文件 | §4.4 全量引用点清单；机械替换 + 编译即验证（改名后任何遗漏 = 编译错误，无静默风险） |
| 3D draw 名字隐藏基类 draw | §6.2 ⚠ 红线：调用方只持 QChartSeries3D*；reviewer 审查点 |
| orbit pitch 万向锁/翻转 | pitch clamp ±89° + 每步正交化；单测覆盖 |
| far/near 比例过大 → 深度精度（z-fighting） | 默认 near=0.1/far=1000；fit 时按包围盒半径收紧 far；Phase 3 深度缓冲前再评估 |
| 屏幕近邻命中误判（3D hover 简化） | 阈值 8px + 取最近；仅用于联动高亮，不承担精确拾取（Phase 3 射线解决） |
| 双 Widget 数据副本不一致 | 两侧从同一数据源生成；单测锁 (u,v) 格点一致（§11 可选用例） |

### 13.2 遗留（明确不做 / 后置）

- 3D 轴刻度/轴 spine、射线拾取、3D tooltip/hover 增强、光照 → Phase 3+（D-3D-13）。
- 3D 场景导出（saveAsPng/Svg/Pdf）：buildExportScene 虚化后天然支持，但验收不要求、demo 不演示。
- 数值预转换缓存、批量投影 GPU 化、z-buffer 降级 → Phase 3（§12）。
- Phase 0 遗留：QChartWidget.h 前向声明化解耦（不属本阶段）。
- QChartBench 3D 场景基准（可加可不加；加则记入 Phase 3 基线）。

---

## 14. 实施顺序建议（供 captain 排期，依赖链）

1. **改名 + 基类**：QChartCamera→基类+QChartCamera2D（§4.4 全表）→ 编译 + 旧 69 例全绿（零回归门槛先立住）。
2. **数学层**：QChartMath.h + TestQChartMath（含 Projection3D 家族头文件）→ ctest。
3. **相机**：QChartCamera3D + TestQChartCamera3D（含退化一致性硬验收）→ ctest。
4. **系列**：QDataPoint3D + QChartSeries3D + 三子类 + TestQChartSurface3D → ctest。
5. **渲染**：QChartScene 3D 段 + QChartPrimitive + QChartLayer3D + QPainterChartRenderer 3D 子路径 + TestQChartRenderer3D → ctest。
6. **Widget**：QChartWidget3D（§8.2 基类钩子 + 子类）+ 交互。
7. **demo**：demo_scatter3d → demo_line3d → demo_surface3d（双 Widget 联动）。
8. **终验**：Linux 构建 0 error/0 warning + ctest 全绿（旧 69 + 新）+ 3 新 demo + 8 旧 demo 冒烟无回归；reviewer 独立审查每步。

> 每步完成后立即交付 reviewer 审查（D9：逐任务审查，实跑测试/demo），审查通过才进下一步。
