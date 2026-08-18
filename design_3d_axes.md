# design_3d_axes.md —— 3D 轴/网格控制流 设计文档（Phase 2 补项）

> **读者**：engineer（照此实现）、reviewer（照此审查）、captain（排期）。
> **依据**：用户拍板架构约束 A1~A10（t19 任务描述）+ **用户四点反馈修订（v2）**：①tick 改点标记；②网格与系列统一深度排序+偏置；③改名 QChartAxes3D + 职责边界；④viewCube 反算 + **R5 viewCube 主状态模型（v3）**：viewCube = World 空间盒（与相机无关）、相机纯映射器（design_3d.md §4.2 R5）。原 10 题问卷作废存档。
> **范围**：3D 轴（spine）/网格（盒/晶格）/刻度（tick 点标记）/标签（billboard）控制流；不碰 GPU；正交模式边框轴（2D 退化）后置（A4）。
> **红线遵守**：本文档为新增文件，不改任何代码/CMake/既有文档。
> **给 engineer 的约定**：API 签名即最终形态；★=本设计定案；⚠=注意的坑；旧 69 例零改动 + 现有 134 例零回归为硬指标。

---

## 1. 架构约束确认（A1~A10 → 落点，v2 修订处标注）

| # | 约束 | 落点 |
|---|---|---|
| A1 | 三层分离红线：Data→Numeric→World→Screen；②只知 Numeric↔World、③只知 World↔Screen；视图→dataBounds 反算属控制器 | §2 |
| A2 | 取景框哲学（R5 同步）：用户操作 viewCube（**World 空间盒，2D viewRect 的 3D 对标物，与相机无关**）；dataBounds 从 viewCube 8 角 fromWorld 派生（正向 computeWorldBounds）；网格/盒/轴/刻度/标签从 dataBounds 生成；视图变化才重算 | §2/§9 |
| A3 | 初始定位链（R5）：defaultDataBounds→computeWorldBounds→**setViewCubeToFit**（viewCube=目标盒）；优先级 显式 setDomainBox > 数据包围盒 > defaultDataBounds | §3 |
| A4 | 轴形态：dataBounds 盒 12 边 + 3 强调 spine（min 角）；单轴可隐藏；正交边框轴后置 | §4 |
| A5 | 网格：默认盒模式（地板 tick 对齐网格 + 盒边）；晶格模式可开关（三坐标面族）；Numeric 生成→toWorld 弯曲→投影 | §5 |
| A6 | 刻度/标签：**tick = 屏幕固定像素点标记**（v2 反馈 1，2D 点状标记先例）；标签 billboard（复用 tickLabels，固定像素字号+偏移）；轴标题可开关；走主题 | §6 |
| A7 | 深度语义：**网格与系列统一深度排序 + 网格 depth 小偏置（painter polygon offset）**；spine/盒边/刻度点/标签恒后画（前景层）（v2 反馈 2）；hover 不命中 | §7 |
| A8 | 编排归属：非 Q_OBJECT 编排器 **QChartAxes3D**（v2 反馈 3 改名，组合持有 QChartAxis*）；QChartLayer3D 持有；moc/CMake 红线零扰动 | §8 |
| A9 | 反向缺失兜底：无 fromWorld → 网格锚定域盒（静态参照系）或关闭；文档写明行为 | §2.4 |
| A10 | 与 2D 联动分工：3D=空间参照、2D=精确读数；demo 3D 轴默认开、'A' 键关 | §12 |

---

## 2. 三层分离与视图驱动 dataBounds（A1/A2）

### 2.1 职责边界（A1，红线）

```
① Data ─[Axis::toNumeric]─► Numeric      （通用坐标系：球/柱/笛卡尔，QChartAxis 职责不变）
② Numeric ─[Projection3D::toWorld]─► World （无限三维笛卡尔；fromWorld 反向同属②）
③ World ─[Camera3D::project]─► Screen      （视锥+透视/正交）
控制器（Widget3D）: 视图→dataBounds 反算（viewCube 8 角直接 fromWorld 聚合，R5：无逆矩阵）
```

- **②只知 Numeric↔World**（含 fromWorld 反向）；**③只知 World↔Screen**。任何一层不得学另一层空间——QChartAxes3D 只产 Numeric 空间几何（§8），Layer3D 做 ②→③ 转换（§8.3），渲染器只画 Screen 图元。
- QChartAxes3D 不碰 Axis 的绘制（drawAtEdge/drawAtPosition 是 2D 专属，3D 不调用）；只复用 `tickValues/tickLabels/title/color/tickCount`。

### 2.2 视图→dataBounds 反算（A2，R5：viewCube = World 空间盒，与相机无关）

**viewCube 定义（用户定案，不可回退）**：viewCube = **World 空间轴对齐盒 {min,max}**（三维笛卡尔空间，2D viewRect 的 3D 对标物），**与相机没有任何关系**——它是那个「可以和 dataBounds 互相反算的盒」：
- **正向**：fit 时 `dataBounds → Projection3D::computeWorldBounds → World 盒` = 初始 viewCube（A3 链，§3）。
- **反向**：`viewCube 5×5×5 网格采样 → fromWorld → min/max` = dataBounds（**用户定案：通用坐标系下极值点不一定在盒角上，只采 8 角会漏极值**；与 2D 极坐标 computeDataBounds 32×32 网格采样同哲学；Cartesian3D 走快速通道免采样，见下）。
- 主状态归属：viewCube + orientation + fovY 存于 `QChartCamera3D`（design_3d.md §4.2 R5），相机为纯映射器（position/lookAt/up/near/far 派生）；Widget3D 经 camera3D 读写盒，交互（pan/dolly/orbit）操作盒/朝向。

```cpp
// QChartWidget3D 新增（protected）
/// viewCube 5×5×5 网格采样（World 空间）→ fromWorld → min/max 聚合 → 缓存；
/// Cartesian3D 快速通道：免采样，dataBounds = viewCube 直接反算（恒等映射）
void QChartWidget3D::recomputeDataBounds3D();
```

实现要点：
1. **5×5×5 网格采样（通用路径）**：`viewCube = camera3D->viewCube()`；每轴 5 档（min, 25%, 50%, 75%, max）→ 5³=125 个 World 采样点。**无相机空间转换、无逆矩阵、无 unproject**（R5：viewCube 本就是 World 盒）。125 点 vs 2D 极坐标 1024 点，仍便宜一个数量级；与角点方案同复杂度实现，通用坐标系不漏极值（如柱/球坐标的 θ 极值可落在盒棱中点而非角上）。
2. **笛卡尔快速通道（用户定案）**：`projection3D->isIdentityMapping()`（§5.4 新虚拟，Cartesian3D 返回 true）→ **免采样**：恒等映射下 fromWorld ≡ 恒等，dataBounds = viewCube 的 min/max 直接取盒角（等价 8 角，零 fromWorld 调用）。性能意义：最常用坐标系（Cartesian3D）的反算从 125 次降到 0 次；同哲学贯穿轴/网格生成（§5.4）。
3. **聚合**：`num = projection3D->fromWorld(sample)`；非有限（NaN/Inf）跳过；`dataBounds3DMin/Max = 有效点 min/max`（全 NaN → A9 兜底，§2.4）。
4. **缓存时机 = 视图变化时**（A2）：camera3D `viewChanged`（setViewCube/orbit/dolly/pan 均触发）→ `recomputeDataBounds3D()` + 更新各 layer3D 轴盒（§8.3）+ `invalidateForeground()`。每帧重绘**不**重算（§9）。

**论证**：
- **与 2D 同构**：2D 的 `dataBounds = projection->computeDataBounds(viewRect)`（从 View Cartesian 视窗反算，32×32 采样）；viewCube 是 viewRect 的 3D 对应物（取景框 → 反算 Numeric 盒），5³ 采样与 2D 网格采样同哲学。正交俯视退化时 viewCube={viewRect 范围, z 覆盖数据平面}，Cartesian3D 快速通道下反算结果 == viewRect 对应 Numeric 盒（§10.2 用例 16）。
- **稳定性**：viewCube 是 World 盒，随交互（pan/dolly）平滑变化，无相机空间扫掠问题（弃用早期视锥/相机空间方案的理由：orbit 时近平面角点在数据域边缘扫掠、反算盒抖动）。
- **成本**：通用路径 125 点聚合，比 2D 极坐标 1024 采样便宜一个数量级；Cartesian3D 快速通道 0 成本（A2 + 用户性能定案）。
- **与相机解耦**：反算只看盒，不读 position/lookAt/up/fovY/near/far——相机怎么映射不影响 dataBounds（取景框哲学核心）。

```cpp
// QChartWidget3D 新增（public，读缓存）
QVector3D dataBounds3DMin() const;   // 可见 Numeric 盒 min（缓存）
QVector3D dataBounds3DMax() const;   // 可见 Numeric 盒 max（缓存）
bool dataBounds3DValid() const;      // false = A9 兜底生效（§2.4）
```

### 2.3 QChartMath::unproject（仅 Phase 3 射线拾取预留；R5 反算不再依赖）

```cpp
// QChartMath.h（inline）
/// Clip(齐次) → World：逆 viewProj ÷w；w<=0 → NaN 哨兵。Phase 3 射线拾取复用
inline QVector3D unproject(const QMatrix4x4& viewProj, const QVector4D& clipPos);
```

### 2.4 A9 反向缺失兜底（具体行为表，★定案）

| 情形 | dataBounds3DValid | 轴/网格数据盒来源 | 行为 |
|---|---|---|---|
| fromWorld 可用（Cartesian3D / Cylindrical3D / Spherical3D） | true | viewCube 反算（§2.2），随相机变化 | 正常视图驱动参照系（A2） |
| fromWorld 全 NaN（FunctionalProjection3D 无 backward、或全奇点） | **false** | **锚定域盒**（§3 域盒链结果），生成一次、**不随视图重算** | 静态参照系：相机旋转时盒/网格/轴不动（方向感仍可读）；不自动关闭网格（关闭只由用户 'A' 键/API 控制） |
| 部分有效（viewCube 部分角点在域外/奇点） | true（有效点聚合） | 视图驱动（聚合结果可能小于实际可见域） | 与 2D 采样法近似行为一致，接受 |

★ 定案理由：FunctionalProjection3D（球面/莫比乌斯 demo 即此）**没有反向**，反算必然 invalid；「锚定域盒静态参照系」让参照系稳定可读且零额外成本（图元生成一次）；「关闭」仅作为用户选项。

⚠ demo 现状影响：demo_surface3d 的球面/莫比乌斯均为 FunctionalProjection3D（无 fromWorld）→ 走 A9 静态路径：轴/网格锚定域盒，相机旋转时盒不动。line3d 用 Cylindrical3D（有反向）→ 视图驱动路径。

---

## 3. 初始定位链（A3）

```cpp
// QChartWidget3D 新增
public:
    /// 显式域盒（Numeric 空间；A3 优先级最高）。设置后立即 fitWorld + 更新轴盒
    void setDomainBox(const QVector3D& dataMin, const QVector3D& dataMax);
    void clearDomainBox();                       // 回退到 数据包围盒 > defaultDataBounds
    bool hasDomainBox() const;
    void fitWorld();                             // 现有方法，改造为按 A3 链
private:
    std::optional<QVector3D> m_domainMin, m_domainMax;
    /// 数据包围盒：遍历 layers3D series3DList → points() → toNumeric×3 → min/max（一次性，非每帧）
    std::pair<QVector3D,QVector3D> computeSeriesDataBounds() const;
    /// A3 链：显式 > 数据包围盒 > defaultDataBounds
    std::pair<QVector3D,QVector3D> resolveDataBox() const;
```

`fitWorld()` 改造（A3 全链，R5 语义）：
```
resolveDataBox() → projection3D->computeWorldBounds(dataMin, dataMax) → World 盒
→ camera3D->setViewCubeToFit(worldBox)（viewCube=目标盒、中心=盒中心；orientation/fovY 保持；R5）
→ recomputeDataBounds3D() → 推送轴盒到各 layer3D → invalidateForeground()
```
⚠ 距离派生（相机内）：d = radius/tan(fovY/2)（radius=半对角线，保守拟合定案）；精确拟合 d = max(hx/tan(fovX/2), hy/tan(fovY/2)) − hz 作为注释写入 QChartCamera3D（备将来实现，当前不做，见 design_3d.md §4.2）。
⚠ 数据包围盒聚合成本：大 series 下 O(N) 遍历——只在 fitWorld/域盒变化时执行一次；Phase 3 与数值预转换缓存合并优化。

---

## 4. 轴形态与盒图元（A4）

### 4.1 盒 12 边 + 3 强调 spine（Numeric 空间几何 → World → Screen）

角点约定（`QChartAxes3D::boxCorners`，§8.2）：`index = u | (v<<1) | (w<<2)`，u/v/w 每维 0=dataMin 分量、1=dataMax 分量；min 角 = 0。

| 边族 | 角索引对（12 条） | spine（min 角 0 出发，3 条强调） |
|---|---|---|
| u∥（固定 v,w） | (0,1)(2,3)(4,5)(6,7) | (0,1) ← spine 0 |
| v∥（固定 u,w） | (0,2)(1,3)(4,6)(5,7) | (0,2) ← spine 1 |
| w∥（固定 u,v） | (0,4)(1,5)(2,6)(3,7) | (0,4) ← spine 2 |

- **全 12 边淡框**（保证任意角度可读，A4）+ **3 条强调 spine**（min 角出发，颜色加深/线宽加粗，携带刻度点与标签）。
- 单轴可隐藏：`QChartAxes3D::axis(dim).visible = false` 隐藏该维 spine + 其刻度/标签（盒边仍画淡框）。
- **正交模式边框轴（2D 退化）后置**（A4）：正交模式下沿屏幕边缘画 2D 边框轴的风格**本补项不做**，记入 Phase 3/后续。

### 4.2 盒图元生成流程

```
Layer3D::collectPrimitives（盒部分）：
  box = m_axesDataMin/Max（§8.3，widget3D 注入）
  对 12 条边：两端点 Numeric 坐标 → 采样 S 段（§5.4）→ 每段 toWorld(端点) → camera3D->project
             → LineSegment 图元（layer=ForegroundDecor，§7）
  3 条 spine：同流程，layer=ForegroundDecor，色=轴色、线宽 2（淡框盒边线宽 1）
  网格/刻度点/标签见 §5/§6
```

---

## 5. 网格：盒模式 / 晶格模式（A5）

### 5.1 模式与开关

```cpp
// QChartLayer3D 新增
enum class GridMode { Box, Lattice };
void setGridMode(GridMode m);      GridMode gridMode() const;   // 默认 Box
// gridVisible 沿用基类 QChartLayer（默认 true）：网格总开关
// ⚠ gridFloorVisible / gridFloorHalfSize 移除（并入 Box 模式地板网格，迁移表见 §8.5）
```

- **盒模式（默认）** = 地板 tick 对齐网格 + 盒 12 边（§4）：地板 = 盒底面（w=wMin 平面）上的 2D tick 网格（沿 u、v 两个方向，tick 位置 = 该维 tickValues）。保留 gridFloorVisible 语义（地板网格可关，由 gridVisible 总控）。
- **晶格模式（可开关）** = 三坐标面族全画（§5.2）。
- 网格线 `layer=Grid`（§7：与系列统一深度排序 + 偏置），色走 `gridColor()`（基类/主题）。

### 5.2 晶格三族行集合（2D drawGrid 遍历模式 ×3）

| 族 | 固定 | 扫掠 | 行数 |
|---|---|---|---|
| 族 U（平行 u） | (vⱼ, wₖ)，j=0..nv、k=0..nw | u | (nv+1)×(nw+1) |
| 族 V（平行 v） | (uᵢ, wₖ)，i=0..nu、k=0..nw | v | (nu+1)×(nw+1) |
| 族 W（平行 w） | (uᵢ, vⱼ)，i=0..nu、j=0..nv | w | (nu+1)×(nv+1) |

- nu/nv/nw = 各维 `tickValues(dimMin, dimMax)` 返回的刻度数（-1 计数）。
- 每行 = 一条线：Numeric 空间两点 → 采样 S 段（§5.4）→ toWorld 弯曲 → 投影（**换投影即扭曲**：球/柱下晶格自然弯曲，零额外代码，A5 相对论场景）。
- 排序粒度：**每条线按采样段拆成多个 LineSegment 图元，每段独立 depth**（§7.3——段级排序保证穿入数据的线前后段正确遮挡；段内穿插在 32 段粒度下视觉可接受）。

### 5.3 性能预算（记入 Phase 3 输入）

| 模式 | 图元数 | 说明 |
|---|---|---|
| 盒（默认，tickCount≈5） | **≈1k** | 12 盒边×32 段=384 + 地板 2×6×32=384 + 刻度点 3×6=18 + spine/装饰 ≈0.1k |
| 晶格（tickCount≈9） | **≈9.6k** | 3 族 × 10² 行 × 32 段 = 9600 |
| 采样段数 S | 32（默认）/ 2（Cartesian3D，§5.4） | 每线分段数，弯曲投影必需 |

⚠ tick 从短线改为点标记后，刻度项为 Point 图元（不采样），预算更宽松；量级不变。

### 5.4 投影快速通道与采样段数提示（QChartProjection3D 扩展，additive）

```cpp
// QChartProjection3D.h 追加（默认实现；子类覆盖）
/// 直线采样段数提示：弯曲投影 32，Cartesian3D 恒等 → 2（两点直线）
virtual int samplingSegmentsHint() const { return 32; }
/// 恒等映射快速通道（用户定案）：恒等映射下 fromWorld/toWorld ≡ 恒等，
/// 反算 dataBounds 免采样（§2.2 快速通道）、图元生成免 toWorld/分段（直接 Num→World 直通）
virtual bool isIdentityMapping() const { return false; }
```
- `QChartCartesianProjection3D` 覆盖：`samplingSegmentsHint()` → 2、`isIdentityMapping()` → **true**。
- 快速通道落点（笛卡尔性能定案，用户要求「不必要步骤快速通过」）：
  1. **反算 dataBounds**（§2.2）：identity → 免 5³ 采样，直接取盒 min/max（0 次 fromWorld）。
  2. **盒边/网格/晶格图元**（§8.4 emitLine）：identity → 免 toWorld 调用（Numeric 坐标即 World 坐标）、段数=2（`samplingSegmentsHint`），两端点直接投影。
  3. **Series 闭包**（design_3d.md §7.1 makeProjectFn）：identity → 免 toNumeric 之外的 toWorld 冗余（Numeric≡World 时闭包直通；toNumeric 是否恒等由各 Axis 决定，不越权）。
- Layer3D 生成盒边/网格/晶格时按 `samplingSegmentsHint()` 分段；球/柱下 32 段自然弯曲。

---

## 6. 刻度与标签（A6）

### 6.1 刻度 tick = 屏幕固定像素点标记（v2 反馈 1 定案）

**方案**（2D「弯曲轴上的点状标记」先例，design_notes.md：弯曲坐标轴不做局部法向量推导，直接 drawPoint）：

- **位置**：`QChartAxes3D::ticks(dim, dimMin, dimMax)` = 复用 `axis->tickValues`；锚点 `tickAnchor(dim, tickValue, dataMin)` = dataMin 的 dim 分量替换为 tickValue（落在 min 角 spine 边上）。
- **形态**：`QChartPrimitive{ Type::Point, layer=ForegroundDecor, a=project(toWorld(anchor)).screen, markerSize=cfg.markerSizePx, color=轴色 }`。
- **大小定案：屏幕固定像素（默认半径 4.0px，`markerSizePx` 可配）**。理由：①2D 先例 drawPoint 即像素点，直接同构；②世界尺寸在 QPainter 线框渲染中没有对象尺度语义（无光照/透视缩放的对象尺寸），且与 billboard 标签（固定像素字号）哲学一致——刻度与标签都是**屏幕稳定的参照元素**，任意 dolly 下清晰可辨；③实现最简：Point 图元与 markerSize 字段现成，锚点投影即得。
- **删除切线方向/长度计算**（tickLengthWorld/tickInvert/tickDirNumeric 不再需要——与 2D 一样不做局部法向量/切线推导）。

### 6.2 标签（billboard 屏幕空间文本）

```cpp
// QChartRenderer.h 新增
struct QChartTextLabel {
    QPointF screenPos;          // 锚点屏幕坐标（已含偏移）
    QString text;               // tickLabels 输出 或 轴标题
    Qt::Alignment anchor = Qt::AlignLeft | Qt::AlignVCenter;  // 相对 screenPos 的对齐
    qreal fontSize = 10.0;      // 像素字号
    QColor color;               // 主题 textColor / axisColor
    bool isTitle = false;       // 轴标题（渲染可加大加粗）
};
```

- **内容**：`tickLabelTexts(dim, min, max)` = 复用 `axis->tickLabels(ticks)`（A6）；轴标题默认 = `axis->title()`（空则 `dimensionName`）。
- **位置**：`screenPos = project(toWorld(tickAnchor)).screen + cfg.labelOffsetPx`（像素偏移，默认沿投影轴向外 10px，可配）；恒面向相机（drawText 即 billboard，QPainter 无真 3D 文本，A6）。
- **数量**：联动 demo 场景 `axis->setTickCount(2~3)` 压刻度（A6）。
- **主题**：字号/颜色走主题（axisColor/textColor），与 2D 标签一致。
- **绘制**：Renderer `drawLabels`（§7.2 步骤 6）；标签裁剪到 plotArea（防溢出）。

---

## 7. 深度语义（A7，v2 反馈 2 定案）

### 7.1 分层模型

**网格不再是"背景层先画"**——几何穿插场景（球嵌晶格）下错误：球**前方**的晶格线应遮挡球，纯分层会被球盖住。定案：

- **网格（Grid）与系列（Series）统一深度排序**（一条列表按 depth 降序绘制，远→近）；网格项 depth 加一个小偏置（**painter 版 polygon offset**）：`gridDepth = depth + kGridDepthBias`（kGridDepthBias=1e-3，实现已定案），**同深度处系列优先**（z-fighting 时系列赢；网格视为「略远」→ 先画 → 系列覆盖）。
- **spine / 盒 12 边 / 刻度点 / 标签保持前景层（ForegroundDecor）恒后画**，不与系列/网格比较深度。理由：它们是**框外装饰**、定义数据域边界——被数据遮住会失去参照意义；billboard 文本被 3D 物体遮挡视觉怪异且实现复杂；且球面 demo 中盒边恰在球面上（参照系与数据重叠的极端情形），若参与排序会被球完全盖住、盒不可读。网格是**场景内参照面**（需要与数据几何正确穿插），故参与排序——两类语义不同，分层处理。

```cpp
// QChartRenderer.h —— QChartPrimitive 追加
enum class Layer { Grid, Series, ForegroundDecor };
Layer layer = Layer::Series;   // 默认 Series → 现有系列收集代码零改动
```
（v2：原 BackgroundGrid 更名 Grid，语义从"先画背景"改为"与系列统一排序 + 深度偏置"；本枚举尚未实现，改名零兼容成本。）

### 7.2 渲染顺序（drawForeground3D，v2）

```
drawForeground3D：
 1. collect：各 layer3D collectPrimitives(cam, plotArea, items, &labels)（labels 可选出参，§8.3）
 2. 分桶：depthItems（Grid + Series）、decorItems（ForegroundDecor）
 3. 偏置：对 Grid 项 depth += kGridDepthBias（同深度系列优先；painter polygon offset，网格视为略远先画）
 4. depthItems 按 depth 降序统一排序（远→近），绘制  ← 球前方网格线 depth 更大 → 后画 → 遮挡球 ✓
 5. 画 decorItems（顺序）：盒 12 边、spine、刻度点    ← 恒可见（不被系列/网格遮挡）
 6. 画 labels（billboard 文本）                      ← 最上层
 7. 2D overlay（图例）后画（现有步骤保留）
```
- 常量：`static constexpr qreal kGridDepthBias = 1e-3;`（世界单位；相对典型场景足够小、大于浮点噪声；可调）。
- 现有逐图元绘制循环抽成 `drawPrimitives(p, items)`。
- ⚠ 现有 134 例零回归保证：`Layer` 默认 Series；轴/网格图元**只在 axesDataBox 有效时生成**（§8.3 默认无效）→ 直接组装的测试场景零轴零网格，nearCoversFar 等像素断言不受影响。

### 7.3 排序粒度与段内穿插

- 排序粒度 = **采样段图元**（每条网格线按 S 段拆成独立 LineSegment，每段取中点投影深度）——穿入数据的线（如晶格线穿过球）前段在球前、后段在球后，段级排序正确分层；段内穿插（单段穿过球表面）在 32 段粒度下视觉可接受（与线框渲染精度一致）。系列线段同理（现有行为）。
- ⚠ 盒边/spine 是装饰层（不排序），Cartesian 下段数 2，无段中点问题；网格/系列按采样段取中点。

### 7.4 hover 不命中（A7）

- 3D hover（`QChartWidget3D::updateHover`）屏幕近邻遍历**只扫 Series 层图元**（`layer == Series` 且 dataIndex ≥ 0）；Grid/ForegroundDecor（dataIndex=-1）直接排除。

---

## 8. 编排归属：QChartAxes3D（A8，v2 反馈 3 改名）

### 8.1 职责边界（反馈 3 回答，写入文档）

| | 2D QChartAxis | QChartAxes3D |
|---|---|---|
| Data↔Numeric 数值化（toNumeric/fromNumeric） | ✅ 核心职责 | ❌ 不做 |
| 刻度生成（tickValues） | ✅ | ✅ **复用**（组合持有 QChartAxis*，不重实现） |
| 标签格式化（tickLabels） | ✅ | ✅ **复用** |
| 2D 绘制（drawAtEdge/drawAtPosition/sizeHint） | ✅ | ❌ 不绘制（无 QPainter） |
| 样式（color/title/visible/tickCount） | ✅ | ✅ **复用** + 3D 专属配置 |
| 3D 专属编排（盒 12 边/spine/刻度锚点/标签偏移/可见性） | ❌ | ✅（只产 **Numeric 空间几何**） |

**一句话**：QChartAxes3D 不是"3D 版 Axis"，而是"3D 轴参照系的编排器"——刻度/标签的**内容**仍由 2D Axis 生成，3D 侧只负责把它们编排到盒/spine 上。

**命名判断（反馈 3）**：`QChartAxis3D` 确有误读风险——QChartAxis 是库内核心概念（数值化+刻度+标签+绘制），`QChartAxis3D` 暗示平行类族，让人以为也有 toNumeric/绘制。**定案改名 `QChartAxes3D`**（单对象 + 每维 AxisConfig 槽）：①消除误读；②3D 轴参照系是**单一概念**（盒+spine+刻度+标签一体），单对象 API 更内聚；③与 A8「Layer3D 持有」语义兼容（持有三个 per-dim 配置槽）。备选 `QChartAxisFrame3D`（强调框，但与 QFrame 混淆），不取。

### 8.2 QChartAxes3D 完整签名（非 Q_OBJECT → moc/CMake 红线零扰动）

```cpp
// QChartAxes3D.h —— 3D 轴参照系编排器（非 Q_OBJECT）
// 职责：①复用 2D Axis 的刻度生成/标签格式化/样式（组合，非继承——2D drawAtEdge/drawAtPosition
//       语义与 3D 不兼容，继承会带进误导性绘制接口）；②产出 Numeric 空间几何（盒/边/spine/刻度锚点）。
// 三层分离红线：本类只产 Numeric 空间几何，不做 toWorld/投影（Layer3D 做）；不数值化、不绘制。
class QChartAxes3D {
public:
    QChartAxes3D();

    // ===== 每维配置槽（dim∈{0,1,2}；组合复用 QChartAxis*）=====
    struct AxisConfig {
        QChartAxis* axis = nullptr;        // 复用刻度/标签/样式（非持有）；null = 该维不生成刻度/标签
        bool visible = true;               // 单轴隐藏（A4），默认 true
        qreal markerSizePx = 4.0;          // tick 点标记半径（px，v2 反馈 1）
        QPointF labelOffsetPx{0, 0};       // 标签偏移（px）；(0,0)=自动（沿投影轴外 10px）
        bool axisTitleVisible = true;
        QString axisTitle;                 // 空 = axis->title()，再空 = dimensionName
    };
    AxisConfig& axis(int dim);             // 配置入口
    const AxisConfig& axis(int dim) const;

    // ===== 总开关（demo 'A' 键）=====
    bool visible() const;        void setVisible(bool v);     // 默认 true

    // ===== Numeric 空间几何（静态工具 + 委托，Layer3D 调用）=====
    /// 盒 8 角；约定 index = u | (v<<1) | (w<<2)
    static QVector<QVector3D> boxCorners(const QVector3D& dataMin, const QVector3D& dataMax);
    /// 12 条边（角索引对）：u∥(0,1)(2,3)(4,5)(6,7)；v∥(0,2)(1,3)(4,6)(5,7)；w∥(0,4)(1,5)(2,6)(3,7)
    static QVector<QPair<int,int>> boxEdges();
    /// 3 条强调 spine（min 角出发）：{u∥边0, v∥边4, w∥边8}
    static QVector<int> spineEdgeIndices();
    /// 该维刻度值（Numeric）：= axis->tickValues(dimMin, dimMax)；axis 为 null 返回空
    QVector<qreal> ticks(int dim, qreal dimMin, qreal dimMax) const;
    /// 刻度标签：= axis->tickLabels(ticks)
    QStringList tickLabelTexts(int dim, qreal dimMin, qreal dimMax) const;
    /// 刻度锚点（Numeric）：dataMin 的 dim 分量替换为 tickValue（min 角 spine 边上）
    static QVector3D tickAnchor(int dim, qreal tickValue, const QVector3D& dataMin);
private:
    AxisConfig m_cfg[3];
    bool m_visible = true;
};
```

⚠ 三层分离检查：QChartAxes3D 无 QPainter、无 QChartCamera3D、无 QChartProjection3D 引用——纯 Numeric 空间几何 + 配置。所有 toWorld/project 在 QChartLayer3D。

### 8.3 QChartLayer3D 扩展（A8：持有 QChartAxes3D + 轴盒）

```cpp
// QChartLayer3D 新增
public:
    /// 轴参照系编排器（拥有；默认绑定 layer 的 axisX/Y/Z——dim0→axisX、dim1→axisY、dim2→axisZ；
    /// setAxisX/Y/Z 时自动重绑；用户经 axes3D()->axis(dim) 配置）
    QChartAxes3D* axes3D();
    const QChartAxes3D* axes3D() const;

    void setGridMode(GridMode m);    GridMode gridMode() const;   // §5.1，默认 Box

    /// 轴/网格数据盒（Numeric；widget3D 注入：dataBounds3D 或 A9 域盒）。
    /// 默认 (0,0,0)-(0,0,0) = 无效 → 不生成任何轴/网格图元（现有直接组装场景零影响）
    void setAxesDataBox(const QVector3D& dataMin, const QVector3D& dataMax);
    bool hasValidAxesDataBox() const;

    /// 图元收集（签名向后兼容：labels 可选出参；分层经 QChartPrimitive::Layer）
    void collectPrimitives(const QChartCamera3D* cam, const QRectF& plotArea,
                           QVector<QChartPrimitive>& out,
                           QVector<QChartTextLabel>* labels = nullptr) const;

private:
    std::unique_ptr<QChartAxes3D> m_axes3D;   // 拥有；默认各槽绑定 axisX/Y/Z
    GridMode m_gridMode = GridMode::Box;
    QVector3D m_axesDataMin{0,0,0}, m_axesDataMax{0,0,0};
    // （gridFloorVisible / gridFloorHalfSize 移除，§8.5）
```

`collectPrimitives` 内部编排（顺序与分层）：
1. 盒无效（min==max 或 min>max）→ 只收集系列图元（现有逻辑），返回。
2. **Grid 层**：盒模式 = 盒底面（w=wMin）tick 网格；晶格模式 = 三族（§5.2）。色 = `gridColor()`，虚线可配；depth 按采样段中点（渲染器统一加偏置，§7.2）。
3. **Series 层**：现有系列图元（worldCache 填充 + 全链闭包，不变）。
4. **ForegroundDecor 层**：盒 12 边（淡框，线宽 1）+ 3 强调 spine（线宽 2、轴色）+ 各可见轴刻度**点标记**（§6.1）。
5. **labels**（若出参非空）：各可见轴 tick 标签（billboard 偏移）+ 轴标题（spine 端点）。

### 8.4 图元/标签生成辅助（Layer3D 私有实现要点）

```cpp
// Numeric 点 → Screen（②③交界）：toWorld → camera3D->project
QChartProjectedPoint projectNumeric(const QVector3D& num, const QChartCamera3D* cam,
                                    const QRectF& plotArea) const;
// 直线采样（Numeric 两点 → World 折线采样，段数 = projection3D->samplingSegmentsHint()）
// 每段投影成 LineSegment 图元（depth=段中点）；任一端 NaN 跳过该段
void emitLine(QVector3D numA, QVector3D numB, QChartPrimitive::Layer layer,
              const QColor& color, qreal penWidth, const QChartCamera3D* cam,
              const QRectF& plotArea, QVector<QChartPrimitive>& out) const;
```
⚠ 盒边/spine/网格在 Cartesian3D 下段数=2（直线两点）；球/柱下 32 段自然弯曲（§5.4）。

### 8.5 gridFloor 并入迁移表（A5「保留 gridFloorVisible 语义」）

| 旧 API（t13 现状） | 新 API（本设计） | 说明 |
|---|---|---|
| `setGridFloorVisible(bool)` | `gridVisible()`（基类，默认 true） | 语义保留：地板网格可关；关 = 网格全关 |
| `setGridFloorHalfSize(qreal)` | 移除；盒大小由 axesDataBox（widget3D 驱动）决定；手动范围用 `QChartWidget3D::setDomainBox` | 参照系从数据盒派生（A2），不再手工 half |
| `m_gridFloorVisible`（y=0 平面网格，深度混排） | Box 模式盒底面（w=wMin）tick 对齐网格，`Layer=Grid`（与系列统一排序 + 偏置，§7） | 深度语义变更：混排 → 统一排序（v2 反馈 2 定案） |
| demo_surface3d L126-127（setGridFloorVisible(true)+HalfSize(1.6)） | 删除两行；默认 gridVisible=true + widget3D 驱动盒；如需手动范围 setDomainBox | §12 demo 同步 |

⚠ 现有测试零依赖 gridFloor（grep 仅 demo 使用）→ 迁移零回归风险。

---

## 9. 重算分工（每帧 vs 视图变化）

| 事件 | 动作 | 频率 |
|---|---|---|
| **视图变化**（orbit/dolly/pan 操作 viewCube、fit/相机 setter/投影或域盒变化） | ① `recomputeDataBounds3D()`（viewCube 8 角直接 fromWorld，R5；A9 无效则跳过）② 更新各 layer3D `setAxesDataBox`（dataBounds3D 有效 → 用它；否则 A9 域盒）③ `invalidateForeground()` | 交互/设置时，非每帧 |
| **每帧重绘**（QPainter 重绘） | 仅图元收集 + 分层绘制（§7.2）；**不重算 dataBounds3D、不重建盒** | 每帧 |
| 域盒链（A3） | `resolveDataBox()` 结果缓存；投影/域盒变化时重算 | 变化时 |
| 数据包围盒聚合（§3） | fitWorld/域盒变化时一次性 | 变化时 |

⚠ 图元收集每帧发生（QPainter 无场景缓存，CPU 即时绘制）；预算 ≈1k/9.6k 图元（§5.3）可接受。若性能不达标 → Phase 3 图元缓存/GL（§13 性能输入）。

---

## 10. 测试用例清单

### 10.1 新增 TestQChartAxes3D（编排器单测，tests/test_qchartaxes3d.h/.cpp）

| # | 用例 | 断言 |
|---|---|---|
| 1 | `boxCorners_convention` | 8 角 = dataMin/dataMax 按 index=u\|v<<1\|w<<2（手算对照） |
| 2 | `boxEdges_12` | 12 条边、u/v/w 各 4 条平行、每条边两端点恰差一个分量 |
| 3 | `spineEdges_fromMinCorner` | 3 条 spine 均含角 0（min 角）、分别沿 u/v/w |
| 4 | `ticks_reuseAxis` | `axes.ticks(dim,min,max) == axis->tickValues(...)`（Axis 复用回归） |
| 5 | `tickAnchor_onSpine` | anchor.dim == tickValue、其余分量 == dataMin 对应分量 |
| 6 | `markerSizePx_default` | 默认 4.0、可配；per-dim 独立 |
| 7 | `labels_reuseAxis` | tickLabelTexts == axis->tickLabels |
| 8 | `axisConfig_perDim` | 每维 visible/markerSize/labelOffset 独立生效（改 dim0 不影响 dim1） |

### 10.2 renderer3d 扩展（像素/图元断言，tests/test_qchartrenderer3d.cpp 追加；v2 更新）

| # | 用例 | 断言 |
|---|---|---|
| 9 | `boxMode_primitiveCounts` | 盒模式 + Cartesian3D（段数 2）：盒 12 边图元 + 地板 2×(T+1) 线 + tick 点 3×(T+1)；Layer 归属正确（网格=Grid、spine/盒边/tick=ForegroundDecor、系列=Series） |
| 10 | `lattice_rowCounts` | 晶格模式 3 族行数 = (nu+1)(nw+1)+(nv+1)(nw+1)+(nu+1)(nv+1) |
| 11 | `gridBehindSeries_pixel` | 球**后方**网格线被球盖住（像素 = 曲面色） |
| 12 | `gridInFrontOfSeries_pixel` | 球**前方**网格线遮挡球（像素 = 网格色）——构造：网格线位于相机与球之间 |
| 13 | `gridTie_seriesWins` | 同深度处（正交俯视 y=0 平面网格 vs z=0 系列线）像素 = 系列色（kGridDepthBias 偏置生效） |
| 14 | `decorAlwaysOnTop` | 盒边/spine 恒在系列之上（像素可见，不被球盖） |
| 15 | `axesToggle_zero` | `axes3D()->setVisible(false)`（等价 'A' 键）→ decor/labels 图元为 0（无轴色像素） |
| 16 | `dataBounds3D_viewCubeReverse` | Cartesian3D 快速通道：`isIdentityMapping()==true` → 免采样，反算 min/max == viewCube 本身（正交俯视退化 == 2D viewRect 对应 Numeric 盒） |
| 17 | `dataBounds3D_noBackward_fallback` | FunctionalProjection3D（无 fromWorld）→ dataBounds3DValid==false → 轴/网格用域盒（静态：相机变化后盒图元不变） |
| 18 | `dataBounds3D_gridSampling_curved` | 柱坐标（有反向）：viewCube 5×5×5 采样 → fromWorld 聚合；**构造极值不在角上的场景（θ 极值落在盒棱中点），断言 5³ 采样能捕获而 8 角方案会漏** |
| 19 | `samplingHint` | Cartesian3D→2 段、球面→32 段（§5.4） |
| 20 | `identityFastPath` | Cartesian3D `isIdentityMapping()==true`、球/柱/Functional==false；identity 下 emitLine 免 toWorld、段数=2（§5.4） |

### 10.3 回归保障（硬指标）

- 旧 69 例**零改动**；现有 3D 134 例**零回归**：`QChartPrimitive::Layer` 默认 Series；collectPrimitives 签名向后兼容（labels 默认 nullptr）；轴/网格默认不生成（axesDataBox 无效）→ nearCoversFar 等像素断言不受影响；gridFloor 默认关且无测试依赖（§8.5）。
- 运行：同一 `QChartTests` + ctest（offscreen 无头）全量；Linux 构建 0 error/0 warning。

---

## 11. Widget3D 控制方法汇总（A2/A3/A9 落点）

```cpp
// QChartWidget3D 新增（public/protected/private 汇总，§2/§3/§8.3）
public:
    void setDomainBox(const QVector3D& dataMin, const QVector3D& dataMax);
    void clearDomainBox();
    bool hasDomainBox() const;
    QVector3D dataBounds3DMin() const;  QVector3D dataBounds3DMax() const;
    bool dataBounds3DValid() const;
protected:
    void recomputeDataBounds3D();          // viewCube 8 角直接 fromWorld（R5，§2.2）
private:
    std::pair<QVector3D,QVector3D> computeSeriesDataBounds() const;   // 一次性聚合
    std::pair<QVector3D,QVector3D> resolveDataBox() const;            // A3 链
    void pushAxesDataBoxToLayers();                                   // 更新各 layer3D 轴盒
    std::optional<QVector3D> m_domainMin, m_domainMax;
    QVector3D m_dataBounds3DMin{0,0,0}, m_dataBounds3DMax{0,0,0};
    bool m_dataBounds3DValid = false;
    std::pair<QVector3D,QVector3D> m_anchorBox;                       // A3 域盒链缓存
```
- 构造/`setCamera3D` 时 connect `camera3D->viewChanged` → `recomputeDataBounds3D + pushAxesDataBoxToLayers + invalidateForeground`（视图变化才重算，A2）。
- `fitWorld()` 改造为 A3 全链（§3）。

---

## 12. demo 同步计划（A10）

| demo | 同步内容 |
|---|---|
| `demo_surface3d` | 3D 侧加盒/spine/刻度点/标签（球面/莫比乌斯走 A9 静态域盒路径）；删 setGridFloorVisible/HalfSize 两行（§8.5）；按键 'A' 切换 `axes3D()->setVisible`；轴 tickCount 压 2~3（A6）；右 2D 平面完整 2D 轴不动（精确读数区，A10） |
| `demo_line3d` | 加盒/spine/刻度（Cylindrical3D 有反向 → 视图驱动路径，旋转时盒随 dataBounds 更新）；'A' 键 |
| `demo_scatter3d` | 建议同步（统一观感）；可后置（不阻塞验收） |
| test.cpp argv | 无需新增入口（3 demo 已注册） |

---

## 13. Phase 3 预留与风险

### 13.1 Phase 3 预留

| 预留项 | 本补项落点 | Phase 3 消费 |
|---|---|---|
| `QChartMath::unproject`（Clip→World） | §2.3 | 射线拾取（屏幕→NDC→World 射线）直接复用 |
| viewCube 反算 8 角聚合 | §2.2 | GL 后端可见域判定/裁剪（frustum culling 的近似输入） |
| 图元分层（Layer 枚举）+ 图元计数预算 | §5.3/§7 | 图元缓存/GL 命令缓冲；1k/9.6k 为 VBO 规模基线 |
| 网格 depth 偏置（painter polygon offset） | §7.2 | GL 版 polygon offset（glPolygonOffset）同语义 |
| `samplingSegmentsHint` | §5.4 | 曲面细分策略联动 |
| 轴/网格图元缓存（视图变化才重建） | §9 | 静态参照系（A9）天然可缓存 |
| 正交模式边框轴（2D 退化） | A4 后置 | Phase 3 或后续 |
| 数据包围盒聚合与数值预转换缓存合并 | §3 ⚠ | D-3D-10 性能项 6 合并 |

### 13.2 风险

| 风险 | 对策 |
|---|---|
| 晶格模式 9.6k 图元 CPU 收集每帧（§5.3） | 默认盒模式 1k；晶格仅用户显式开；Phase 3 缓存/GL |
| 网格 depth 偏置量纲（kGridDepthBias=1e-3 世界单位）在超大/超小场景下失效 | 相对场景尺度可调常量；单测 gridTie_seriesWins 锁默认行为；Phase 3 换 glPolygonOffset |
| 段级排序的段内穿插（晶格线穿过球表面单段内） | 32 段粒度视觉可接受（§7.3）；与线框精度一致 |
| A9 静态参照系下相机旋转后盒与数据错位 | 有意为之（参照系固定）；文档写明；'A' 键可关 |
| 默认值变化影响现有 134 例 | axesDataBox 默认无效 → 直接组装场景零轴零网格（§7.2 ⚠） |
| 数据包围盒聚合 O(N) | 仅 fitWorld/域盒变化时一次性（§3 ⚠） |

### 13.3 遗留（明确不做）

- 正交模式 2D 边框轴（A4 后置）；真 3D 文本（billboard 为准，Phase 3 GL 仍 billboard）；轴/网格动画（Phase 4）；tick 次刻度（subTickValues 复用可选，默认不做）；tick 世界尺寸标记（反馈 1 否决）。

---

## 14. 实施顺序建议（供 captain 排期）

1. **数学/结构前置**：QChartProjection3D::samplingSegmentsHint（additive；`QChartMath::unproject` 为 Phase 3 射线拾取预留、本补项不实现，§2.3）。
2. **QChartAxes3D**（§8.2，非 Q_OBJECT）+ TestQChartAxes3D（10.1 全 8 例）。
3. **QChartLayer3D 扩展**（§8.3：axes3D 编排器 + axesDataBox + GridMode + collectPrimitives 分层与标签；gridFloor 并入）→ 现有 134 例回归 + 新图元计数用例（10.2-9/10）。
4. **Renderer 分层**（§7.2：Grid+Series 统一排序 + kGridDepthBias 偏置 + decor 后画 + drawLabels）+ 像素断言（10.2-11/12/13/14/15）。
5. **Widget3D 控制器**（§2.2/§3：viewCube 反算 + 域盒链 + fitWorld 改造 + 视图变化钩子）+ 用例（10.2-16/17）。
6. **demo 同步**（§12：surface3d/line3d 加轴网格 + 'A' 键）。
7. **终验**：Linux 构建 0 error/0 warning + ctest 全量（旧 69 + 现 134 + 新）+ 11 demo 冒烟；reviewer 逐任务独立审查（实跑）。

> 每步完成后立即交付 reviewer（D9 逐任务审查），审查通过再进下一步。

---

## 附：修订记录

### v2（用户四点反馈）

| 反馈 | 修订 |
|---|---|
| 1 tick 太复杂 | §6.1 改为屏幕固定像素点标记（markerSizePx 默认 4px），删除切线方向/长度计算（2D 点状标记先例） |
| 2 网格深度缺陷 | §7 改为网格与系列统一深度排序 + kGridDepthBias=1e-3 偏置（painter polygon offset）；spine/盒边/刻度点/标签保持前景层恒后画（理由见 §7.1）；§10.2 用例 11 拆为 11/12/13 三例 + 新增 14 |
| 3 QChartAxis3D 命名/职责 | §8 改名 QChartAxes3D（单对象 + 每维 AxisConfig 槽）；§8.1 职责边界表 + 命名论证 |
| 4 采样来源 | §2.2 改为 viewCube 8 角 fromWorld 聚合（后经 R5 再修订为 World 空间盒直接取角） |

### v3（R5 viewCube 主状态模型同步，用户拍板）

| 项 | 修订 |
|---|---|
| viewCube 定义 | §2.2 修订：viewCube = **World 空间轴对齐盒**（2D viewRect 的 3D 对标物，**与相机无关**）；删除相机空间盒/逆 viewMatrix 方案 |
| 反算 | viewCube 8 角**直接取盒角** → fromWorld → min/max（无逆矩阵、无 unproject）；与相机解耦（不读 position/lookAt 等派生值） |
| unproject | §2.3 措辞：仅 Phase 3 射线拾取预留，本补项不实现（§14 步骤 1 同步） |
| A2 行 | §1 约束表 A2 措辞同步（viewCube 与相机无关） |
| 用例 | §10.2 用例 16 改为 `dataBounds3D_viewCubeReverse`（Cartesian3D 恒等：反算 == viewCube 本身） |

### v4（用户三项定案：5³ 采样 / 笛卡尔快速通道 / 交互语义）

| 项 | 修订 |
|---|---|
| 采样 | §2.2 反算改 **5×5×5=125 点网格采样**（通用坐标系极值不在角上，用户定案）；用例 16 改 Cartesian3D 快速通道断言 + 新增用例 18（柱坐标 5³ 采样捕获棱中点极值） |
| 快速通道 | §5.4 新增 `isIdentityMapping()`（Cartesian3D=true）：反算免采样、emitLine 免 toWorld/段数=2、Series 闭包直通；新增用例 20 |
| 交互语义 | §2.2/§9 措辞同步：orbit 只转 orientation（viewCube 不动）；**平移不做鼠标手势**（R6，仅 API panViewCube/setViewCubeCenter） |
| 主状态归属 | viewCube+orientation+fovY 存于 QChartCamera3D（design_3d.md §4.2 R5）；相机纯映射器；交互操作盒/朝向 |
