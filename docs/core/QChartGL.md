# QChartGL Documentation

## Brief Introduction:
GL 资源池（design_phase3 §7.3，A2/A3：共享/惰性/引用计数）。职责：`sharedContext()`（共享根，惰性：registerHost 首实例创建 QOffscreenSurface+QOpenGLContext 3.3 Core+depth24，后续 GlHost 经 setShareContext 共享（Qt≥6.5），无存活实例返回 nullptr——零资源 A3）；`program(ShaderKind)`（**程序池按上下文建池**（t44 落实 t43 O2：Qt 6.4.2 无 setShareContext → 多 widget 上下文独立；Qt≥6.5 共享后同一程序多上下文可用，池按上下文冗余=正确性优先）；引用计数随实例增减）；`releasePrograms()`（最后实例析构时）。`ShaderKind{Line, Point, Pick}` 为程序池键。**非 Q_OBJECT**（moc 约定：本类不进）。

## Constant Variables:

| Type | Name | Description | Available Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `enum class ShaderKind` | `ShaderKind` | 程序池键：Line（折线）/ Point（散点）/ Pick（ID 帧拾取）。 | `Line` <br> `Point` <br> `Pick` | `QOpenGLChartRenderer` <br> `QChartGL` |

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QOpenGLContext*` | `s_sharedContext` | （static）共享根上下文（惰性创建；无实例 → nullptr 零资源）。 | `QOpenGLContext*` <br> `nullptr` | `nullptr` | `QOpenGLContext` |
| `QOffscreenSurface*` | `s_offscreenSurface` | （static）共享根配套 offscreen surface。 | `QOffscreenSurface*` <br> `nullptr` | `nullptr` | `QOffscreenSurface` |
| `int` | `s_hostCount` | （static）注册实例计数（首实例建/末实例释放）。 | `int` | `0` | — |
| `QHash<QOpenGLContext*, QHash<int, QOpenGLShaderProgram*>>` | `s_programs` | （static）**按上下文建池**：context → (kind → program)；上下文销毁后条目保留至 releasePrograms（t45 O3 地址复用理论风险，t50 保持观察）。 | `QHash<QOpenGLContext*, QHash<int, QOpenGLShaderProgram*>>` | 空 | `QOpenGLShaderProgram` |

Notes:
- 头注释「share group 内复用」为 t42 骨架期措辞；实现按上下文建池（t44 修正，见 audit B2/C2）。
- 头注释「program() 暂返回 nullptr 占位」已过时（t44 已实现程序池），见 audit D3。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `QOpenGLContext*` | `sharedContext` | 共享根（惰性：首实例创建 QOffscreenSurface+QOpenGLContext 3.3 Core+depth24；registerHost 首实例建/末实例释放；无实例 → nullptr）。 | 无 | public static | `QOpenGLContext*`/`nullptr` | `GlHost`（setShareContext）/`initializeGL` | `QOpenGLContext` <br> `GlHost` |
| `QOpenGLShaderProgram*` | `program` | 程序池取/建：当前上下文（`QOpenGLContext::currentContext`）→ kind 键 → 惰性编译（Line/Point/Pick GLSL 330 字符串，§4；编译需 current context，t43 O2 调用方 ensure current 后使用）。 | `ShaderKind kind` | public static | `QOpenGLShaderProgram*`/`nullptr`（编译失败） | `QOpenGLChartRenderer::initializeGL/drawPass` | `QOpenGLShaderProgram` <br> `ShaderKind` |
| `void` | `releasePrograms` | 释放全部程序（glDeleteProgram；无 current 上下文时 qDeleteAll——glDeleteProgram no-op 安全，t45 O4 观察）。 | 无 | public static | — | 最后宿主实例析构 | `QOpenGLShaderProgram` |
| `QSurfaceFormat` | `surfaceFormat` | QSurfaceFormat 统一（3.3 Core、depth 24、vsync 默认，§7.3）。 | 无 | public static | — | `GlHost` 构造 | `QSurfaceFormat` |

Notes:
- 实例登记（registerHost/unregisterHost 内部静态）：首实例创建共享根、末实例 releasePrograms + 共享根归零（`sharedContext_refcount` 单测：成对安全/格式/归零 nullptr/可重建）。
- 创建失败：一次性告警降级（A9，GL 不可用时 QPainter 路径共存）。

## Overrided Qt Events:
None.（非 QWidget）

## Signals:
None.（非 QObject；**非 Q_OBJECT** 类，moc 约定不产 moc）
