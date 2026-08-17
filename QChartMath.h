// QChartMath.h —— 3D 数学纯函数（inline，无 Q_OBJECT）
// 职责：Clip → NDC → Screen 显式拆分（Phase 3 GL 同用）、矩阵构造辅助、深度辅助、
//       批量投影入口（Phase 3 GPU 批量/预转换时签名不变）。
// 形态：header-only（#pragma once + inline），不新增 .cpp → CMake QCHART_SOURCES 零改动。
#pragma once
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QRectF>
#include <QPointF>
#include <QVector>
#include <QtMath>

namespace QChartMath {

    // ===== Clip → NDC → Screen（显式拆分，可测可复用；Phase 3 GL 同用）=====
    /// Clip → NDC：÷w。w<=0（相机背后/近平面外）→ 返回 NaN 哨兵（调用方跳过）
    inline QVector3D clipToNdc(const QVector4D& clip) {
        if (clip.w() <= 0.0)
            return QVector3D(qQNaN(), qQNaN(), qQNaN());
        return QVector3D(clip.x() / clip.w(),
                         clip.y() / clip.w(),
                         clip.z() / clip.w());
    }

    /// NDC → Screen：x: left+(ndc.x+1)/2*w；y: bottom-(ndc.y+1)/2*h
    /// （y 翻转，与 2D cartesianToPixel 一致：View 上 → 像素上，见 design_3d.md §3 ⚠）
    inline QPointF ndcToScreen(const QVector3D& ndc, const QRectF& plotArea) {
        qreal x = plotArea.left() + (ndc.x() + 1.0) * 0.5 * plotArea.width();
        qreal y = plotArea.bottom() - (ndc.y() + 1.0) * 0.5 * plotArea.height();
        return QPointF(x, y);
    }

    /// Clip → Screen 组合（含 w<=0 → NaN 检查）
    inline QPointF clipToScreen(const QVector4D& clip, const QRectF& plotArea) {
        if (clip.w() <= 0.0)
            return QPointF(qQNaN(), qQNaN());
        return ndcToScreen(clipToNdc(clip), plotArea);
    }

    // ===== 矩阵构造辅助（frustum 参数）=====
    /// 透视：fovY 度、aspect、near/far（对 QMatrix4x4::perspective 的封装，集中约定）
    inline QMatrix4x4 perspectiveMatrix(qreal fovYDeg, qreal aspect,
                                        qreal nearP, qreal farP) {
        QMatrix4x4 m;
        m.perspective(fovYDeg, aspect, nearP, farP);
        return m;
    }

    /// 正交：视口盒 + near/far（对 QMatrix4x4::ortho 的封装）
    inline QMatrix4x4 orthographicMatrix(qreal left, qreal right,
                                         qreal bottom, qreal top,
                                         qreal nearP, qreal farP) {
        QMatrix4x4 m;
        m.ortho(left, right, bottom, top, nearP, farP);
        return m;
    }

    // ===== 深度辅助 =====
    /// 视图空间深度 = -viewZ（相机前方为正；数值越大离相机越远，
    /// painter 排序用「远→近」= depth 降序，见 design_3d.md §7.4）
    inline qreal viewDepth(const QMatrix4x4& viewMatrix, const QVector3D& worldPoint) {
        return -(viewMatrix.map(worldPoint).z());
    }

    // ===== 批量投影（Phase 3 预留入口，D-3D-10；Phase 2 实现并单测）=====
    /// World 批量 → 屏幕点数组 + 深度数组（逐点调用 clipToScreen/viewDepth；
    /// Phase 3 换 GPU 批量/预转换时签名不变）。
    /// depth 用 viewDepth(view, worldPoint) 计算（排序键），与屏幕点数组对齐；
    /// w<=0 的点屏幕坐标为 NaN 哨兵（槽位仍保留，两数组逐元素对齐）。
    inline void projectBatch(const QMatrix4x4& viewProj, const QMatrix4x4& view,
                             const QRectF& plotArea,
                             const QVector<QVector3D>& world,
                             QVector<QPointF>* outScreen, QVector<qreal>* outDepth) {
        if (!outScreen || !outDepth) return;
        outScreen->resize(world.size());
        outDepth->resize(world.size());
        for (int i = 0; i < world.size(); ++i) {
            const QVector3D& p = world.at(i);
            const QVector4D clip = viewProj * QVector4D(p, 1.0f);
            (*outScreen)[i] = clipToScreen(clip, plotArea);
            (*outDepth)[i] = viewDepth(view, p);
        }
    }
}
