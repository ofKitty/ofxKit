#pragma once
//
//  ImGridGuides.h  —  ofxKit
//
//  Immediate-mode grid, margin and snap-guide overlay drawn via ImDrawList.
//  Follows the Im* naming convention (no enclosing namespace).
//
//  All rendering is in screen-space using a caller-supplied content→screen
//  mapping (ox, oy, zoom):
//    screen.x = ox + content.x * zoom
//    screen.y = oy + content.y * zoom
//
//  Visible-range culling is applied automatically: only lines that fall
//  within the clip rectangle are emitted, so the line count is bounded by
//  screen pixel dimensions, never by paper size.
//
//  Usage:
//    ImGridGuides g;
//    g.showGrid = true;
//    // each frame (in overlayDraw or equivalent):
//    g.draw(dl, paperW, paperH, vp._ox, vp._oy, vp._zoom,
//           vp.canvasOriginPx().x, vp.canvasOriginPx().y,
//           vp.canvasW(), vp.canvasH());
//
#include "imgui.h"
#include <vector>
#include <cmath>
#include <algorithm>

struct ImGridGuides {

    // ---- Grid ---------------------------------------------------------------
    bool  showGrid          = false;
    float grid1mmZoomThresh = 3.f;   ///< zoom at which 1mm lines become visible

    // Grid colours (RGBA)
    ImU32 col1mm   = IM_COL32(200, 220, 240,  60);
    ImU32 col10mm  = IM_COL32(180, 205, 230, 100);
    ImU32 col50mm  = IM_COL32(150, 185, 215, 140);
    ImU32 colCentre= IM_COL32(200, 100, 100,  80);
    ImU32 colOrigin= IM_COL32(220,  60,  60, 180);  ///< X axis tick at origin
    ImU32 colOriginY=IM_COL32( 60, 220,  60, 180);  ///< Y axis tick at origin

    // ---- Margins ------------------------------------------------------------
    bool  showMargins  = false;
    float marginTop    = 0.f;   ///< mm from paper top
    float marginRight  = 0.f;   ///< mm from paper right
    float marginBottom = 0.f;   ///< mm from paper bottom
    float marginLeft   = 0.f;   ///< mm from paper left
    ImU32 colMargin    = IM_COL32(180, 80, 220, 180);

    // ---- User snap guides ---------------------------------------------------
    std::vector<float> guidesX;  ///< vertical lines at these content-mm X positions
    std::vector<float> guidesY;  ///< horizontal lines at these content-mm Y positions
    ImU32 colGuide = IM_COL32(0, 190, 255, 160);

    // -------------------------------------------------------------------------
    /// Draw all enabled overlays.
    /// @param dl        ImGui draw list (e.g. ImGui::GetWindowDrawList()).
    /// @param paperW/H  Content dimensions in mm (or whatever unit zoom/ox/oy use).
    /// @param ox, oy    Screen position of the content origin (0,0).
    /// @param zoom      Screen pixels per content unit.
    /// @param clipX/Y   Top-left of the visible canvas region in screen pixels.
    /// @param clipW/H   Width/height of the visible canvas region in screen pixels.
    // -------------------------------------------------------------------------
    void draw(ImDrawList* dl,
              float paperW, float paperH,
              float ox, float oy, float zoom,
              float clipX, float clipY, float clipW, float clipH) const
    {
        if (!dl || zoom <= 0.f) return;

        // Visible content range (in mm / content units)
        const float visXmin = (clipX - ox) / zoom;
        const float visXmax = (clipX + clipW - ox) / zoom;
        const float visYmin = (clipY - oy) / zoom;
        const float visYmax = (clipY + clipH - oy) / zoom;

        // Clamp to paper bounds (no lines outside paper)
        const float xMin = std::max(0.f, visXmin);
        const float xMax = std::min(paperW, visXmax);
        const float yMin = std::max(0.f, visYmin);
        const float yMax = std::min(paperH, visYmax);

        // Helper: screen coords from content coords
        auto sx = [&](float cx) { return ox + cx * zoom; };
        auto sy = [&](float cy) { return oy + cy * zoom; };

        // Helper: draw one tier of grid lines
        auto drawGridTier = [&](float step, ImU32 col, float thickness) {
            if (step <= 0.f) return;
            // Vertical lines
            const float xFirst = std::ceil(xMin / step) * step;
            for (float x = xFirst; x <= xMax + 1e-4f; x += step)
                dl->AddLine({sx(x), sy(yMin)}, {sx(x), sy(yMax)}, col, thickness);
            // Horizontal lines
            const float yFirst = std::ceil(yMin / step) * step;
            for (float y = yFirst; y <= yMax + 1e-4f; y += step)
                dl->AddLine({sx(xMin), sy(y)}, {sx(xMax), sy(y)}, col, thickness);
        };

        if (showGrid) {
            // 1mm lines — only when zoomed in enough
            if (zoom >= grid1mmZoomThresh)
                drawGridTier(1.f,  col1mm,  0.5f);

            drawGridTier(10.f, col10mm, 1.0f);
            drawGridTier(50.f, col50mm, 1.5f);

            // Centre lines (full paper width/height)
            if (paperW * 0.5f >= xMin && paperW * 0.5f <= xMax)
                dl->AddLine({sx(paperW * 0.5f), sy(yMin)},
                            {sx(paperW * 0.5f), sy(yMax)}, colCentre, 1.5f);
            if (paperH * 0.5f >= yMin && paperH * 0.5f <= yMax)
                dl->AddLine({sx(xMin), sy(paperH * 0.5f)},
                            {sx(xMax), sy(paperH * 0.5f)}, colCentre, 1.5f);

            // Origin cross (small tick marks at (0,0))
            const float ms = 5.f; // tick length in mm
            if (0.f >= xMin && ms <= xMax)
                dl->AddLine({sx(0.f), sy(0.f)}, {sx(ms), sy(0.f)}, colOrigin,  2.f);
            if (0.f >= yMin && ms <= yMax)
                dl->AddLine({sx(0.f), sy(0.f)}, {sx(0.f), sy(ms)}, colOriginY, 2.f);
        }

        // Margins
        if (showMargins) {
            const float ml = marginLeft,  mr = paperW - marginRight;
            const float mt = marginTop,   mb = paperH - marginBottom;
            dl->AddRect({sx(ml), sy(mt)}, {sx(mr), sy(mb)}, colMargin, 0.f, 0, 1.5f);
        }

        // User snap guides
        if (!guidesX.empty() || !guidesY.empty()) {
            for (float gx : guidesX)
                if (gx >= visXmin && gx <= visXmax)
                    dl->AddLine({sx(gx), sy(visYmin)}, {sx(gx), sy(visYmax)},
                                colGuide, 1.f);
            for (float gy : guidesY)
                if (gy >= visYmin && gy <= visYmax)
                    dl->AddLine({sx(visXmin), sy(gy)}, {sx(visXmax), sy(gy)},
                                colGuide, 1.f);
        }
    }
};
