# 绘制结构设计（v2）

> 状态：**设计稿，未实施**。本文取代 `design_dirty_and_segments.md`。
> 约定：契约编号 `S*`；阶段编号 `§2.x`；标注“现状”的 `file:line` 只描述**当前代码**，不代表设计意图。

---

## 第 0 部分 · 定义

后面每一节都要能指回这里某一条。定义不扎实，流程章就没法写。

### 0.1 `QChartPrimitive` —— 图元

**它是什么**：一条最小几何绘制命令。几何量一律是 **Numeric** 坐标。

字段按“谁写”分组，不按类型分组：

| 组 | 字段 | 谁写 | 用途 |
|---|---|---|---|
| 几何 | `type` | 生产者 | 形态 |
| 几何 | `numA` / `numB` | 生产者 | Point / Line 的 Numeric 坐标 |
| 几何 | `numRect` | 生产者 | Rect / Ellipse（轴对齐，左上+宽高） |
| 几何 | `numVerts` / `numIndices` | 生产者 | Path / Polygon / Grid / TriangleMesh / Fan / Strip |
| 几何 | `rows` / `cols` | 生产者 | **仅 Grid**：网格的行列数 |
| 样式 | `color` / `fillColor` / `penWidth` / `markerSize` | 生产者 | 纯消费 |
| 运行时 | `visible` | renderer 裁剪步 | 本帧是否可见 |
| 变换产物 | `cartA` / `cartB` / `cartRect` / `cartVerts` / `cartIndices` | CPU 变换步 | **仅 CPU 后端使用**；GL 后端不填 |

**两条边界必须牢记**：

1. 图元**不知道**自己属于谁 —— 归属由所属的 `Segment` 表示。
2. 图元**不知道**自己在屏幕上的位置 —— 那是渲染阶段的事。

**形态清单**（9 种）：

| 形态 | 语义 | 是否参与细分 |
|---|---|---|
| `Point` | 单点（散点、刻度点） | 否 |
| `Line` | 两点直线段（轴脊、折线的一段） | **是**（按段加密） |
| `Path` | 顶点序列（折线、闭合描边） | **是**（按相邻点分段加密） |
| `Rect` / `Ellipse` | 轴对齐（柱状、图例底等） | 否（其边不加密） |
| `Polygon` | 任意多边形（单环，renderer 三角化） | 否 |
| `Grid` | **矩形网格**：`rows` × `cols` + 顶点表 | **是**（GPU：整格 patch） |
| `TriangleMesh` | 索引化任意三角面 | 否 |
| `TriangleFan` / `TriangleStrip` | 扇形 / 条带 | 否 |

> ⚠️ **`Grid` 是数据描述，不是绘制命令。** OpenGL 没有 grid 这个图元。renderer 负责把它翻译成索引：**每格 6 个索引**（2 个三角形）—— 走细分时喂 `GL_PATCHES`（`triangles` 域），不走细分时喂 `GL_TRIANGLES`。共享顶点靠索引复用，一个都不重复（41×41 从 9600 个展开顶点降到 1681 个）。

---

### 0.2 `Segment` —— 段

**它是什么**：一个生产者一次性产出的一批图元 + 标签。**段是增量更新的原子单位。**

```cpp
struct Segment {
    QObject* owner = nullptr;            // QChartAxis / QChartSeries
    QVector<QChartPrimitive> prims;
    QVector<QChartTextLabel> labels;

    // 运行时字段：本段的 cart* 是否仍是本段内容的有效映像。
    // ★ 新建段一律 false（结构默认值），由 renderer 在变换后置 true。
    // ★ 判据**只能看这个位，不能看 cart 的值**——"没算过"和"算出来恰好是 0/NaN"无法区分。
    bool cartValid = false;
};
```

**`cartValid` 的两个失效来源**（都落到这一个字段上）：

| 失效来源 | 谁置 false |
|---|---|
| 段被重建（内容变了） | layer 新建段时天然为 false |
| 投影变化（映射改了） | 把所有段置 false |

**段内值存储，不用指针。** 指针会引入悬垂（段表活着、owner 已销毁）、失效（owner 的 `QVector` 扩容）、读写交错（渲染读的同时数据写）三类问题。代价是重建该段时一次 vector 拷贝，而重建频次远低于绘制频次。

**段 = 原来“组”的粒度**：

- 轴：**一脊一段**（不是整个网格一段）。
- 系列：**一系列一段**。

> 为什么不能把整个网格合成一段：自由标签靠“同段最后一个可见图元”定位，合成一段后所有网格标签会塌到同一个点上。

**段内值存储，不用指针。** 指针会引入悬垂（段表活着、owner 已销毁）、失效（owner 的 `QVector` 扩容）、读写交错（渲染读的同时数据写）三类问题。代价是重建该段时一次 vector 拷贝，而重建频次远低于绘制频次。

---

### 0.3 `QChartScene` —— 场景

**它是什么**：对“这一次要画什么”的完整声明。**被动产物**，不含帧号、不含**输入**脏标记（"谁脏了"是 layer 的事）；收集的后果落在段的 `cartValid` 上（§0.2），scene 本身不额外记录任何帧状态。

```cpp
struct QChartScene {
    QVector<Segment> axisSegments;      // 轴/网格段（绘制顺序的前半）
    QVector<Segment> seriesSegments;    // 系列段（后半）
    const QChartAbstractCamera* camera = nullptr;
    const QChartAbstractProjection* projection = nullptr;
    QRectF plotArea;
    QColor backgroundColor;
};
```

**轴与系列必须分开成两个数组**，三条理由：

1. 重建触发不同：轴段看脏表的 `axis` 行（相机经轴范围变化、或轴属性变化），系列段看脏表的系列行。
2. 绘制顺序天然要求轴在前。
3. 检索时不需要类型判断。

**不再有独立的 `labels` 数组** —— 标签属于段（见 §2.11）。

---

### 0.4 编号体系

经过“拾取不需要精确到图元级”这一决定之后，编号被压缩到**只剩一种**：

| 名字 | 数什么 | 存在哪 | 谁用 |
|---|---|---|---|
| **段号** | 段在 `seriesSegments` 里的下标（0 .. S−1） | 不存字段，就是下标 | **只有拾取**（GPU 编码成颜色） |

被删除的三个编号概念：

| 删掉 | 原因 |
|---|---|
| `prim.sourceId` | 段的下标就是组号，字段冗余 |
| `scene.groupSources` | 段自带 `owner`，`qobject_cast` 直接问 |
| `prim.dataStart` | 拾取不再细化到数据点，没有消费者 |
| `Segment::firstId` | 段号即下标，不需要全局扁平的起点 |

两条性质写死：

- **段号不跨帧稳定。** 每帧按收集顺序重排，只用于当帧拾取。
- **轴段不编号、不进 ID 帧。** 命中网格/轴一律视为“未命中系列”。

---

### 0.5 `Camera` —— 视图与像素的全部换算

**职责**：Cartesian ↔ 像素，以及 y 朝向翻转。**朝向只允许在这里实现**，CPU 的 `project/unproject` 与 GLSL 用的矩阵必须遵守同一约定。

| 函数 | 说明 |
|---|---|
| `project(cart, plotArea) → {screen, depth}` | Cartesian → 像素 + 视图深度 |
| `unproject(pixel, plotArea) → Ray` | 像素 → 射线（`origin` 在近平面、`direction` 归一化）**已存在** |
| **`unproject(pixel, depth, plotArea) → QVector3D`** | **新增**：像素 + 深度缓冲值 → Cartesian 点（射线只给了两个端点，缺的那一步非线性换算需要 near/far，属相机私有状态） |
| `viewProjectionMatrix(aspect)` | GLSL 用 |
| **`pixelToFramebuffer(pixel, plotArea, deviceSize, dpr) → QPointF`** | **新增**：Qt 顶左逻辑像素 → GL 底左设备像素（`glReadPixels` 用） |

---

### 0.6 `Projection` —— Numeric ↔ Cartesian

| 函数 | 说明 |
|---|---|
| `toCartesian` / `fromCartesian` | 双向映射（后者在球/柱坐标有极点与缝的损失，接受） |
| `isIdentityMapping` | 恒等快路径（决定是否需要细分、是否需要快绘制） |
| `computeViewRect` / `computeDataBounds` | 相机窗口 ↔ 轴范围的换算（2D/3D 各自重载） |

**退役**：`samplingSegmentsHint`。采样密度不再由投影推荐，改由 renderer 按 §2.5 的判据自适应决定，因此投影与**收集**彻底无关。

---

### 0.7 `Renderer` 基类 —— 阶段与交接物

`render(scene, device)` 是一条固定流水线，四个纯虚：

```cpp
void render(QChartScene& scene, QPaintDevice* device);
  ├─ transformNumericToCartesian(scene);   // 纯虚（含细分）
  ├─ cullAndResolveLabels(scene);          // 纯虚
  ├─ drawPrimitives(scene, device);        // 纯虚
  └─ drawLabels(scene, device);            // 纯虚
```

**基类只规定阶段顺序与交接物**（scene 进、像素出），不规定每段怎么做。GPU 后端把其中若干阶段做成空函数是**正确用法**（见 §1.3）。

**不新增内部旗标，也不给 `render()` 加参数。** "要不要重算 cart"由段自己带着（§0.2 的 `cartValid`）：

| 情形 | 变换 + 细分 | 裁剪 | 换程序 |
|---|---|---|---|
| 某段被重建 | **只算该段**（`cartValid == false`） | ✅ 全量 | — |
| 投影变化 | **全部段** | ✅ 全量 | ✅ |
| 仅视图变化（`invalidateView()`） | — | ✅ 全量 | — |
| 都没变 | — | — | — |

两条要点：

- **`cull` 永远全量**，不看 `cartValid` —— 相机/plotArea 一动，所有图元的像素位置都变。谁也别拿 `cartValid` 去跳过裁剪。
- 既有 `m_viewDirty`（`QChartRenderer.h`）保留：视图变了没，由 `invalidateView()` 置位。

---

### 0.8 GL 动态程序组织

GL 版本基线升级到 **4.3 Core**（tessellation 需 4.0+）。

**程序矩阵**（按 `ShaderKind`，共 3 个）：

| 程序 | 阶段 | 投影表达式注入点 |
|---|---|---|
| `Point` | VS + FS | VS |
| `Line` | VS + **TCS + TES** + FS | TES |
| `Triangle` | VS + **TCS + TES** + FS | TES |

**TES 的域只有两种**：线用 `isolines`，**所有面**用 `triangles`。`Grid` 派生的索引是每格 6 个（2 个三角 patch），因此不需要 `quads` 域的第三套 TES —— 代价只是格子内部的斜边保留（今天也有），换来程序矩阵不膨胀。

**拾取不另设程序**：片元着色器带一个 `u_pickMode` 分支 —— 正常时输出 `v_color`，拾取时输出编码后的段号。这样程序数量不因拾取而翻倍。

**缓存的键**：`(ShaderKind, projection)`。投影一变直接换程序（现状已是这个形式，`QChartGL::program(kind, scene.projection)`）。

**共享常量**：细分判据的 `ratio` 与上限放在一个头里，shader 源码用 `.arg()` 注入，不出现魔法数字。

**两个 GL 侧硬前置条件**：

1. `QSurfaceFormat::setDefaultFormat()` 必须在 `QApplication` 构造**之前**调用 —— 否则 Qt 用默认格式建窗口，tessellation 程序编不出来。
2. shader 的 `#version 330 core` → `#version 430 core`。

---

## 第 1 部分 · 管线全貌

两条管线的**交接物完全相同**（同一份 scene、同一份标签、同一套相机约定），差别只在“谁来做”。

### 1.1 CPU 管线（QPainter）：全程 RAM，没有“上传”这一步

```
① 收集(layer)          Data → Numeric，写 axisSegments / seriesSegments
② 变换+细分(renderer)   num → cart 写 cart*；同一趟里按 §2.5 加密线类图元
③ 裁剪(renderer)        cart → pixel，按 plotArea 判，写 prim.visible
④ 上传                 —— 不存在 ——
⑤ 绘制(renderer)        逐图元 cart → pixel，QPainter setPen/setBrush/draw*
⑥ 标签                  锚点 → pixel → QFontMetrics → drawText
```

特征：**没有 VRAM 这一步**；⑤ 每次重绘都要把全部图元重投影成像素，这段开销不吃脏标记。

### 1.2 GPU 管线（OpenGL 4.3）：RAM → VRAM，之后由硬件接管

```
① 收集(layer)          Data → Numeric（与 CPU 同）
② 变换(renderer)       = 空函数，交给 TES/VS
   细分                 = 硬件细分器（TCS 算 level，TES 求点）
③ 裁剪                 = 交给硬件；CPU 侧 visible 一律 true
④ 打包上传(renderer)    CPU 侧做索引派生（Grid → 每格 6 索引，2 个三角 patch；
                        Fan/Strip → 展开成三角形列表）→ 段脏才 glBufferData
⑤ 绘制(renderer)        bind VAO + 设 uniform（u_viewProj / u_pickMode / u_segmentId …）
                        + 3 个 pass（Triangle→Line→Point）× 段顺序
⑥ 标签                  全程 CPU：QPainter 画进 QImage → 合成上屏
```

### 1.3 对照表与三处既定差异

| 关注点 | CPU | GPU |
|---|---|---|
| 坐标变换 | CPU | TES / VS |
| 细分 | **只加密线类**（`Line`/`Path`）；面（Grid/Rect/Polygon/Mesh）一律不加密 | **线 + 所有面**都走硬件细分 |
| 谁判“谁在上面” | 绘制顺序（painter） | 深度缓冲 + `GL_LEQUAL` |
| 几何加工（索引派生/三角化/扇形展开） | 绘制时逐图元 | 段重建时一次，**仍在 CPU** |
| 数据落点 | 全 RAM | RAM → VRAM |
| 标签 | QPainter | QPainter 覆盖层（非 GPU 内文字） |
| 基类可置空的阶段 | 无 | `transformNumericToCartesian`、`cullAndResolveLabels` 的主体 |

**三处既定差异**（都是刻意的、写进文档免得以后当 bug 追）：

1. **GL 不渲染自由标签**（tier3）—— 纯 GPU 后端无能力。
2. **CPU 曲面不细分** —— QPainter 逐三角形开销撑不住，接受粗糙。
3. **CPU 的 3D 拾取坐标是近似** —— 没有深度缓冲，只能射线求交（见 §4.2）。

---

## 第 2 部分 · 逐阶段流程

### 2.1 绘制发起：四个脏变量

| 旗标 | 归属 | 置脏来源 | 消费 |
|---|---|---|---|
| `layoutDirty` | widget | resize / margins / 轴的 alignment·字体·可见性（→ 外边距） | `relayout()` → plotArea |
| `projectionDirty` | widget | `setProjection` | renderer：程序切换；**把所有段的 `cartValid` 置 false** |
| `cameraDirty` | layer | 相机信号、plotArea 变化（fit） | ① renderer：视图变了（`invalidateView()`）→ 裁剪全量；② **轴范围重算** → 域真变了才置脏表的 `axis` 行 |
| `dataDirty` | layer | 系列数据 / 增删 / 可见性 / 颜色 | **脏表的系列行** → 系列段重建 |

**外溢边**（判据：只要 B 的输入里含有 A 的产出，就有一条边；每条边上的**每一跳自己负责置脏**）：

| 边 | 路径 |
|---|---|
| camera → 轴段 | 相机变 → `updateAxes()` → 轴 `setRange` → 刻度变 → 轴图元变 |
| layout → camera → 轴段 | plotArea 变 → 相机 `fitToPlotArea` → 轴范围变 |
| projection → camera → 轴段 | `setProjection` 重算相机窗口 → 轴范围变 |

**三条纪律**：

1. **camera 不直接进收集。** 它只做两件事：通知 renderer 视图变了（`invalidateView()`）；触发轴范围重算。轴范围**真的变了**才动轴段。
2. **layout 变化会让相机 fit**，所以由布局引发的相机变化不需要额外置 `cameraDirty`。`addLayer` 与 `layoutDirty` 无关（层必在 plotArea 内）。
3. **projection 不碰收集。** 因为轴与系列吐的都是 Numeric 直线，加密归 renderer。

#### 2.1.1 从输入到重建：两层

输入旗标描述的是**"哪种输入变了"**；真正决定 renderer 干什么的是**"哪几段内容要重建"**。两者之间隔着一张表：

| 层 | 内容 | 归属 | 谁看 |
|---|---|---|---|
| **输入层** | 四个旗标 | widget / layer | 决定"要不要重算域、要不要重裁" |
| **后果层** | **脏表**：`axis` 一行 + 每个 series 一行 | layer | 决定"哪几段内容要重建" |
| **后果的落地** | `Segment::cartValid` | 段自己 | renderer 决定"哪几段重算 cart" |

- **`dataDirty` 不再是一个布尔**，它就是这张脏表（表非空 ⇔ dataDirty 非空）。这就是"把 dataDirty 下放到系列"的确切含义，**且不需要给 `QChartSeries` 加字段**——dirty 是接收方的状态，系列已经用信号表达了"我变了"。
- **轴只要一行**：所有脊永远同建同弃（域变全变、任一轴属性变也全变），不需要按脊细分。
- **轴属性变化（可见 / 样式 / 刻度 / 对齐）在四个输入旗标里没有位置**，它的正位就是"把脏表的 `axis` 那一行置 true"。以前这条糊在 `cameraDirty` 里，现在归位。

**输入 → 脏表 → 后果**：

| 输入变化 | 脏表 | `cart` | `cull` |
|---|---|---|---|
| `camera` 变 | 重算域 → **域真变了**才置 `axis` | 仅轴段 | 全部 |
| `camera` 变但域没变（如 3D orbit） | 不清任何行 | 无 | 全部 |
| 轴属性变 | 置 `axis` | 仅轴段 | 全部 |
| `data` 变（某系列） | 置该系列行 | 仅该段 | 全部（新图元未裁） |
| `layout` 变 | 经 fit 走 camera 那条 | — | 全部 |
| `projection` 变 | **不清任何行** | **全部**（映射改了） | 全部 |

最后一行是唯一"cart 失效 ≠ 段重建"的情形，单独记住。

**不变量**：**脏表非空 ⇒ 必须有一帧 paint 在排队。** 否则就是"数据改了、表里躺着、没人来画"（实测踩过：改数据后 10 秒零收集）。所以"置脏即请求重绘"不是补丁，是这张表的配套条件——表是待办，paint 是来办。

---

### 2.2 图元收集

**谁收集**：

| 内容 | 谁 | 落进哪个数组 |
|---|---|---|
| 轴脊、刻度点、网格脊 | layer 代管（调 `axis->drawAtPosition`） | `axisSegments` |
| 系列图元 | 系列自己的 `collectPrimitives` | `seriesSegments` |

**提交格式**：`QChartPrimitive`（纯 Numeric）+ `QChartTextLabel`（意图，见 §2.11）。

**汇总**：layer 把段塞进 `scene.axisSegments` / `scene.seriesSegments`。

**分段重建的规则**：

1. 先轴段、后系列段（保证两个数组各守其位）。
2. **只重建脏表里标了的行**，没标的一个字节不动。
3. **重建 = 新段对象 ⇒ `cartValid` 自然为 `false`**（结构默认值，不靠人记得清）。
4. 系列段重建后**按 `m_series` 顺序归并一次**（O(S)，S 是系列数）：缺的补、多余的删、顺序对齐。这样 `addSeries` / `removeSeries` 走同一条路，不需要额外的"结构脏"旗标。
5. 段数变化只影响数组项数，**不涉及任何平移**（段是独立项，不是一段连续内存）。

**调用序（时序对齐的唯一依据）**：

```
widget.paintEvent
  → pushContextToLayers()             // 注入 plotArea / camera / projection
  → layer.ensureSceneCollected()      // ① 收集：按脏表重建相应段
  → renderer->render(scene, device)   // ② 绘制：只读段，只写 cart* / visible / cartValid
```

- **widget 通知 layer 准备数据，renderer 只读。** renderer 不反向依赖 layer / widget。
- 收集与绘制**同一帧内不重叠**，所以"边改边画"从结构上不可能发生——也就不需要任何并发保护。

---

### 2.3 编号赋予

**没有需要“赋予”的编号了。** 段号 = 它在 `seriesSegments` 里的下标，由数组位置天然决定；段内图元不需要编号。

举例：轴 2 脊 + 3 个系列

```
axisSegments   [0]=脊A  [1]=脊B                  ← 不编号、不进 ID 帧
seriesSegments [0]=系列X  [1]=系列Y  [2]=系列Z   ← 段号 0/1/2
```

拾取命中系列 Y → 段号 1 → `seriesSegments[1].owner` → 系列 Y。**没有 id、没有偏移、没有查表。**

---

### 2.4 动态程序编译

- 触发：`(ShaderKind, projection)` 未命中缓存时编译并缓存。
- 投影切换：换程序（不是重编），旧程序保留在缓存里备切回。
- 拾取：不换程序，只把 `u_pickMode` 置 true。
- 常量注入：细分判据的 `ratio` / 上限用 `.arg()` 写进源码，保证 C++ 与 GLSL 用的是同一份数字。

---

### 2.5 细分（多采样）

**判据（纯 Cartesian，与相机无关）**：

```
c0 = toCartesian(n0);  c1 = toCartesian(n1);  cm = toCartesian((n0+n1)/2)
d  = |cm − (c0+c1)/2|
L  = |c1 − c0|
若 L > ε 且 d / L > ratio → 二分递归，深度上限 kMaxLevel
```

三个性质：

- 只用 `toCartesian`，**不碰 camera / plotArea** → 细分结果与视图无关。
- 与尺度无关：10 单位长和 0.01 单位长的线段得到同样合理的密度。
- 恒等投影下第一层就通过（`d ≡ 0`），可用 `isIdentityMapping` 直接短路。

**上限**：圆心附近这类局部曲率爆炸处，任何固定密度的多边形逼近都会露馅 —— 那是映射本身的缺陷，不该靠无限加密掩盖。所以必须有级数上限（建议 CPU 5 层 = 32 段，GPU 用硬件上限）。

**两条落点**：

| 后端 | 在哪 | 产物 |
|---|---|---|
| CPU | 变换阶段（`transformNumericToCartesian` 内） | 加密后的 `cartVerts` |
| GPU | TCS 算 level、TES 生成点 | 顶点由硬件产生，**VBO 里只存控制点** |

**失效条件只有 `{段内容, 投影}`** —— 相机完全不参与。因此：

- GL 的顶点数由硬件决定，**VBO 缓存键不含任何视图成分**；
- 相机拖动时，VBO 与细分结果一个字节都不用动。

**参与范围**：

| 形态 | CPU | GPU |
|---|---|---|
| `Line` / `Path` | 加密 | 加密（`isolines` 域） |
| `Grid` | **不加密**（CPU 曲面不细分，既定差异） | 加密（`triangles` 域，每格 2 个三角 patch） |
| `Rect` / `Polygon` | **不加密**（接受非线性投影下的直边） | 加密（`triangles` 域） |
| `TriangleMesh` / `Fan` / `Strip` | 不加密 | 加密（`triangles` 域） |

---

### 2.6 坐标变换

| 后端 | 做什么 |
|---|---|
| CPU | 逐顶点 `projection->toCartesian(num)` → 写 `cart*`（细分在同一趟里完成） |
| GPU | **空函数**。`cart = glslToCartesian(num)` 在着色器里算（Point 在 VS、Line/Triangle 在 TES） |

同一套投影的数学存在 C++ 与 GLSL 两份，这是既定事实（着色器无法回调 C++）。一致性靠“同一份常量 + 对照 demo”保证，不做自动对齐测试。

---

### 2.7 裁剪

- **只标 `visible`、永不删元素。** 删了段内下标就抖，拾取与标签组尾全部失效。
- CPU：图元包围盒 vs plotArea（经相机变换后），写 `prim.visible`。
- GPU：`visible` 一律 true，真裁交给硬件。
- **顺序：先裁剪、后细分。** 但细分结果按图元缓存（随段内容/投影失效），所以“先裁后细”不会把细分绑回相机。
- 已知边界误差：外凸曲线在视图边缘可能被切掉最宽约 `ratio × 弦长`（约 0.5%）的一条细边，接受。

---

### 2.8 VBO 打包与上传

**引入索引绘制**（现状完全没有 `ELEMENT_ARRAY_BUFFER`，全是展开顶点）。

| 形态 | 索引派生 |
|---|---|
| `Grid` | 每格 6 索引（2 个三角 patch）；走细分喂 `GL_PATCHES`，不走细分喂 `GL_TRIANGLES` |
| `TriangleMesh` | 直接用 `numIndices` |
| `TriangleFan` / `TriangleStrip` | **打包阶段展开**成三角形列表（见 §5 待办） |
| `Polygon` | 三角化后按索引 |
| `Rect` | 4 角 → 2 三角形（6 索引）或 6 顶点直传 |

**批次归属段，绝不跨段合批。**

```
struct GLBatch {
    GLuint vao, vbo, ibo;
    GLenum primitive;        // GL_TRIANGLES / GL_LINES / GL_POINTS / GL_PATCHES
    GLsizei indexCount;
    int patchVertices;       // 0 = 非 patch；2 = 线 patch；4 = 网格 patch
    ShaderKind shaderKind;
    float pointSize, lineWidth;
};

struct SegmentBatches { QVector<GLBatch> batches; bool dirty = true; };
QVector<SegmentBatches> m_segmentBatches;   // 与 scene 的段一一对应
```

**重建时机**：

| 变化 | 动作 |
|---|---|
| 相机动 | **零**。只更新 `u_viewProj` |
| 某系列数据变 | 只重建该段的批次 |
| 轴范围变 | 只重建轴段的批次 |
| 加/删系列 | 增删对应段与其批次项 |

**代价**：不再跨段合批，draw call 从“约 5 个”变成“段数 × 每段 kind 数”（20 系列约 40 次，可接受；200 系列需重新考虑）。

---

### 2.9 绘制顺序

```
for pass in { Triangle, Line, Point }:        // 外层：着色器程序
    for segment in 段顺序:                     // 内层：轴段在前，系列段在后
        for batch in segment 中属于本 pass 的:
            glPatchParameteri(...)（若是 patch）
            draw
```

两个顺序是交织的，规则写死：**外层 pass，内层段序。**

---

### 2.10 深度与偏置

**不设任何偏置。** 深度该是什么就是什么。

| 项 | 结论 |
|---|---|
| `QChartPrimitive::depth` | **删**（GL 用硬件深度缓冲；CPU 3D 排序每帧现算，存字段等于存一个立即过期的缓存） |
| `depthBias` | **删**（ε 魔法数） |
| `isGrid` / `isDecor` | **删**（当年是为了“网格微靠前”“装饰不测深度”而靠 `depth` 猜出来的补丁；GL 从不写 `depth`，所以这两个判据在 GL 下**恒为 false**，早已是死代码） |
| 同深度谁赢 | `glDepthFunc(GL_LEQUAL)` + 段顺序 → **后画的赢**，与 CPU painter 的“后来者覆盖”同一语义 |

**2D 没有深度可言。** 2D 的投影不产生 z 差异，所有图元深度相同，“谁在上面”完全由**绘制顺序**决定；3D 才是真深度。所以：

- **2D**：不需要造一个假深度，也不需要任何特判 —— 所有图元 z 相同，`GL_LEQUAL` + 段顺序自然得到“后画的赢”，与 CPU painter 的顺序语义完全一致。（哪天想绕开深度测试直接关掉它，结果一样。）
- **3D**：深度由 `u_viewProj` 真实给出，`GL_LEQUAL` 只负责解决同深度平手；CPU 的 painter 排序在同深度时按段序。

一句话：**深度只在 3D 里有意义；2D 的层级就是绘制顺序。**

真共面时没有正确答案，硬塞 ε 只会让问题换个形式出现 —— 那种情况应由图表设计避免。

---

### 2.11 文字标签

**组织**：标签放进 `Segment`，与图元一起重建。**不再有独立的 `labels` 数组。**

| 项 | 依赖 | 重算时机 |
|---|---|---|
| 生成与绑定（文字内容、锚在哪个段、段内哪个图元） | 段内容 | **随所属段的脏**（轴段看 cameraDirty、系列段看 dataDirty） |
| 落位（`numericAnchor → cartesianAnchor → 像素`） | 投影 + 相机 | **每次重绘**算一遍（几十 µs，比 `drawText` 便宜三个数量级，不值得缓存） |

**三级绑定，一律不引用全局编号**：

| tier | 判定 | 锚点 |
|---|---|---|
| tier1 显式锚点 | `numericAnchor` 有非 NaN 分量 | 直接投影 |
| tier2 绑定图元 | 锚点全 NaN 且 `segPrim >= 0` | 该段内第 `segPrim` 个图元 |
| tier3 自由标签 | 锚点全 NaN 且 `segPrim == -1` | 该段**最后一个可见图元**的组尾锚点 |

> 现状 `QChartTextLabel::refPrimitiveId` 存的是**场景数组下标**（`QPainterChartRenderer.cpp:119` 直接 `scene.primitives[...]`），段化之后这个下标会变，必须换成段内偏移（`segPrim`）。

**避让迟滞的键**：从“文字 + 字号”改成“`owner` 指针 + 文字”，更稳定。

**既定差异**：GL 后端不渲染 tier3（纯 GPU 无能力）。

---

## 第 3 部分 · 契约

- **S1** 段序 = 绘制顺序；`axisSegments` 恒在 `seriesSegments` 之前。
- **S2** 段是更新的原子单位：重建一段不得影响其它段的图元与标签。
- **S3** 段内图元同属一个 `owner`，且最多两种 `ShaderKind`（仅两个例外：轴段、`QPolygonSeries` 的描边段）。
- **S4** 变换/细分 由 `Segment::cartValid` 驱动（失效来源只有两个：**段重建**、**投影变化**）；裁剪由 {相机, plotArea} 驱动，且**永远全量**，不得用 `cartValid` 跳过。
- **S5** 细分只改“供绘制的几何”，**不改段结构、不改图元数量、不改段号**。
- **S6** `visible` 只标 `false`，**永不删元素**。
- **S7** `m_segmentBatches` 与 scene 的段**一一对应**（数量与顺序同步）。
- **S8** 标签随所属段一起重建；绑定用段内偏移或段尾，**不引用段号以外的编号**。
- **S9** 渲染器与拾取器**不得改变段与图元的数量、顺序、所有者**（结构只读）；渲染器**可以写**运行时字段：`seg.cartValid`、`prim.cart*`、`prim.visible`、标签的 `cartesianAnchor` / `visible` / `pixelpos`；**拾取器一个字段都不写**。
- **S10** 拾取的编号 = `seriesSegments` 下标；**不跨帧稳定**；轴段不编码。
- **S11** `cartValid` 是段的运行时位：layer 建段时为 `false`，renderer 算完 `cart*` 置 `true`；投影变化必须把**所有段**置 `false`。判据只能看这个位，**不许拿 `cart` 的值去猜"算没算过"**（`0` / `NaN` 都可能是合法结果）。

---

## 第 4 部分 · 拾取

### 4.1 `HitResult`

```cpp
struct HitResult {
    QChartSeries* series = nullptr;                 // 空 = 未命中（含命中装饰）
    QVector3D cartesian{Q_QNAN, Q_QNAN, Q_QNAN};    // 命中点（Cartesian）；未命中 = 全 NaN
};
```

- **只给 Cartesian，不给 Numeric。** Cartesian 两后端都能无损失给出；Numeric 需要 `fromCartesian`，而在球/柱下是有损的（接受损失，但把这一步留给调用方，不埋进拾取）。
- **不细化到图元、不细化到数据点。** 因此没有 `dataIndex` 字段。

### 4.2 CPU 路径（近似）

```
hitTest(pixel, scene)
  从后往前遍历系列段（顶层优先）：
    从后往前遍历段内图元：
      visible == false → 跳过
      cart* → 像素（camera->project，与绘制同一条）
      按 type 判覆盖（Point 距离 / Line·Path 点到线段 / Rect·Polygon 包含 / 网格任三角形包含）
      命中 → series = 段.owner
  坐标：
    2D → camera->unproject(pixel).origin（唯一确定）
    3D → 射线与该图元几何求最近点（§4.1 的近似来源）
```

### 4.3 GPU 路径（精确）

**ID 帧 = 颜色附件（RGBA8，存段号）+ 深度附件（D24）**，一次 pass 同时产出“是谁”和“多深”：

```
pickIdAt(pixel)
  若 ID 帧脏 → 用同一批 VBO、同一套变换、开深度测试画一趟
  1×1 回读两样：
    RGB   → 段号 → seriesSegments[段号].owner
    DEPTH → camera->unproject(pixel, depth) → Cartesian 点
```

- **编码的是段号，不是图元号。** 所以不存在“批次重排导致 id 置换”“多顶点图元被切成多个 id”这些问题 —— 段内所有图元共用一个号。
- 一次光栅化同时给出 id 与深度，天然自洽，不依赖主绘制帧的生命周期。
- **ID 帧的失效条件 = {段重建, 视图变化, 投影变化}**：它画的是“位置”，所以相机一动就得重画（这与“VBO 段脏才重建”不是一回事，别混）。
- 轴段不进 ID 帧（既定），因此**被 3D 盒边/网格线挡住的图元仍可能被拾中**（薄线遮挡，接受）。

### 4.4 归属与分派

- `QChartAbstractWidget` 持有 `std::unique_ptr<QChartHitTester> m_hitTester`，与 renderer **成对创建**：CPU 后端 → `QPainterHitTester`（不依赖 renderer）；GL 后端 → `QOpenGLHitTester`（持 renderer 指针，调 `pickIdAt`）。
- widget 只做一次转发：`hitTest(pixel, layer->scene())`。
- **后端开关同时决定渲染与拾取**，禁止混搭。

---

## 第 5 部分 · 已定 / 待定 / 明确不做

### 已定

| 项 | 结论 |
|---|---|
| 脏变量 | 四个（layout/projection 归 widget，camera/data 归 layer）；**renderer 不新增任何旗标** |
| 部分更新 | `dataDirty` 细化成**脏表**（layer 持 `axis` 一行 + 每系列一行）；后果落在 `Segment::cartValid` 上；**不给 `QChartSeries` 加字段** |
| 时序 | widget 通知 layer 收集 → renderer 只读；收集与绘制同帧不重叠，无需并发保护 |
| 图元组织 | 段化，值存储；`axisSegments` / `seriesSegments`（均已落地） |
| 编号 | 只剩段号（`seriesSegments` 下标），不跨帧稳定 |
| 细分 | 判据纯 Cartesian、上限、CPU 只加密线类 |
| GL | 4.3；3 个程序 + `u_pickMode`；索引绘制；段级批次 |
| 深度 | 删字段删偏置，`GL_LEQUAL` + 段序 |
| 标签 | 进段；三级绑定；生成随内容、落位随重绘 |
| 拾取 | `HitResult{series, cartesian}`；GPU 读 id + 深度；CPU 近似 |
| 曲面 | `Type::Grid`（数据描述），renderer 派生索引 |

### 基线实测（line / line3d × cpu / gl，70 秒，含 resize / 拖动 / 滚轮）

| 现象 | 数据 |
|---|---|
| 部分收集 = **0%** | 29 / 28 / 44 / 121 次收集，**全部** `FULL rebuild`；系列段在任何相机变化下都被重收 |
| 变换白跑 = **100%** | transform 次数与收集次数相同，而系列段 cart 校验和全程**只有一个值** |
| `layout` 链是通的 | `resizeEvent` → `relayout` → plotArea `0x0 → 632x477` → 立即走完全链 |
| **数据变更不重绘** | 改 series 数据后 10 秒零收集（CPU 两次）；GL/2D 躺了 3 秒被鼠标捎带出来 —— `invalidateData()` 只置旗标，无人 `scheduleRepaint()` |
| 3D 脏源重复 | `rangeChanged → invalidateData` 与视图指纹同时存在，同一变化被记 2~3 次 |
| 内容指纹无效化 | `contentKey` 折进了 `contentRevision`，于是"只要 invalidate 过就必然变化"，拦不住重复收集 |

### 待定 / 待办

1. **脏变量落地（下一轮，按上表基线逐条消）**：
   - `collectPrimitives` 拆成 `collectAxisPrimitives()` / `collectSeriesPrimitives()`，由脏表驱动；
   - 系列段重建后按 `m_series` 顺序归并（O(S)）；
   - `transform` 判据改为 `!seg.cartValid`；投影变化时全体置 `false`；
   - **重绘收口**：`invalidate*` → 请求重绘（表非空必有 paint 排队）；
   - 3D 去掉 `rangeChanged → invalidateData` 的重复接线；
   - 内容指纹剔除 `contentRevision`；
   - 复测同样四个程序，出对照表。
2. **Fan/Strip 打包目前是错的**：现在当三角形列表用，5 顶点只画出 1 个三角形。要展开成三角形列表，等动 `uploadBatches` 那一轮一起修（那时有 demo 能验）。
3. **`m_segmentBatches`、两个相机函数、`QChartHitTester` 重写** 均为新增/改动，实施前逐项请示。
4. 逐点样式（现在 markerSize/penWidth 整段统一）：一旦允许逐点不同，段内会按样式裂成多批 —— 属后续。
5. 自由标签的 GL 支持（当轮评估过，定为既定差异，不主动扩大范围）。

### 明确不做

| 不做 | 理由 |
|---|---|
| CPU 位图缓存 | 触发重算的来源太多，命中率低 |
| “全都没变就跳过”快路径 / 计数器 | 只作排查手段，不留代码 |
| 段内指针 / 生产者常驻持有数组 | 悬垂、失效、读写交错 |
| 段内单图元更新 | 段是原子单位 |
| 深度偏置 | ε 魔法数，掩盖真问题 |
| CPU 曲面细分 | QPainter 逐三角形撑不住，接受粗糙 |
| 轴段拾取 | 无消费者；轴段不进 ID 帧 |
