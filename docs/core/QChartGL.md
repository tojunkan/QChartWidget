# QChartGL Documentation

## Brief Introduction:
QChartGL 是 **GL 资源池**（全静态工具类，禁止实例化：ctor/dtor/copy/assign 全 delete）：① 共享根上下文管理——`registerHost/unregisterHost`（宿主实例计数；首实例惰性创建共享根 `QOffscreenSurface + QOpenGLContext`（引用计数），末实例注销时释放程序池与根上下文）、`sharedContext()`（无存活实例 → nullptr）；② **Shader 程序池**——按 `(QOpenGLContext*, ShaderKind, projection 类型哈希)` 缓存已编译程序（`s_programs`）；③ 动态拼接 Shader——从 Projection 取 `glslToCartesian()` **表达式**注入顶点着色器（R10 契约定型：注入点包装 `vec3 num = a_pos; vec3 cart = <expr>;`，空投影回退恒等 `num`），Point 种类附加 `u_pointSize`/`gl_PointSize`；Fragment：Line/Triangle 颜色直通、Point 圆形 discard、Pick 输出 RGB24 图元 ID。统一 SurfaceFormat：3.3 Core + depth 24（`surfaceFormat()`）。

## Constant Variables:
None.（同头文件枚举 `ShaderKind{Line, Point, Triangle, Pick}` 为类型定义非类成员；Pick 注释：ID 帧拾取复用 Line 顶点输出 RGB24 ID，S0 拾取后置未用）

## Member Variables:
None.（类内无成员；cpp 匿名命名空间文件级状态：`s_instanceCount`、`s_sharedContext`、`s_shareSurface`、`s_programs`（context → (kind, projectionTypeHash) → program）；Shader 构建函数 buildVertexShader/buildFragmentShader/compileProgram/projectionTypeHash 均文件内 static）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `static void` | `registerHost` | 宿主注册：计数 +1；首实例时创建共享根（surfaceFormat）QOffscreenSurface + QOpenGLContext；创建失败（无 GL 环境）→ 清理并单次 qWarning（QPainter 路径共存兜底） | 无 | public | — | `QChartAbstractWidget`/`QChartWidget3D`（**未入 S0**）；S0 无调用方 | — |
| `static void` | `unregisterHost` | 宿主注销：计数 -1；归零时 `releasePrograms()` + 删除共享根上下文/表面 | 无 | public | — | 同上（GlHost 析构） | — |
| `static QOpenGLContext*` | `sharedContext` | 返回共享根上下文（无存活实例 → nullptr） | 无 | public | 指针/`nullptr` | `QChartAbstractWidget`/`QChartWidget3D`（未入 S0） | — |
| `static QSurfaceFormat` | `surfaceFormat` | 统一格式：3.3 CoreProfile + depth 24 + samples 0 | 无 | public | `QSurfaceFormat` | TestAxisMatrixGl（`host.setFormat(...)`）、registerHost 内部 | — |
| `static QOpenGLShaderProgram*` | `program` | 取/编译 Shader 程序：key=(当前上下文, kind, projection 类型哈希)；命中缓存直接返回，否则 compileProgram 并插入（编译失败 → nullptr） | `ShaderKind kind, const QChartAbstractProjection* projection` | public | 指针/`nullptr` | `QOpenGLChartRenderer::drawPass`（每 pass 一次） | `QOpenGLChartRenderer` |
| `static void` | `releasePrograms` | 显式释放所有已编译程序（qDeleteAll 全清；unregisterHost 末实例自动调用） | 无 | public | — | `unregisterHost` 内部 | — |

Notes:
- GLSL 注入契约（R10）：`glslToCartesian()` 返回**以 `vec3 num` 为输入的 Cartesian 表达式**；buildVertexShader 在其外包 `vec3 num = a_pos; vec3 cart = <expr>;`，再 `u_viewProj * vec4(cart,1.0)`（clip.z += u_depthBias×clip.w）；gl_PointSize 仅 Point 种类声明。GLSL 内置函数（radians/cos/sin/mix 等）可用；插值投影需 `u_blendAlpha` uniform。
- projectionTypeHash：`qHash(typeid(*proj).name()) ^ (dimension()<<32)`（0 当 projection 空）；dimensionName(0) 用于编译日志。
- 程序池按 QOpenGLContext* 键存；上下文销毁后的条目清理随宿主阶段完善（当前释放仅走 unregisterHost→releasePrograms 全清路径）。

## Overrided Qt Events:
无（非 QObject）。

## Signals:
None.（非 QObject，无信号）
