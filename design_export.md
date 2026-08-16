# 导出（PNG / SVG / PDF）设计（Phase 1）

> 状态：定稿（拆分稿，待用户确认）。本文件只覆盖导出；主题/调色板见 `design_theme.md`（§2 override 颜色模型、backgroundColor 来源），图例见 `design_legend.md`。
> 决策依据：t5 问卷 C1=a, C2=a, **C3=a**, C4=a, C5=a。
> 特别标注：C3 用户最终确认为「默认导出全 widget（所见即所得，含边距/轴/刻度标签）」；「仅 plotArea」作为可选模式保留，并标注会丢刻度标签。

## 0. 用户已确认的决策速览（导出）

| 题 | 决策 |
|---|---|
| C1 | 导出 API = widget 便捷方法 saveAsPng/Svg/Pdf |
| C2 | 矢量导出 = 临时禁用缓存、直接绘制（真矢量） |
| C3 | 导出范围 = **默认全 widget（所见即所得）**，可选「仅 plotArea」（会丢刻度标签） |
| C4 | PNG 尺寸 = 显式 QSize + 可选 DPI，缺省当前尺寸 |
| C5 | 导出背景 = 主题背景色填充，另提供「透明背景」开关 |

> 定稿决议（与导出相关）：
> 1. **去掉 `plotAreaColor` 独立字段**——plotArea 永远跟随画布背景（无单独填充色）。
> 2. **WholeWidget + 自定义 size 等比重算：保留**（抽 `plotAreaForSize(QSize)` 复用 margin 逻辑）。
> 3. **SVG 依赖已就绪**：Linux `qt6-svg-dev` 6.4.2 已装；Windows 侧 Qt 需包含 SVG 模块（用户侧确认）。

## 1. 背景：QChartScene 是什么

`QChartScene` 不是本 Phase 新引入的概念——它是 Phase 0 已存在的「场景快照」结构体（现定义于 `QChartRenderer.h:18`，当前名为 `ChartScene`）：渲染器的入参。`QChartWidget` 在 `paintEvent` 里组装好一份快照（`plotArea / dataBounds / viewRect / projection / axes / layers`），交给 `renderer->render(scene, device)` 消费。

Phase 1 对它的改动只有两点：① **纯改名** `ChartScene → QChartScene`（字段与语义不变，不破坏 moc/构建）；② **加字段**（主题加 `backgroundColor`；图例加 `legend / legendItems`）。导出不是新类，只是把这份快照通过 `renderUncached` 画到「图片 / SVG / PDF」等任意 `QPaintDevice` 上。

## 2. 导出设计

### 2.1 渲染层改造（QChartScene / QChartRenderer，C2 的关键钩子）

`QChartScene` 增加字段（改名 + 加字段，见 §1）：

```cpp
struct QChartScene {
    // ……原有 plotArea/dataBounds/viewRect/projection/axes/layers 不变……
    QColor backgroundColor;               // 画布底色（invalid = 不填充，即透明）
    QChartLegend* legend = nullptr;
    QList<QChartSeries*> legendItems;
};
```

`QChartRenderer` 增加一个「无缓存渲染」入口（C2 核心钩子）：

```cpp
// 直接绘制到 device，不读写内部 QPixmap 缓存。
// 用途：导出（PNG/SVG/PDF）——避免矢量 device 被栅格化，且避免污染屏显缓存。
virtual void renderUncached(const QChartScene& scene, QPaintDevice* device) = 0;
```

`QPainterChartRenderer` 实现要点：
- 抽出私有 `void drawDirect(QPainter*, const QChartScene&)`（= drawBackground + drawForeground）。
- `render()` 的 `!m_cachingEnabled` 分支与 `renderUncached()` 都调用 `drawDirect`。
- `drawBackground` 改为：`fillRect(整 device 矩形, scene.backgroundColor)`（valid 才填）——plotArea 无单独填充，跟随画布背景。
- `drawForeground` 末尾画图例（见 `design_legend.md` §3）。

### 2.2 QChartWidget 导出 API（C1/C3/C4/C5）

```cpp
enum class QChartExportScope { WholeWidget /*默认*/, PlotArea };

// 便捷重载（默认 WholeWidget；C3=a）
bool saveAsPng(const QString& path, const QSize& size = {}, qreal devicePixelRatio = 1.0);
bool saveAsSvg(const QString& path, const QSize& size = {});
bool saveAsPdf(const QString& path, const QSize& size = {});

// 显式范围重载（C3：默认全 widget；「仅 plotArea」可选）
bool saveAsPng(const QString& path, QChartExportScope scope, const QSize& size = {}, qreal devicePixelRatio = 1.0);
bool saveAsSvg(const QString& path, QChartExportScope scope, const QSize& size = {});
bool saveAsPdf(const QString& path, QChartExportScope scope, const QSize& size = {});

// 透明背景开关（C5：默认 false = 用主题背景填充）
void setExportTransparentBackground(bool);  bool exportTransparentBackground() const;
```

语义：
- **size 缺省**：`WholeWidget` → 控件当前尺寸；`PlotArea` → plotArea 当前像素尺寸。
- **PNG 的 devicePixelRatio（C4）**：输出像素 = `size * devicePixelRatio`（写进 QImage 的 dpr）。
- **所有导出统一走 `renderUncached`**：一次性绘制，既不把矢量栅格化（C2），也不读写屏显 QPixmap 缓存（避免导出尺寸 ≠ 屏显尺寸导致的缓存反复重建）。

**范围 → 设备与场景映射**（C3 核心语义）：

| scope | 设备尺寸 | scene.plotArea | 背景填充 |
|---|---|---|---|
| WholeWidget（默认） | 控件尺寸（或 size，等比缩放 plotArea） | 按目标尺寸重算的 plotArea（复用 layoutAxes 的 margin/sizeHint 逻辑，抽 `plotAreaForSize(QSize)` 复用） | 填 `backgroundColor`（整设备） |
| PlotArea | plotArea 尺寸（或 size） | `QRectF(0,0,w,h)`（整设备） | 填 `backgroundColor`（整设备矩形） |

- **WholeWidget（默认）= 所见即所得**：含背景、边距、边框轴与刻度标签（画在 plotArea 外 margin）、图例。
- **PlotArea 模式明确标注（可选，用户知情的选择）**：边框轴刻度标签画在 plotArea 之外 margin（QChartAxis.cpp labelDir 朝外），该模式会**丢失刻度标签与轴标题**，只保留数据区内容；图例因在 plotArea 内 overlay，**仍会出现在导出中**。
- **C3+C5 交叉点**：plotArea 无独立填充，永远跟随画布背景；默认全 widget 与「仅 plotArea」两种模式下，背景都统一填主题 `backgroundColor`（「仅 plotArea」时填整个设备矩形＝plotArea 矩形，保证不透明/非黑底）。
- **透明背景开关（C5）**：开启时导出构建的 scene 里 `backgroundColor` 置 invalid → PNG 得到 alpha 透明、SVG 不含背景矩形；PDF 无 alpha，忽略该开关、始终填充背景（文档注明）。

**各格式落地**：
- PNG：`QImage(size*dpr)`，setDevicePixelRatio(dpr)，`renderUncached`，`image.save(path,"PNG")`。
- SVG：`QSvgGenerator` setSize/setTitle，`renderUncached`（矢量 path，不含栅格）。依赖 `Qt6::Svg`（见 §3）。
- PDF：`QPdfWriter` setPageSize(QPageSize)，`renderUncached`。

## 3. 文件与构建（本功能部分）

- 无新增源文件（导出为 `QChartWidget` 便捷方法 + 渲染器改造；`QChartScene` 改名与字段见 §1）。
- CMakeLists.txt 改动（**不动 target 结构 / 不动 moc 所有权约定**）：
  1. `find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg)` —— 新增 `Svg`（QSvgGenerator 需要）。
  2. `target_link_libraries(QChartWidget PUBLIC ... Qt6::Svg)` —— 追加 `Qt6::Svg`。

✅ 环境已就绪：Linux 侧 `qt6-svg-dev` 6.4.2 已安装（`/usr/lib/x86_64-linux-gnu/cmake/Qt6Svg` 已就位）；Windows 侧 Qt 需包含 SVG 模块（用户侧确认）。PNG/PDF 只依赖 QtGui，不受影响。

## 4. 测试计划（可独立断言）

遵循既有 TestUnit 约定：`tests/test_export.h`（Q_OBJECT 类声明）+ `.cpp` 实现，接入 `CMakeLists`（QCHART_TEST_HEADERS + QChartTests 源）与 `TestUnit/main.cpp`（include + `QTest::qExec` 一行）。测试可断言，不靠肉眼（QGuiApplication + offscreen）：

### 4.1 导出（test_export）
- **PNG 尺寸/背景（C4/C5）**：`saveAsPng(path, {320,240}, 2.0)` → 文件存在，`QImage` 尺寸 `640x480`；**角落像素 == backgroundColor**（填充断言）；非背景像素存在（内容确实画了）。
- **默认范围（C3=a）**：`saveAsPng(path)`（默认 WholeWidget）→ 尺寸 == 控件尺寸；`saveAsPng(path, QChartExportScope::PlotArea)`（显式传入）→ 尺寸 == plotArea 尺寸。
- **SVG 真矢量（C2）**：`saveAsSvg` 后读文件文本，断言含 `<svg`、含 `<path`（矢量路径）、**不含** `<image`/PNG base64（证明未栅格化）。
- **PDF**：`saveAsPdf` 后文件以 `%PDF-` 开头。
- **透明开关（C5）**：`setExportTransparentBackground(true)` + PNG → 角落像素 alpha==0；SVG 文本不含背景矩形（或不含背景 fill 色）。

## 5. 实现任务拆分（本功能部分，逐 task reviewer 审查）

> 排期更新（用户决定）：导出**合并为一个实现任务 + 一个审查任务**——`task-export`（renderUncached 钩子 + QChartExportScope + saveAsPng/Svg/Pdf + CMake Svg + §4 全部测试）+ `rev-export`（合并审查）。原拆分如下仅供参考：
> 每个 task 完成即交 reviewer（D9），通过才进下一个。

1. **task-export-png**：**首步把 `ChartScene` 重命名为 `QChartScene`（纯改名，若尚未由其它功能完成）**；然后 `renderUncached` 钩子 + `QChartExportScope`（WholeWidget 默认 / PlotArea 可选）+ `saveAsPng`（size/dpr、背景/透明）；单测 PNG 尺寸/背景/范围。依赖：task-theme-widget（背景填充，见 `design_theme.md` §7）+ task-legend-draw（图例入导出，见 `design_legend.md` §6）。
2. **task-export-vector**：`saveAsSvg` / `saveAsPdf`（QSvgGenerator/QPdfWriter，走 renderUncached 真矢量）；CMake 加 Qt6::Svg；单测 SVG 含 path 不含 image、PDF 头。依赖：task-export-png。
3. **task-phase1-demo**：新增/改造 1 个深色模式 demo（`setTheme(QChartTheme::Preset::Dark)` + 图例 + 一键导出示意），7 个旧 demo 冒烟无回归。依赖：task-export-vector。

> 说明：Phase 1 整体最终验收（task-phase1-verify，干净构建 + 全量 ctest + demo 冒烟 + 三格式导出产物复核 + 深色无回归）由主对话统一调度，不隶属于任一功能文档。
