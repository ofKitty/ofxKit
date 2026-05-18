#include "RulerUtil.h"

#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace ofkitty {

// ============================================================================
// Internal helpers
// ============================================================================

static float niceInterval(float pixPerU, float targetPx) {
    const float c[] = {1.f, 2.f, 5.f, 10.f, 20.f, 25.f, 50.f, 100.f, 200.f, 500.f, 1000.f};
    float best = 10.f, bestDiff = 1e9f;
    for (float v : c) {
        float d = std::fabs(v * pixPerU - targetPx);
        if (d < bestDiff) { bestDiff = d; best = v; }
    }
    return best;
}

static constexpr float kHitPx = 5.f;

// ============================================================================
// drawRulersInRegion
// ============================================================================

void drawRulersInRegion(ImDrawList*  dl,
                        ImVec2       origin,
                        ImVec2       size,
                        ImVec2       mouse,
                        float        pixPerUnit,
                        const char*  unitLabel,
                        float        uiScale,
                        float        rulerScale,
                        GuideSet*    guides,
                        ImVec2       scrollPx)
{
    if (!dl || size.x <= 0.f || size.y <= 0.f) return;
    if (pixPerUnit <= 0.f) pixPerUnit = 1.f;

    const float scale = uiScale * rulerScale;
    const float RS    = std::round(20.f * scale);
    const float fs    = std::round(9.f  * scale);

    constexpr ImU32 kBg    = IM_COL32(25,  25,  35,  235);
    constexpr ImU32 kBord  = IM_COL32(70,  70,  85,  255);
    constexpr ImU32 kTick  = IM_COL32(160, 160, 175, 210);
    constexpr ImU32 kLabel = IM_COL32(130, 130, 145, 255);
    constexpr ImU32 kCurs  = IM_COL32(240, 80,  80,  220);

    ImFont* fn = ImGui::GetFont();

    // Screen position of the content area top-left (after ruler strips)
    const float cX = origin.x + RS;
    const float cY = origin.y + RS;

    // Cursor in content units, accounting for scroll
    // tick-0 is at screen position (cX + scrollPx.x, cY + scrollPx.y)
    const float mx = (mouse.x - cX - scrollPx.x) / pixPerUnit;
    const float my = (mouse.y - cY - scrollPx.y) / pixPerUnit;

    // Visible unit range for ticks (what portion of the ruler is on-screen)
    const float unitMinX = -scrollPx.x / pixPerUnit;
    const float unitMaxX = (size.x - RS - scrollPx.x) / pixPerUnit;
    const float unitMinY = -scrollPx.y / pixPerUnit;
    const float unitMaxY = (size.y - RS - scrollPx.y) / pixPerUnit;

    const float majorInterval = niceInterval(pixPerUnit, 100.f);
    const float minorInterval = majorInterval / 5.f;

    // Helper: content-unit value → screen X / Y
    auto toScreenX = [&](float u) { return cX + scrollPx.x + u * pixPerUnit; };
    auto toScreenY = [&](float u) { return cY + scrollPx.y + u * pixPerUnit; };

    // =========================================================================
    // GUIDE INTERACTION
    // =========================================================================
    if (guides) {
        const ImGuiIO& io = ImGui::GetIO();
        const bool lclicked  = io.MouseClicked[0];
        const bool lreleased = io.MouseReleased[0];
        const bool rclicked  = io.MouseClicked[1];

        const ImVec2 hRulerMin(origin.x + RS, origin.y);
        const ImVec2 hRulerMax(origin.x + size.x, origin.y + RS);
        const ImVec2 vRulerMin(origin.x, origin.y + RS);
        const ImVec2 vRulerMax(origin.x + RS, origin.y + size.y);
        const ImVec2 contentMin(cX, cY);
        const ImVec2 contentMax(origin.x + size.x, origin.y + size.y);

        auto inHRuler = [&](ImVec2 p) {
            return p.x >= hRulerMin.x && p.x <= hRulerMax.x
                && p.y >= hRulerMin.y && p.y <= hRulerMax.y;
        };
        auto inVRuler = [&](ImVec2 p) {
            return p.x >= vRulerMin.x && p.x <= vRulerMax.x
                && p.y >= vRulerMin.y && p.y <= vRulerMax.y;
        };
        auto inContent = [&](ImVec2 p) {
            return p.x >= contentMin.x && p.x <= contentMax.x
                && p.y >= contentMin.y && p.y <= contentMax.y;
        };

        if (!guides->dragging) {
            // Right-click delete
            if (rclicked && inContent(mouse)) {
                for (int gi = 0; gi < (int)guides->v.size(); gi++) {
                    if (std::fabs(mouse.x - toScreenX(guides->v[gi])) <= kHitPx) {
                        guides->v.erase(guides->v.begin() + gi); break;
                    }
                }
                for (int gi = 0; gi < (int)guides->h.size(); gi++) {
                    if (std::fabs(mouse.y - toScreenY(guides->h[gi])) <= kHitPx) {
                        guides->h.erase(guides->h.begin() + gi); break;
                    }
                }
            }

            // Move existing guide
            if (lclicked && inContent(mouse)) {
                for (int gi = 0; gi < (int)guides->v.size(); gi++) {
                    if (std::fabs(mouse.x - toScreenX(guides->v[gi])) <= kHitPx) {
                        guides->dragging = true; guides->dragIsH = false;
                        guides->dragIndex = gi;  guides->dragValue = guides->v[gi];
                        break;
                    }
                }
                if (!guides->dragging) {
                    for (int gi = 0; gi < (int)guides->h.size(); gi++) {
                        if (std::fabs(mouse.y - toScreenY(guides->h[gi])) <= kHitPx) {
                            guides->dragging = true; guides->dragIsH = true;
                            guides->dragIndex = gi;  guides->dragValue = guides->h[gi];
                            break;
                        }
                    }
                }
            }

            // Create from ruler strip
            if (!guides->dragging && lclicked) {
                if (inHRuler(mouse)) {
                    guides->dragging = true; guides->dragIsH   = true;
                    guides->dragIndex = -1;  guides->dragValue = my;
                } else if (inVRuler(mouse)) {
                    guides->dragging = true; guides->dragIsH   = false;
                    guides->dragIndex = -1;  guides->dragValue = mx;
                }
            }
        } else {
            // Update live value
            guides->dragValue = guides->dragIsH ? my : mx;

            if (lreleased) {
                const bool outside = !inContent(mouse);
                auto& vec = guides->dragIsH ? guides->h : guides->v;
                if (guides->dragIndex >= 0) {
                    if (outside) {
                        if (guides->dragIndex < (int)vec.size())
                            vec.erase(vec.begin() + guides->dragIndex);
                    } else {
                        if (guides->dragIndex < (int)vec.size())
                            vec[guides->dragIndex] = guides->dragValue;
                    }
                } else if (!outside) {
                    vec.push_back(guides->dragValue);
                }
                guides->dragging = false;
            }
        }
    }

    // =========================================================================
    // DRAW GUIDES (below ruler strips so they don't bleed into the margins)
    // =========================================================================
    if (guides && guides->visible) {
        const ImU32 gc = guides->color;
        for (float gv : guides->v) {
            float sx = toScreenX(gv);
            if (sx >= cX && sx <= origin.x + size.x)
                dl->AddLine({sx, cY}, {sx, origin.y + size.y}, gc, 1.f);
        }
        for (float gh : guides->h) {
            float sy = toScreenY(gh);
            if (sy >= cY && sy <= origin.y + size.y)
                dl->AddLine({cX, sy}, {origin.x + size.x, sy}, gc, 1.f);
        }
        if (guides->dragging) {
            const ImU32 ghostCol = IM_COL32(0, 190, 255, 220);
            if (guides->dragIsH) {
                float sy = toScreenY(guides->dragValue);
                dl->AddLine({cX, sy}, {origin.x + size.x, sy}, ghostCol, 1.5f);
                char buf[32]; std::snprintf(buf, sizeof(buf), "%.4g%s", guides->dragValue, unitLabel);
                dl->AddText(fn, fs, {cX + 4.f, sy - fs - 2.f}, ghostCol, buf);
            } else {
                float sx = toScreenX(guides->dragValue);
                dl->AddLine({sx, cY}, {sx, origin.y + size.y}, ghostCol, 1.5f);
                char buf[32]; std::snprintf(buf, sizeof(buf), "%.4g%s", guides->dragValue, unitLabel);
                dl->AddText(fn, fs, {sx + 2.f, cY + 4.f}, ghostCol, buf);
            }
        }
        if (!guides->dragging) {
            const ImVec2& mp = ImGui::GetIO().MousePos;
            for (float gv : guides->v) {
                float sx = toScreenX(gv);
                if (std::fabs(mp.x - sx) <= kHitPx)
                    dl->AddLine({sx, cY}, {sx, origin.y + size.y}, IM_COL32(0, 210, 255, 255), 1.5f);
            }
            for (float gh : guides->h) {
                float sy = toScreenY(gh);
                if (std::fabs(mp.y - sy) <= kHitPx)
                    dl->AddLine({cX, sy}, {origin.x + size.x, sy}, IM_COL32(0, 210, 255, 255), 1.5f);
            }
        }
    }

    // =========================================================================
    // DRAW RULERS (drawn last so strips cover guide line ends)
    // =========================================================================

    // ---- Horizontal ruler (top strip) ----
    {
        const ImVec2 rMin(origin.x + RS, origin.y);
        const ImVec2 rMax(origin.x + size.x, origin.y + RS);
        dl->AddRectFilled(rMin, rMax, kBg);
        dl->AddLine({rMin.x, rMax.y}, {rMax.x, rMax.y}, kBord);

        float u = std::floor(unitMinX / minorInterval) * minorInterval;
        for (; u <= unitMaxX; u += minorInterval) {
            const float px = toScreenX(u);
            if (px < rMin.x || px > rMax.x) continue;
            const bool  isMajor = std::fmod(std::fabs(u), majorInterval) < minorInterval * 0.5f;
            const float tickLen = isMajor ? RS * 0.65f : RS * 0.3f;
            dl->AddLine({px, rMax.y - tickLen}, {px, rMax.y}, kTick);
            if (isMajor) {
                char buf[20]; std::snprintf(buf, sizeof(buf), "%.4g%s", u, unitLabel);
                dl->AddText(fn, fs, {px + 2.f, origin.y + 2.f}, kLabel, buf);
            }
        }

        const float cx = toScreenX(mx);
        if (cx >= rMin.x && cx <= rMax.x) {
            dl->AddLine({cx, origin.y}, {cx, origin.y + RS}, kCurs, 1.5f);
            char buf[24]; std::snprintf(buf, sizeof(buf), "%.4g%s", mx, unitLabel);
            const float tw = ImGui::CalcTextSize(buf).x * (fs / ImGui::GetFontSize());
            dl->AddText(fn, fs, {cx - tw * 0.5f, origin.y + 2.f}, kCurs, buf);
        }
    }

    // ---- Vertical ruler (left strip) ----
    {
        const ImVec2 rMin(origin.x, origin.y + RS);
        const ImVec2 rMax(origin.x + RS, origin.y + size.y);
        dl->AddRectFilled(rMin, rMax, kBg);
        dl->AddLine({rMax.x, rMin.y}, {rMax.x, rMax.y}, kBord);

        float u = std::floor(unitMinY / minorInterval) * minorInterval;
        for (; u <= unitMaxY; u += minorInterval) {
            const float py = toScreenY(u);
            if (py < rMin.y || py > rMax.y) continue;
            const bool  isMajor = std::fmod(std::fabs(u), majorInterval) < minorInterval * 0.5f;
            const float tickLen = isMajor ? RS * 0.65f : RS * 0.3f;
            dl->AddLine({rMax.x - tickLen, py}, {rMax.x, py}, kTick);
            if (isMajor) {
                char buf[20]; std::snprintf(buf, sizeof(buf), "%.4g%s", u, unitLabel);
                dl->AddText(fn, fs, {origin.x + 1.f, py + 2.f}, kLabel, buf);
            }
        }

        const float cy = toScreenY(my);
        if (cy >= rMin.y && cy <= rMax.y) {
            dl->AddLine({origin.x, cy}, {origin.x + RS, cy}, kCurs, 1.5f);
            char buf[24]; std::snprintf(buf, sizeof(buf), "%.4g%s", my, unitLabel);
            dl->AddText(fn, fs, {origin.x + 1.f, cy + 2.f}, kCurs, buf);
        }
    }

    // ---- Corner square ----
    {
        dl->AddRectFilled(origin, {origin.x + RS, origin.y + RS}, kBg);
        dl->AddLine({origin.x + RS, origin.y},   {origin.x + RS, origin.y + RS}, kBord);
        dl->AddLine({origin.x,      origin.y + RS},{origin.x + RS, origin.y + RS}, kBord);

        if (mouse.x >= origin.x + RS && mouse.y >= origin.y + RS) {
            char buf[32]; std::snprintf(buf, sizeof(buf), "%.4g,%.4g", mx, my);
            const float tw = ImGui::CalcTextSize(buf).x * (fs / ImGui::GetFontSize());
            dl->AddText(fn, fs, {mouse.x - tw * 0.5f, mouse.y - RS - 2.f}, kCurs, buf);
        }
    }
}

// ============================================================================
// drawMarginRect
// ============================================================================

void drawMarginRect(ImDrawList* dl,
                    ImVec2      screenMin,
                    ImVec2      screenMax,
                    ImU32       color,
                    float       thickness)
{
    if (!dl) return;
    if (screenMax.x <= screenMin.x || screenMax.y <= screenMin.y) return;
    dl->AddRect(screenMin, screenMax, color, 0.f, 0, thickness);
}

} // namespace ofkitty
