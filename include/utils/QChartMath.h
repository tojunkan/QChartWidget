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
#include <algorithm>

namespace QChartMath {

    // ===== 线/面细分判据（唯一来源：CPU 变换阶段与 GLSL 的 GS/TCS 共用）=====
    // 判据（纯 Cartesian，与相机无关）：把一条线段均分成 k 段后，每段的相对偏差
    //   ≈ d / (k · L)，要求 ≤ ratio ⇒ k = ceil( d / (ratio · L) )
    // 其中 d = |cart(中点) − (c0+c1)/2|（弓高），L = |c1 − c0|（弦长）。
    // ★ 是**线性**关系，不是 sqrt —— 弓高随段数按 1/k² 下降、弦长按 1/k 下降。
    constexpr double SUBDIV_RATIO       = 0.005;   // 允许的相对偏差（0.5%）
    constexpr int    SUBDIV_MAX_SEGMENTS = 32;      // 一条线段最多切几段（= GS 的 max_vertices − 1）

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

    // ===== 反投影（Phase 3 射线拾取预留，design_3d_axes.md §2.3；本任务实现并单测）=====
    /// Clip(齐次) → World：逆 viewProj ÷w；w<=0 → NaN 哨兵。
    /// 输入 clipPos.w<=0（相机背后/近平面外）或 viewProj 不可逆 → NaN。
    // ===== 多边形三角化（简单多边形：单环、无洞、不自交）=====
    namespace detail {
        /// 二维叉积 (b−a)×(c−a)：>0 逆时针，<0 顺时针，=0 共线
        inline qreal cross2(const QVector3D& a, const QVector3D& b, const QVector3D& c) {
            return (b.x() - a.x()) * (c.y() - a.y())
                 - (b.y() - a.y()) * (c.x() - a.x());
        }

        /// 点是否落在三角形内（含边界；同号判据）
        inline bool pointInTriangle(const QVector3D& p, const QVector3D& a,
                                    const QVector3D& b, const QVector3D& c) {
            const qreal d1 = cross2(a, b, p);
            const qreal d2 = cross2(b, c, p);
            const qreal d3 = cross2(c, a, p);
            const bool hasNeg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
            const bool hasPos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);
            return !(hasNeg && hasPos);
        }
    } // namespace detail

    /// 多边形三角化（只用顶点的 x/y 判定拓扑，与投影无关）。
    /// 凸多边形走扇形快路径 O(n)；凹多边形走耳切 O(n²)。
    /// 输出 outIndices：每 3 个一组，索引指向 verts。
    /// 退化（共线/面积为 0）/自交多边形 → 返回 false（结果未定义，调用方跳过绘制）。
    /// TODO(holes)：带洞（fillRule 多环）暂不支持。
    inline bool triangulatePolygon(const QVector<QVector3D>& verts, QVector<int>& outIndices) {
        outIndices.clear();
        const int n = verts.size();
        if (n < 3) return false;

        // 1. 有向面积 → 环绕方向（>0 逆时针）
        qreal area2 = 0.0;
        for (int i = 0; i < n; ++i) {
            const QVector3D& a = verts.at(i);
            const QVector3D& b = verts.at((i + 1) % n);
            area2 += a.x() * b.y() - b.x() * a.y();
        }
        if (qFuzzyIsNull(area2)) return false;   // 退化（共线/零面积）
        const qreal sign = (area2 > 0.0) ? 1.0 : -1.0;

        // 2. 凸多边形快速路径：扇形
        bool convex = true;
        for (int i = 0; i < n; ++i) {
            const qreal c = detail::cross2(verts.at((i + n - 1) % n), verts.at(i),
                                           verts.at((i + 1) % n));
            if (c * sign < 0.0) { convex = false; break; }
        }
        if (convex) {
            outIndices.reserve((n - 2) * 3);
            for (int i = 1; i + 1 < n; ++i) {
                outIndices << 0 << i << (i + 1);
            }
            return true;
        }

        // 3. 耳切：先把环统一成逆时针
        QVector<int> ring;
        ring.reserve(n);
        for (int i = 0; i < n; ++i) ring << i;
        if (sign < 0.0) std::reverse(ring.begin(), ring.end());

        outIndices.reserve((n - 2) * 3);
        int guard = 0;
        while (ring.size() > 3 && guard++ < n * n) {
            const int m = ring.size();
            bool clipped = false;
            for (int i = 0; i < m; ++i) {
                const int i0 = ring.at((i + m - 1) % m);
                const int i1 = ring.at(i);
                const int i2 = ring.at((i + 1) % m);
                const QVector3D& a = verts.at(i0);
                const QVector3D& b = verts.at(i1);
                const QVector3D& c = verts.at(i2);
                if (detail::cross2(a, b, c) <= 0.0) continue;   // 非凸耳
                // 耳内不得含其他顶点
                bool contains = false;
                for (int j = 0; j < m; ++j) {
                    const int jj = ring.at(j);
                    if (jj == i0 || jj == i1 || jj == i2) continue;
                    if (detail::pointInTriangle(verts.at(jj), a, b, c)) { contains = true; break; }
                }
                if (contains) continue;
                outIndices << i0 << i1 << i2;
                ring.removeAt(i);
                clipped = true;
                break;
            }
            if (!clipped) return false;   // 退化/自交：放弃
        }
        if (ring.size() == 3) outIndices << ring.at(0) << ring.at(1) << ring.at(2);
        return outIndices.size() >= 3;
    }
}
