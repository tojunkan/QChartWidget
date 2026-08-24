// QChartRenderer.cpp —— 渲染器抽象接口实现
#include "QChartRenderer.h"

// 虚析构放 .cpp：给多态基类一个稳定的 vtable 锚点（避免每个 TU 各发一份）。
QChartRenderer::~QChartRenderer() = default;
