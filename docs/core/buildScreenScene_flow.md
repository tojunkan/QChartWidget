# buildScreenScene_flow.md —— 场景快照组装（2D 默认 / 3D 覆写）

> t55 核心函数 flow · core 模块（src/core/QChartWidget.cpp:379；QChartWidget3D.cpp 覆写）

## 控制流（调用图）

```
paintEvent（2D 屏显）                      saveAs*（导出，经 buildExportScene）
  └─ buildScreenScene()                     └─ buildExportScene(scope, size, outDeviceSize)
       ├─ 填充 QChartScene 字段（见数据流）        ├─ 按 scope/size 计算设备尺寸 + plotArea
       └─ 返回 scene → render(scene, device)       └─ 填充 scene（3D 子类注入 3D 段）

QChartWidget3D::buildScreenScene()（覆写，§8.2 钩子）
  ├─ 基类字段沿用（plotArea/dataBounds/viewRect 等 2D 字段留默认语义）
  └─ 注入 3D 段：camera3D / layers3D / worldBounds（scene.is3D() == true）
```

## 数据流（入参/出参/状态变更）

| QChartScene 字段 | 来源（2D 默认实现） | 3D 覆写差异 |
| :---: | :---: | :---: |
| `plotArea` | `m_plotArea`（layoutAxes 产物） | 同左 |
| `dataBounds` | `m_dataBounds`（viewRect 反算缓存） | 同左（2D 占位投影语义） |
| `viewRect` | `m_camera->viewRect()` | 同左 |
| `projection` | **临时投影优先**：`m_tempProjection ? m_tempProjection : m_projection.get()` | 2D 占位投影（渲染走 3D 段） |
| `axes` / `layers` | `m_axes` / `m_layers` | 同左 |
| `backgroundColor` | `backgroundColor()`（override 或主题默认；invalid=不填充） | 同左 |
| `legend` / `legendItems` | `m_legend` / `m_legendItems` | 同左 |
| `exportMode` | false（屏显） | 同左 |
| `camera3D` | nullptr（= is3D() false） | **注入 m_camera3D.get()**（非空 → is3D true） |
| `layers3D` | 空 | **注入 m_layers3D** |
| `worldBounds` | 默认 | **注入 m_worldBounds**（fit 链产物） |

状态变更：无（纯读取组装；paintEvent 负责消费与重绘）。

## 时序（触发时机与先后）

1. **每帧调用**：paintEvent 内、render 之前——快照必须反映当前状态（布局/视图/数据/主题）。
2. **3D 段注入时机**：QChartWidget3D 构造/布局完成后（camera3D/layers3D 就绪）才有效；scene.is3D() 是 renderer 选择 3D 路径的判据（QPainterChartRenderer::drawForeground → drawForeground3D）。
3. **临时投影解析**：投影切换动画期间 m_tempProjection 非空 → 快照取临时投影（仅渲染路径）；动画结束 clearTemporaryProjection 后回退正式投影。

## 关联

- Called By：paintEvent（屏显）；updateHover（GL 分支复用同一快照组装，src/core/QChartWidget3D.cpp）。
- 3D 导出：buildExportScene 覆写（未验收，ROADMAP 遗留）。
- 相关决策：D2（场景快照 + 参数化渲染）、D13（exportMode 跳过调试黄框）、D17（两处虚化钩子）。
