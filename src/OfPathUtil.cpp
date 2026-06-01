#include "OfPathUtil.h"
#include "imgui.h"
#include <algorithm>
#include <limits>

namespace ofkitty {

float distSqPointSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
    glm::vec2 ab = b - a;
    float len2 = glm::dot(ab, ab);
    float t = len2 > 0.f ? std::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    glm::vec2 c = a + ab * t;
    glm::vec2 d = p - c;
    return glm::dot(d, d);
}

glm::vec2 cubicPoint(const glm::vec2& p0, const glm::vec2& p1,
                     const glm::vec2& p2, const glm::vec2& p3, float t) {
    const float u = 1.f - t;
    const float uu = u * u;
    const float tt = t * t;
    return uu * u * p0 + 3.f * uu * t * p1 + 3.f * u * tt * p2 + tt * t * p3;
}

static void pathBounds(const ofPath& path, float& minX, float& minY, float& maxX, float& maxY) {
    minX = minY = std::numeric_limits<float>::max();
    maxX = maxY = std::numeric_limits<float>::lowest();
    auto consider = [&](float x, float y) {
        minX = std::min(minX, x); minY = std::min(minY, y);
        maxX = std::max(maxX, x); maxY = std::max(maxY, y);
    };
    for (const auto& cmd : path.getCommands()) {
        consider(cmd.to.x, cmd.to.y);
        if (cmd.type == ofPath::Command::bezierTo) {
            consider(cmd.cp1.x, cmd.cp1.y);
            consider(cmd.cp2.x, cmd.cp2.y);
        } else if (cmd.type == ofPath::Command::quadBezierTo) {
            consider(cmd.cp2.x, cmd.cp2.y);
        }
    }
}

float distSqToPath(const ofPath& path, float x, float y) {
    glm::vec2 prev {0.f, 0.f};
    bool hasPrev = false;
    float best = std::numeric_limits<float>::max();
    const glm::vec2 pt {x, y};

    for (const auto& cmd : path.getCommands()) {
        if (cmd.type == ofPath::Command::moveTo) {
            prev = {cmd.to.x, cmd.to.y};
            hasPrev = true;
        } else if (cmd.type == ofPath::Command::lineTo && hasPrev) {
            glm::vec2 p {cmd.to.x, cmd.to.y};
            best = std::min(best, distSqPointSegment(pt, prev, p));
            prev = p;
        } else if (cmd.type == ofPath::Command::bezierTo && hasPrev) {
            const glm::vec2 p0 = prev;
            const glm::vec2 p1 {cmd.cp1.x, cmd.cp1.y};
            const glm::vec2 p2 {cmd.cp2.x, cmd.cp2.y};
            const glm::vec2 p3 {cmd.to.x, cmd.to.y};
            for (int i = 0; i < 32; ++i) {
                const float t0 = static_cast<float>(i) / 32.f;
                const float t1 = static_cast<float>(i + 1) / 32.f;
                best = std::min(best, distSqPointSegment(pt,
                    cubicPoint(p0, p1, p2, p3, t0),
                    cubicPoint(p0, p1, p2, p3, t1)));
            }
            prev = p3;
        } else if (cmd.type == ofPath::Command::close && hasPrev) {
            // close handled when next segment uses moveTo anchor — skip
        }
    }
    return best;
}

bool pathIntersectsRect(const ofPath& path, float rx0, float ry0, float rx1, float ry1) {
    float minX, minY, maxX, maxY;
    pathBounds(path, minX, minY, maxX, maxY);
    const float bx0 = std::min(rx0, rx1), bx1 = std::max(rx0, rx1);
    const float by0 = std::min(ry0, ry1), by1 = std::max(ry0, ry1);
    return !(maxX < bx0 || minX > bx1 || maxY < by0 || minY > by1);
}

void loadAnchorsFromPath(const ofPath& src, ImVectorEditor::Path& out) {
    out.clear();
    for (const auto& cmd : src.getCommands()) {
        if (cmd.type == ofPath::Command::moveTo || cmd.type == ofPath::Command::lineTo) {
            ImVectorEditor::Anchor a;
            a.position = ImVec2(cmd.to.x, cmd.to.y);
            out.anchors.push_back(a);
        } else if (cmd.type == ofPath::Command::bezierTo) {
            if (!out.anchors.empty()) {
                auto& prev = out.anchors.back();
                prev.handleOut = ImVec2(cmd.cp1.x - prev.position.x,
                                        cmd.cp1.y - prev.position.y);
                prev.hasHandleOut = true;
            }
            ImVectorEditor::Anchor a;
            a.position = ImVec2(cmd.to.x, cmd.to.y);
            a.handleIn = ImVec2(cmd.cp2.x - cmd.to.x, cmd.cp2.y - cmd.to.y);
            a.hasHandleIn = true;
            out.anchors.push_back(a);
        } else if (cmd.type == ofPath::Command::quadBezierTo) {
            glm::vec2 p0(cmd.cp1.x, cmd.cp1.y);
            glm::vec2 cp(cmd.cp2.x, cmd.cp2.y);
            glm::vec2 p2(cmd.to.x, cmd.to.y);
            glm::vec2 cp1 = p0 + (2.f / 3.f) * (cp - p0);
            glm::vec2 cp2 = p2 + (2.f / 3.f) * (cp - p2);
            if (!out.anchors.empty()) {
                auto& prev = out.anchors.back();
                prev.handleOut = ImVec2(cp1.x - prev.position.x, cp1.y - prev.position.y);
                prev.hasHandleOut = true;
            }
            ImVectorEditor::Anchor a;
            a.position = ImVec2(p2.x, p2.y);
            a.handleIn = ImVec2(cp2.x - p2.x, cp2.y - p2.y);
            a.hasHandleIn = true;
            out.anchors.push_back(a);
        } else if (cmd.type == ofPath::Command::close) {
            out.closed = true;
        }
    }
}

void ofPathFromEditorPath(const ImVectorEditor::Path& src, ofPath& out) {
    out.clear();
    for (size_t ai = 0; ai < src.anchors.size(); ++ai) {
        const auto& cur = src.anchors[ai];
        glm::vec3 p(cur.position.x, cur.position.y, 0.f);
        if (ai == 0) {
            out.moveTo(p);
        } else {
            const auto& prev = src.anchors[ai - 1];
            if (cur.hasHandleIn || prev.hasHandleOut) {
                glm::vec3 cp1(prev.position.x + prev.handleOut.x,
                              prev.position.y + prev.handleOut.y, 0.f);
                glm::vec3 cp2(cur.position.x + cur.handleIn.x,
                              cur.position.y + cur.handleIn.y, 0.f);
                out.bezierTo(cp1, cp2, p);
            } else {
                out.lineTo(p);
            }
        }
    }
    if (src.closed)
        out.close();
}

void drawPathOnDrawList(const ofPath& path, ImDrawList* dl, ImU32 col, float thickness,
                        const std::function<ImVec2(float x, float y)>& toScreen) {
    if (!dl) return;
    glm::vec2 prev {};
    bool hasPrev = false;
    auto drawSeg = [&](glm::vec2 a, glm::vec2 b) {
        dl->AddLine(toScreen(a.x, a.y), toScreen(b.x, b.y), col, thickness);
    };
    for (const auto& cmd : path.getCommands()) {
        if (cmd.type == ofPath::Command::moveTo) {
            prev = {cmd.to.x, cmd.to.y};
            hasPrev = true;
        } else if (cmd.type == ofPath::Command::lineTo && hasPrev) {
            glm::vec2 p {cmd.to.x, cmd.to.y};
            drawSeg(prev, p);
            prev = p;
        } else if (cmd.type == ofPath::Command::bezierTo && hasPrev) {
            const glm::vec2 p0 = prev;
            const glm::vec2 p1 {cmd.cp1.x, cmd.cp1.y};
            const glm::vec2 p2 {cmd.cp2.x, cmd.cp2.y};
            const glm::vec2 p3 {cmd.to.x, cmd.to.y};
            glm::vec2 last = p0;
            for (int i = 1; i <= 24; ++i) {
                const float t = static_cast<float>(i) / 24.f;
                glm::vec2 pt = cubicPoint(p0, p1, p2, p3, t);
                drawSeg(last, pt);
                last = pt;
            }
            prev = p3;
        } else if (cmd.type == ofPath::Command::quadBezierTo && hasPrev) {
            glm::vec2 p0(cmd.cp1.x, cmd.cp1.y);
            glm::vec2 cp(cmd.cp2.x, cmd.cp2.y);
            glm::vec2 p2(cmd.to.x, cmd.to.y);
            glm::vec2 cp1 = p0 + (2.f / 3.f) * (cp - p0);
            glm::vec2 cp2 = p2 + (2.f / 3.f) * (cp - p2);
            glm::vec2 last = p0;
            for (int i = 1; i <= 24; ++i) {
                const float t = static_cast<float>(i) / 24.f;
                glm::vec2 pt = cubicPoint(p0, cp1, cp2, p2, t);
                drawSeg(last, pt);
                last = pt;
            }
            prev = p2;
        } else if (cmd.type == ofPath::Command::close && hasPrev) {
            // close segment drawn when path loops to first anchor
        }
    }
}

} // namespace ofkitty
