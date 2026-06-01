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
//    g.draw(dl, paperW, paperH, vp.view2D.ox, vp.view2D.oy, vp.contentZoom(),
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

    /// Paper / grid area within preview content coords (mm). When zero, the
    /// caller's paperW/H default to the full viewport content size.
    float areaOffsetX = 0.f;
    float areaOffsetY = 0.f;
    float paperW      = 0.f;
    float paperH      = 0.f;

    /// When true, paper-local Y=0 is the bottom edge (GRBL / Y+ up preview).
    bool  paperYAxisUp  = false;

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

        // Paper-local range (0..paperW/H within the preview content frame).
        const float localXmin = std::max(0.f, visXmin - areaOffsetX);
        const float localXmax = std::min(paperW, visXmax - areaOffsetX);
        const float localYmin = std::max(0.f, visYmin - areaOffsetY);
        const float localYmax = std::min(paperH, visYmax - areaOffsetY);

        // Helper: screen coords from paper-local coords (Y-down content space).
        auto sx = [&](float cx) { return ox + (areaOffsetX + cx) * zoom; };
        auto sy = [&](float cy) {
            const float localY = paperYAxisUp ? (paperH - cy) : cy;
            return oy + (areaOffsetY + localY) * zoom;
        };

        // Helper: draw one tier of grid lines
        auto drawGridTier = [&](float step, ImU32 col, float thickness) {
            if (step <= 0.f) return;
            // Vertical lines
            const float xFirst = std::ceil(localXmin / step) * step;
            for (float x = xFirst; x <= localXmax + 1e-4f; x += step)
                dl->AddLine({sx(x), sy(localYmin)}, {sx(x), sy(localYmax)}, col, thickness);
            // Horizontal lines
            const float yFirst = std::ceil(localYmin / step) * step;
            for (float y = yFirst; y <= localYmax + 1e-4f; y += step)
                dl->AddLine({sx(localXmin), sy(y)}, {sx(localXmax), sy(y)}, col, thickness);
        };

        if (showGrid) {
            // 1mm lines — only when zoomed in enough
            if (zoom >= grid1mmZoomThresh)
                drawGridTier(1.f,  col1mm,  0.5f);

            drawGridTier(10.f, col10mm, 1.0f);
            drawGridTier(50.f, col50mm, 1.5f);

            // Centre lines (full paper width/height)
            if (paperW * 0.5f >= localXmin && paperW * 0.5f <= localXmax)
                dl->AddLine({sx(paperW * 0.5f), sy(localYmin)},
                            {sx(paperW * 0.5f), sy(localYmax)}, colCentre, 1.5f);
            if (paperH * 0.5f >= localYmin && paperH * 0.5f <= localYmax)
                dl->AddLine({sx(localXmin), sy(paperH * 0.5f)},
                            {sx(localXmax), sy(paperH * 0.5f)}, colCentre, 1.5f);

            // Origin cross (small tick marks at paper top-left)
            const float ms = 5.f; // tick length in mm
            if (localXmin <= 0.f && ms <= localXmax)
                dl->AddLine({sx(0.f), sy(0.f)}, {sx(ms), sy(0.f)}, colOrigin,  2.f);
            if (localYmin <= 0.f && ms <= localYmax)
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
