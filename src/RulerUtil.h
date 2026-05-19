#pragma once

#include "imgui.h"
#include <vector>

namespace ofkitty {

// ============================================================================
// Guide state — owns the guide list and internal drag bookkeeping.
// Declare one per panel that supports guides and pass a pointer to
// drawRulersInRegion each frame.
// ============================================================================

struct GuideSet {
    std::vector<float> h;       ///< Horizontal guide Y positions (content units)
    std::vector<float> v;       ///< Vertical   guide X positions (content units)
    bool  visible = true;
    ImU32 color   = IM_COL32(0, 190, 255, 160);

    // --- internal drag state (managed by drawRulersInRegion) --- //
    bool  dragging  = false;
    bool  dragIsH   = false; ///< true = horizontal guide (from top ruler)
    int   dragIndex = -1;    ///< -1 = creating new; ≥0 = moving existing
    float dragValue = 0.f;   ///< live position in content units while dragging
};

// ============================================================================
// drawRulersInRegion
//
// Draw pixel/unit rulers along the top and left edges of an arbitrary screen
// region, and optionally manage a set of draggable guides.
//
// @param dl          Draw list (e.g. GetWindowDrawList()).
// @param origin      Screen-space top-left of the content rectangle.
// @param size        Content rectangle in screen pixels (including ruler strips).
// @param mouse       Current mouse screen position (ImGui::GetIO().MousePos).
// @param pixPerUnit  Screen pixels per one ruler unit.
//                    1.0 = screen pixels; pass mm→screen scale for plotter panels.
// @param unitLabel   Unit suffix shown on labels (e.g. "px", "mm").
// @param uiScale     Global UI scale (runtime().uiScale()).
// @param rulerScale  Per-ruler thickness multiplier (AppPrefs::rulerScale).
// @param guides      Optional guide set.  Pass nullptr to disable guides.
//                    Drag from a ruler strip to create a guide; drag an
//                    existing guide to move it; drag it outside the content
//                    area to delete it; right-click a guide to delete it.
// @param scrollPx    Optional scroll/pan offset in screen pixels.
//                    Shifts where ruler tick-0 appears within the content area.
//                    Useful when the document origin is panned away from the
//                    content top-left (e.g. a centred/zoomed canvas).
//                    Default {0,0} keeps the current behaviour.
// ============================================================================

void drawRulersInRegion(ImDrawList*  dl,
                        ImVec2       origin,
                        ImVec2       size,
                        ImVec2       mouse,
                        float        pixPerUnit,
                        const char*  unitLabel,
                        float        uiScale,
                        float        rulerScale,
                        GuideSet*    guides  = nullptr,
                        ImVec2       scrollPx = {0.f, 0.f});

// ============================================================================
// drawMarginRect
//
// Stroke a rectangle indicating the printable / safe area inside a canvas.
// Visually similar to a guide line, but typically rendered in a distinct hue
// (e.g. purple) so the user can tell margins apart from draggable guides.
//
// The rectangle is given in screen coordinates — the caller is responsible
// for converting the document-space margin (e.g. paper inset by `marginMM`)
// into screen pixels using whatever zoom/pan transform the panel uses.
//
// @param dl         Draw list (e.g. GetWindowDrawList()).
// @param screenMin  Screen-space top-left of the margin rectangle.
// @param screenMax  Screen-space bottom-right of the margin rectangle.
// @param color      Stroke colour (premultiplied ImU32, e.g. IM_COL32(...)).
// @param thickness  Line thickness in screen pixels (default 1.0).
// ============================================================================

void drawMarginRect(ImDrawList* dl,
                    ImVec2      screenMin,
                    ImVec2      screenMax,
                    ImU32       color,
                    float       thickness = 1.0f);

} // namespace ofkitty
