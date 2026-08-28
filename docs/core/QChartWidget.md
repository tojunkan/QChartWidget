# QChartWidget Documentation

## Brief Introduction:
QChartWidget acts as the main controller for the five‑space model. It exclusively holds the Projection (coordinate mapping), Camera2D (View‑to‑Pixel geometry), and Renderer (rendering backend), and coordinates the assembly and interaction of Axes and Layers. All 2D charts (Cartesian, Polar, Functional) are instantiated via this class. 3D charts are extended through its sole subclass QChartWidget3D, which reuses Theme, Legend, and Renderer components.

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `std::unique_ptr<QChartProjection>` | `m_projection` | Define the **only one** Projection for rendering. <br> See also QChartProjection. | `std::unique_ptr<QCartesianProjection>` <br> `std::unique_ptr<QPolarProjection>` <br> `std::unique_ptr<QFunctionalProjection>` <br> `nullptr` | `nullptr` | `QChartProjection` <br> `QCartesianProjection` <br> `QPolarProjection` <br> `QFunctionalProjection` <br> `QChartProjectionFactory` |

Notes:

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartScene` | `buildScreenScene` | Construct several important variables to build up context for renderers. | NULL | protected | - | paintEvent | `QChartScene` <br> `QChartRenderer` |

## Overrided Qt Events:
| Name | Description | Parameters | Triggered By | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `paintEvent` | 1.Construct `QChartScene` <br> 2.Sent it to Renderer. | `QPaintEvent *` | plenty of `update()` callings. <br> Specifically: <br> `setMargins()` <br> `invalidateBackground()` <br> `invalidateForeground()` <br> `invalidateLayout()` | `QChartScene` <br> `QChartRenderer` |

Notes:

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `seriesHovered` | **still need further support.** <br> The signal that broadcasts the hovered series. <br> This is for those customized function for hover event. | `QChartSeries` <br> `int` <br> `bool` | NULL | `QChartSeries` <br> `QChartHitTester` |

Notes:
