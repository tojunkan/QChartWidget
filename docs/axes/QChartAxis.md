# QChartAxis Documentation

## Brief Introduction:
QChartAxis 是轴基类（QObject），掌管五空间链中 Data ↔ Numeric 一环与刻度/标签/样式：`toNumeric/fromNumeric` 纯虚数值化、`tickValues/tickLabels` 纯虚刻度生成、`drawAtEdge`（边框轴，直接 QPainter，S0 无调用方）与 `drawAtPosition`（数据主脊，**图元化**：只向 QChartScene 追加 Path/Point 图元与 QChartTextLabel，不做坐标映射/绘制——映射与绘制归 Projection/Renderer）。2D/3D 通用。

## Constant Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `static constexpr qreal` | `AXIS_MARGIN` | （protected）边框轴轴线到文字外侧总边距 | `qreal` | `8.0` | — |
| `static constexpr qreal` | `TICK_LENGTH` | （protected）主刻度线长度（占轴长比例） | `qreal` | `0.015` | — |
| `static constexpr qreal` | `SUB_TICK_LENGTH` | （protected）次刻度线长度 | `qreal` | `2.0` | — |
| `static constexpr qreal` | `TEXT_PADDING` | （protected）标签文字四周呼吸空间 | `qreal` | `3.0` | — |

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `qreal` | `m_sugarMin` | （protected）语法糖 setRange 下限（非映射基准，Widget 阶段连接 rangeChanged 消费） | `qreal` | `0.0` | — |
| `qreal` | `m_sugarMax` | （protected）语法糖 setRange 上限 | `qreal` | `0.0` | — |
| `int` | `m_tickCount` | （protected）目标主刻度数（niceStep 参考） | `int(≥2)` | `5` | — |
| `int` | `m_subTickCount` | （protected）每主刻度间次刻度数 | `int(≥0)` | `0` | — |
| `bool` | `m_visible` | （protected）轴可见开关 | `true`/`false` | `true` | — |
| `QString` | `m_title` | （protected）轴标题 | `QString` | 空 | — |
| `std::optional<QColor>` | `m_colorOverride` | （protected）用户显式设色覆盖 | `std::optional<QColor>`/`std::nullopt` | `std::nullopt` | `QChartTheme` |
| `QColor` | `m_themeColor` | （protected）主题注入默认色 | `QColor` | `Qt::black` | `QChartTheme` |
| `Qt::Alignment` | `m_alignment` | （protected）轴对齐（六种合法值，非法回退 AlignVCenter） | Bottom/Top/Left/Right/HCenter/VCenter | `Qt::AlignBottom` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartAxis` | 构造；校验 alignment 合法性（非法回退 VCenter）并记日志 | `QObject* parent=nullptr, Qt::Alignment alignment=AlignBottom` | public | — | 子类构造 | — |
| `virtual qreal` | `toNumeric` | 纯虚：Data → Numeric；非法输入返回 NaN 并 qWarning | `QVariant data` | public | `qreal`/NaN | 子类实现；Series/Widget 阶段消费 | — |
| `virtual QVariant` | `fromNumeric` | 纯虚：Numeric → Data | `qreal num` | public | `QVariant` | 子类实现；Widget 阶段消费 | — |
| `virtual QVector<qreal>` | `tickValues` | 纯虚：区间内主刻度位置（Numeric 空间） | `qreal numericMin, qreal numericMax` | public | `QVector<qreal>` | `QChartLayer::drawGrid`、测试刻度组装 | — |
| `virtual QStringList` | `tickLabels` | 纯虚：刻度格式化标签 | `const QVector<qreal>& ticks` | public | `QStringList` | `drawAtPosition` 内部 | — |
| `virtual QVector<qreal>` | `subTickValues` | 次刻度，默认空 | `qreal numericMin, qreal numericMax` | public | `QVector<qreal>` | `drawAtEdge`（待 Widget 阶段） | — |
| `void` | `drawAtEdge` | 边框轴模式：直接 QPainter 画在 plotArea 边缘（不图元化；S0 未入管线） | `QPainter* p, const DrawContext& ctx, bool drawAxisLine, bool drawLabels, bool drawTicks` | public | — | S0 无调用方（Widget 阶段恢复） | `DrawContext` |
| `virtual void` | `drawAtPosition` | 数据主脊：生成 Numeric 图元（1 Path 轴脊 + 每刻度 7 Point 点 + 标签，标签 refPrimitiveId 绑定刻度中心点）；默认参数仅头文件声明 | `qreal dimMin, qreal dimMax, qreal offset0, qreal offset1, int dimIndex, QChartScene& scene, int segments=72, bool drawLabels=true` | public | — | `QChartLayer::drawGrid`、TestAxisPipeline/TestAxisMatrix 夹具 | `QChartScene` |
| `virtual QSizeF` | `sizeHint` | 边框轴占用空间估算；HCenter/VCenter（数据主脊）返回 {0,0} | `const QFont& font` | public | `QSizeF` | S0 无调用方（Widget 布局阶段） | — |
| `void` | `setRange` | 语法糖存 min/max（仅 Cartesian 有意义），发 rangeChanged+styleChanged | `qreal min, qreal max` | public | — | Widget 阶段/测试 | — |
| `qreal` | `min` | 语法糖下限（内联） | 无 | public | `qreal` | `QChartLayer::setAxisX` | — |
| `qreal` | `max` | 语法糖上限（内联） | 无 | public | `qreal` | `QChartLayer::setAxisY` | — |
| `Qt::Alignment` | `alignment` | 对齐访问器（内联） | 无 | public | `Qt::Alignment` | 测试 | — |
| `void` | `setAlignment` | 设置对齐（内联，不校验/不重算布局） | `Qt::Alignment a` | public | — | 用户/测试 | — |
| `bool` | `isVisible` | 可见访问器（内联） | 无 | public | `true`/`false` | 测试 | — |
| `void` | `setVisible` | 设置可见，变化时发 visibleChanged | `bool v` | public | — | 用户/测试 | — |
| `QString` | `title` | 标题访问器（内联） | 无 | public | `QString` | 测试 | — |
| `void` | `setTitle` | 设置标题（内联，不发信号） | `const QString& t` | public | — | 用户 | — |
| `QColor` | `color` | 有效色 = override 或主题默认 | 无 | public | `QColor` | Renderer 标签/图元 | `QChartTheme` |
| `void` | `setColor` | 写 override（同值忽略），发 styleChanged | `const QColor& c` | public | — | 用户/测试 | — |
| `void` | `setThemeColor` | 主题注入默认色（无 override 才真正变化） | `const QColor& c` | public | — | Widget 阶段主题推送 | `QChartTheme` |
| `void` | `clearColor` | 清 override 回主题默认 | 无 | public | — | 用户 | `QChartTheme` |
| `std::optional<QColor>` | `colorOverride` | override 访问器（内联） | 无 | public | `std::optional<QColor>` | 主题判断 | `QChartTheme` |
| `int` | `tickCount` | 主刻度数访问器（内联） | 无 | public | `int` | 测试 | — |
| `void` | `setTickCount` | 设置主刻度数（<2 钳 2），变化发 tickCountChanged | `int n` | public | — | 用户/测试 | — |
| `int` | `subTickCount` | 次刻度数访问器（内联） | 无 | public | `int` | 测试 | — |
| `void` | `setSubTickCount` | 设置次刻度数，变化发 subTickCountChanged | `int n` | public | — | 用户 | — |
| `bool` | `isHorizontal` | 是否水平方向（Top/Bottom/HCenter）（内联） | 无 | public | `true`/`false` | `drawAtEdge` | — |
| `virtual bool` | `isInteractive` | 是否允许 pan/zoom；离散域轴覆盖返回 false | 无 | public | `true`/`false` | Widget 阶段交互 | — |
| `static constexpr qreal` | `textPadding` | TEXT_PADDING 公开访问（内联 static） | 无 | public | `qreal` | 标签排版 | — |

Notes:
- 本类不负责坐标映射（Projection）与视窗变换（Camera/Widget）；不持有 QPainter 绘制状态。
- Q_PROPERTY：visible/title/color/tickCount/subTickCount/alignment。

## Overrided Qt Events:
无（QObject，非 QWidget；S0 子集内无 Qt 事件覆写）。

## Signals:

| Name | Description | Parameters | Emitted By | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `rangeChanged` | setRange 语法糖触发（Widget 连接后映射 dataRange） | `qreal min, qreal max` | `setRange` | — |
| `visibleChanged` | 可见性变化 | 无 | `setVisible` | — |
| `styleChanged` | 样式变化（颜色/title/range） | 无 | `setColor`/`setThemeColor`(无 override 时)/`clearColor`/`setRange`/`setTitle` | `QChartTheme` |
| `tickCountChanged` | 主刻度数变化 | 无 | `setTickCount` | — |
| `subTickCountChanged` | 次刻度数变化 | 无 | `setSubTickCount` | — |
