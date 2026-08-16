// test_export.h —— 导出（PNG/SVG/PDF）单元测试声明
#pragma once
#include <QObject>

class TestExport : public QObject {
    Q_OBJECT
private slots:
    void png_sizeAndBackground();          // PNG 尺寸/背景（C4/C5）
    void png_defaultAndPlotAreaScope();    // 默认 WholeWidget / 显式 PlotArea 范围（C3）
    void svg_isVector();                   // SVG 真矢量（含 path、不含 image/base64）
    void pdf_header();                     // PDF 以 %PDF- 开头
    void transparentBackground();          // 透明开关 → PNG 角落 alpha==0（C5）
    void debugYellowBoxNotInExport();      // 调试黄框不泄漏进导出（PlotArea 角落=背景色）
    void pdf_transparentIgnoresSwitch();   // PDF 忽略透明开关（始终填背景）
    void svgPdf_writeFailure();            // 非法路径 → SVG/PDF 返回 false
};
