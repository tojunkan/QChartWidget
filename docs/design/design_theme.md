# 主题 / 调色板 设计（Phase 1）

> 状态：定稿（拆分稿，待用户确认）。本文件只覆盖主题/调色板；图例见 `design_legend.md`，导出见 `design_export.md`。
> 决策依据：t5 问卷 A1=b, A2=a, A3=a, A4=a, A5=a。
> 说明：§2 的 override 颜色模型是主题的公共机制，图例文字色/导出背景色也复用，故定义在本文件，其余两份文档引用之。

## 0. 用户已确认的决策速览（主题）

| 题 | 决策 |
|---|---|
| A1 | 主题 = 框架色 + 系列调色板（未显式设色的系列自动取色） |
| A2 | API = 预设枚举 + 逐项覆盖（不是「结构体一次性赋值」） |
| A3 | 主题只当默认值，用户显式设过的颜色优先保留 |
| A4 | 手动为主 + 可选「跟随系统调色板自动切换」开关 |
| A5 | 系列调色板按 addSeries 顺序循环取色 |

> 定稿决议（与主题相关）：**去掉 `plotAreaColor` 独立字段**——plotArea 永远跟随画布背景（`backgroundColor`）。

## 1. 背景：QChartScene 是什么

`QChartScene` 不是本 Phase 新引入的概念——它是 Phase 0 已存在的「场景快照」结构体（现定义于 `QChartRenderer.h:18`，当前名为 `ChartScene`）：渲染器的入参。`QChartWidget` 在 `paintEvent` 里组装好一份快照（`plotArea / dataBounds / viewRect / projection / axes / layers`），交给 `renderer->render(scene, device)` 消费。

Phase 1 对它的改动只有两点：① **纯改名** `ChartScene → QChartScene`（字段与语义不变，不破坏 moc/构建）；② **加字段**（本功能加 `backgroundColor`；图例功能加 `legend / legendItems`）。它不是新类，只是给既有快照补颜色与图例信息。

## 2. 公共约定：颜色「显式设色优先」override 模式（A3）

现状三处颜色是「单值字段 + setter 直接赋值」，无法区分「用户显式设过」与「默认值」：
- `QChartAxis::m_color`（默认 `Qt::black`）
- `QChartLayer::m_gridColor`（默认 `QColor(220,220,220)`）
- `QChartSeries::m_color`（默认 `QColor()` 无效 = 现行为黑）

统一改为「**主题默认值 + 可选覆盖**」双槽模型，getter 语义不变（绘制代码零改动）：

```cpp
// 以 QChartAxis 为例（QChartLayer::m_gridColor / QChartSeries::m_color 同构）
std::optional<QColor> m_colorOverride;  // 用户显式设过（setColor）
QColor                m_themeColor;     // 主题注入的默认值（setThemeColor，内部）

QColor color() const { return m_colorOverride.value_or(m_themeColor); } // getter 不变

void setColor(const QColor& c) { m_colorOverride = c; emit colorChanged(); }        // 用户显式设色
void setThemeColor(const QColor& c) {                                               // 主题注入（内部）
    m_themeColor = c;
    if (!m_colorOverride) emit colorChanged();                                      // 仅当无 override 才真正变化
}
void clearColor() { m_colorOverride.reset(); emit colorChanged(); }                 // 回到主题默认
std::optional<QColor> colorOverride() const { return m_colorOverride; }             // 供调色板判断
```

判定机制（A3 的「显式设色优先」）＝ `override.value_or(themeDefault)`：
- 没显式设 → 主题默认生效；切主题 → 默认值变化 → 自动重绘。
- 显式设过 → 永远盖过主题；切主题不覆盖；`clearColor()` 手动回退到主题默认。

注意（已有能力，不改语义）：`QChartSeries` 的 `Q_PROPERTY(color ... WRITE setColor)` 是 QPropertyAnimation 的动画目标（D3）。改模型后 `setColor` 写 override，动画照常工作；动画结束得到的颜色成为「显式覆盖」，符合直觉。

## 3. 类型与 API

### 3.1 QChartTheme

```cpp
// QChartTheme.h（无 Q_OBJECT——纯数据/调色板）
struct QChartTheme {
    enum class Preset { Light, Dark };   // 预设枚举（并入 QChartTheme，无裸 ChartTheme 名）

    QColor backgroundColor;   // 画布底色（整控件矩形）；plotArea 跟随此色，无独立填充
    QColor gridColor;
    QColor axisColor;         // 轴线 + 刻度 + 刻度标签
    QColor textColor;         // 图例文字（默认 = axisColor）
    QVector<QColor> seriesPalette;   // A5 循环取色

    static QChartTheme light();
    static QChartTheme dark();
};
```

预设（初值，颜色值实现时可微调）：
- `light()`：bg `#FFFFFF`、grid `#DCDCDC`、axis/text `#000000`、palette 取 matplotlib tab10 系。
- `dark()`：bg `#1E1E1E`、grid `#3A3A3A`、axis/text `#E0E0E0`、palette 用同色系**提亮版**（保证深底可读）。

### 3.2 QChartWidget 新增 API（A2：预设枚举 + 逐项覆盖）

```cpp
// 一键切换（预设枚举，值在 QChartTheme 内）
void setTheme(QChartTheme::Preset preset);
void setTheme(const QChartTheme& theme);   // 进阶：自定义调色板（同 struct，成本低，一并给）
QChartTheme theme() const;                 // 当前应用的主题（base，不含 override）

// 逐项覆盖（override 模式，与轴/网格/系列一致）
void setBackgroundColor(const QColor& c);
void clearBackgroundColor();
QColor backgroundColor() const;            // 有效色（override 或主题默认）

// 系统深/浅自动跟随（A4：默认关，可选开）
void setFollowSystemPalette(bool on);      // 监听 QGuiApplication::paletteChanged，深→Preset::Dark 浅→Preset::Light
bool followSystemPalette() const;
```

网格/轴/系列颜色逐项覆盖沿用既有 setter：`QChartLayer::setGridColor`、`QChartAxis::setColor`、`QChartSeries::setColor`（改为写 override），并各自新增 `clearGridColor()` / `clearColor()`。

## 4. 挂接点（push 模型）

`QChartWidget` 是主题唯一持有者与推送者（push 模型，绘制代码零改动）：

1. `setTheme()` → 解析 `QChartTheme` → 推送给子组件：
   - 每个 `QChartAxis`：`a->setThemeColor(theme.axisColor)`；
   - 每个 `QChartLayer`：`g->setThemeGridColor(theme.gridColor)`；
   - 每个 series（按 add 顺序，见下）：`s->setThemeColor(palette[cycle])`；
   - `QChartLegend`：`m_legend->setThemeTextColor(theme.textColor)`（类见 `design_legend.md`）；
   - 自身 `backgroundColor` 解析。
2. `addAxis()/addLayer()` 时立即补推当前主题默认；series 经 `seriesAdded` 信号分配调色板色（见下）。
3. 推送后 `invalidateBackground()`（背景/网格/轴）+ `invalidateForeground()`（系列/图例）。

背景上屏：渲染器 `drawBackground` 从 `QChartScene` 读 `backgroundColor` 填充（渲染层改造见 `design_export.md`），替换现 `fillRect(plotArea, transparent)`。

### 系列调色板循环取色（A5）实现点

- `QChartWidget` 持有 `int m_seriesColorIndex = 0`（全局 add 顺序，跨 layer）。
- 在 `addLayer` 里已连接的 `seriesAdded` 槽中：`s->setThemeColor(palette[m_seriesColorIndex % palette.size()]); ++m_seriesColorIndex;`
  - 前提：该 series 尚无 override（新加入必无）。若将来「带显式色的 series 加入」，用 `if (!s->colorOverride())` 保护，且不推进索引（保持「按 addSeries 顺序」可预测）。
- `setTheme()` 重推时：按当前 `m_layers` 的 add 顺序重排索引、重新 `setThemeColor`（仅对无 override 的 series 生效）。

## 5. 文件与构建（本功能部分）

| 文件 | 说明 | Q_OBJECT / moc |
|---|---|---|
| `QChartTheme.h` / `QChartTheme.cpp` | `QChartTheme` 结构 + `Preset` 嵌套枚举 + light()/dark() 预设 | 无（纯数据） |

跨文档共享前提（只做一次）：engineer 首步把现有 `ChartScene` 重命名为 `QChartScene`（`QChartRenderer.h:18` 纯改名，字段与语义不变，不破坏 moc/构建）；本功能随后给它加 `backgroundColor` 字段。

CMakeLists.txt 改动（**不动 target 结构 / 不动 moc 所有权约定**）：`set(QCHART_SOURCES ...)` 追加 `QChartTheme.cpp`。

## 6. 测试计划（test_qcharttheme）

遵循既有 TestUnit 约定：`tests/test_qcharttheme.h`（Q_OBJECT 类声明）+ `.cpp` 实现，接入 `CMakeLists`（QCHART_TEST_HEADERS + QChartTests 源）与 `TestUnit/main.cpp`（include + `QTest::qExec` 一行）。

- `light()` 与 `dark()` 的 backgroundColor/axisColor/gridColor 两两不同；`seriesPalette.size() > 0`。
- override 解析：`axis->setColor(red)` 后 `color()==red`（显式优先）；`setTheme(QChartTheme::Preset::Dark)` 后 `color()` 仍 `red`；`clearColor()` 后 `color()==dark.axisColor`。
- 网格同构：`layer->setGridColor` 覆盖、`clearGridColor` 回主题默认。
- 系列调色板循环：加 4 个未设色 series → `color()` == `palette[0],palette[1],palette[2],palette[0]`；显式 `setColor` 的 series 不占位/不被覆盖。
- `setFollowSystemPalette` 默认 false；开启后切换深色调色板信号 → 主题变 `Preset::Dark`（可注入假信号）。

## 7. 实现任务拆分（本功能部分，小粒度，逐 task reviewer 审查）

> 每个 task 完成即交 reviewer（D9），通过才进下一个。

1. **task-theme-core**：`QChartTheme`（`Preset` 嵌套枚举 + struct + light/dark 预设 + QChartTheme.cpp）接入 CMake；单测 test_qcharttheme（预设 + 颜色互异）。依赖：无。
2. **task-theme-override**：§2 override 模型落地到 `QChartAxis::m_color` / `QChartLayer::m_gridColor` / `QChartSeries::m_color`（getter 不变、setter 语义升级、新增 setThemeColor/clearColor）；单测 override 解析。依赖：task-theme-core。
3. **task-theme-widget**：**首步把 `ChartScene` 重命名为 `QChartScene`（纯改名）**；然后 `QChartWidget` setTheme/backgroundColor/followSystemPalette + 推送逻辑 + `QChartScene` 增加 backgroundColor + renderer drawBackground 背景填充；单测主题生效（切 Dark 后轴/网格/背景解析正确）。依赖：task-theme-override。
4. **task-theme-palette**：A5 系列调色板循环取色（seriesAdded 分配 + setTheme 重推）；单测循环/显式优先。依赖：task-theme-widget。

> 说明：task 2/3/4 存在顺序依赖是因为 override 模型是主题推送的地基；建议串行以免返工。
