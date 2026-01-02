#include <tinygl/core/tinygl.h>

namespace tinygl {

 StaticVector<VOut, 16> SoftRenderContext::clipAgainstPlane(const StaticVector<VOut, 16>& inputVerts, int planeID) {
    StaticVector<VOut, 16> outputVerts;
    if (inputVerts.empty()) return outputVerts;

    // Lambda: 判断点是否在平面内 (Inside Test)
    // OpenGL Frustum Planes based on w:
    // Left:   w + x > 0 | Right:  w - x > 0
    // Bottom: w + y > 0 | Top:    w - y > 0
    // Near:   w + z > 0 | Far:    w - z > 0
    auto isInside = [&](const Vec4& p) {
        switch (planeID) {
            case 0: return p.w + p.x >= 0; // Left
            case 1: return p.w - p.x >= 0; // Right
            case 2: return p.w + p.y >= 0; // Bottom
            case 3: return p.w - p.y >= 0; // Top
            case 4: return p.w + p.z >= EPSILON; // Near (关键! 防止除以0)
            case 5: return p.w - p.z >= 0; // Far
            default: return true;
        }
    };

    // Lambda: 计算线段与平面的交点插值系数 t (Intersection)
    // 利用相似三角形原理: t = dist_in / (dist_in - dist_out)
    auto getIntersectT = [&](const Vec4& prev, const Vec4& curr) {
        float dp = 0, dc = 0; // Dist Prev, Dist Curr
        switch (planeID) {
            case 0: dp = prev.w + prev.x; dc = curr.w + curr.x; break;
            case 1: dp = prev.w - prev.x; dc = curr.w - curr.x; break;
            case 2: dp = prev.w + prev.y; dc = curr.w + curr.y; break;
            case 3: dp = prev.w - prev.y; dc = curr.w - curr.y; break;
            case 4: dp = prev.w + prev.z; dc = curr.w + curr.z; break;
            case 5: dp = prev.w - prev.z; dc = curr.w - curr.z; break;
        }
        return dp / (dp - dc);
    };

    const VOut* prev = &inputVerts.back();
    bool prevInside = isInside(prev->pos);

    for (const auto& curr : inputVerts) {
        bool currInside = isInside(curr.pos);

        if (currInside) {
            if (!prevInside) {
                // 情况 1: Out -> In (外部进入内部)
                // 需要在交点处生成新顶点，并加入
                float t = getIntersectT(prev->pos, curr.pos);
                outputVerts.push_back(lerpVertex(*prev, curr, t));
            }
            // 情况 2: In -> In (一直在内部)
            // 直接加入当前点
            outputVerts.push_back(curr);
        } else if (prevInside) {
            // 情况 3: In -> Out (内部跑到外部)
            // 需要在交点处生成新顶点，并加入
            float t = getIntersectT(prev->pos, curr.pos);
            outputVerts.push_back(lerpVertex(*prev, curr, t));
        }
        // 情况 4: Out -> Out (一直在外部)，直接丢弃

        prev = &curr;
        prevInside = currInside;
    }

    return outputVerts;
}


bool SoftRenderContext::clipLineAxis(float p, float q, float& t0, float& t1) {
    if (std::abs(p) < EPSILON) { // Parallel to the boundary
        if (q < 0) return false; // Outside the boundary
    } else {
        float t = q / p;
        if (p < 0) { // Potentially entering
            if (t > t1) return false;
            if (t > t0) t0 = t;
        } else { // Potentially exiting
            if (t < t0) return false;
            if (t < t1) t1 = t;
        }
    }
    return true;
}

StaticVector<VOut, 16> SoftRenderContext::clipLine(const VOut& v0, const VOut& v1) {
    float t0 = 0.0f, t1 = 1.0f;
    Vec4 d = v1.pos - v0.pos;

    // Clip against 6 planes of the canonical view volume
    if (!clipLineAxis( d.x + d.w, -(v0.pos.x + v0.pos.w), t0, t1)) return {}; // Left
    if (!clipLineAxis(-d.x + d.w,  (v0.pos.x - v0.pos.w), t0, t1)) return {}; // Right
    if (!clipLineAxis( d.y + d.w, -(v0.pos.y + v0.pos.w), t0, t1)) return {}; // Bottom
    if (!clipLineAxis(-d.y + d.w,  (v0.pos.y - v0.pos.w), t0, t1)) return {}; // Top
    if (!clipLineAxis( d.z + d.w, -(v0.pos.z + v0.pos.w), t0, t1)) return {}; // Near
    if (!clipLineAxis(-d.z + d.w,  (v0.pos.z - v0.pos.w), t0, t1)) return {}; // Far

    StaticVector<VOut, 16> clippedVerts;
    if (t0 > 0.0f) {
        clippedVerts.push_back(lerpVertex(v0, v1, t0));
    } else {
        clippedVerts.push_back(v0);
    }

    if (t1 < 1.0f) {
        clippedVerts.push_back(lerpVertex(v0, v1, t1));
    } else {
        clippedVerts.push_back(v1);
    }
    
    return clippedVerts;
}

}