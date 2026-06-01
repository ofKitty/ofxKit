#pragma once

#include "ofMain.h"

namespace ofkitty {

/// Pan/zoom coordinate state for a 2D canvas view.
///
/// Shared by both usage patterns:
///   - ViewportInstance (ImGui FBO panel) — Runtime::drawViewportWindow2D populates it
///   - OF-space main viewport — app drives it directly from ofMouseEventArgs
///
/// Coordinate convention (Y-DOWN, matching both OF screen and ImGui):
///   screen  = (ox + content.x * zoom,  oy + content.y * zoom)
///   content = ((screen.x - ox) / zoom, (screen.y - oy) / zoom)
///
/// "Content" units are whatever the renderer uses (mm, px, …).
/// "Screen" units are window pixels (the same space as ofGetMouseX/Y).
struct View2DState {

    // ---- Configuration (set once) -------------------------------------------

    /// Logical size of the content in content units (e.g. paper mm).
    glm::vec2 contentSize = {200.f, 200.f};

    /// Minimum / maximum zoom multiplier (on top of the computed fit zoom).
    float zoomMin = 0.1f;
    float zoomMax = 50.f;

    // ---- Persistent state (survives frames) ----------------------------------

    /// Pan offset in screen pixels from the fitted-centre position.
    glm::vec2 pan  = {};

    /// Zoom multiplier on top of the "fit" zoom.
    /// 1.0 = content fills the viewport; >1 = zoomed in.
    float     zoom = 1.f;

    // ---- Per-frame viewport geometry (set by the driver each frame) ----------

    /// Top-left corner of the canvas area in screen pixels.
    glm::vec2 canvasOrigin = {};
    /// Canvas width and height in screen pixels.
    float     canvasW = 1.f;
    float     canvasH = 1.f;

    // ---- Derived state (recomputed each frame by updateDerived) --------------

    /// Origin and zoom in screen-pixel space.
    float ox   = 0.f;
    float oy   = 0.f;
    float zoom_ = 1.f;  ///< absolute zoom (fitZoom * zoom)

    /// True when the pointer is over the canvas (set by the driver).
    bool  hovered = false;

    // ---- Core math ----------------------------------------------------------

    /// Fit zoom: largest multiplier so that content fills the canvas.
    float fitZoom() const {
        if (contentSize.x <= 0.f || contentSize.y <= 0.f) return 1.f;
        return std::min(canvasW / contentSize.x, canvasH / contentSize.y);
    }

    /// Recompute ox/oy/zoom_ from pan + zoom + canvas geometry.
    /// Call once per frame after updating canvasOrigin/canvasW/canvasH.
    void updateDerived() {
        const float fz = fitZoom();
        zoom_ = fz * zoom;
        ox = canvasOrigin.x + canvasW * 0.5f + pan.x - contentSize.x * zoom_ * 0.5f;
        oy = canvasOrigin.y + canvasH * 0.5f + pan.y - contentSize.y * zoom_ * 0.5f;
    }

    // ---- Coordinate converters (valid after updateDerived) ------------------

    glm::vec2 toScreen (float cx, float cy) const noexcept
        { return { ox + cx * zoom_, oy + cy * zoom_ }; }

    glm::vec2 toContent(float sx, float sy) const noexcept
        { return (zoom_ > 0.f) ? glm::vec2{ (sx - ox) / zoom_, (sy - oy) / zoom_ }
                                : glm::vec2{}; }

    float contentZoom() const noexcept { return zoom_; }

    // ---- Input helpers (call from wherever input is available) ---------------

    /// Zoom around a screen-space pivot point (e.g. mouse cursor).
    /// @p scrollDelta  positive = zoom in, negative = zoom out (wheel units).
    /// @p pivotSx/Sy   screen-pixel cursor position.
    void applyScrollZoom(float scrollDelta, float pivotSx, float pivotSy) {
        if (scrollDelta == 0.f) return;
        const float factor  = (scrollDelta > 0.f) ? 1.15f : 1.f / 1.15f;
        const float fz      = fitZoom();
        const float zoomOld = fz * zoom;
        const float pxOld   = canvasW * 0.5f + pan.x - contentSize.x * zoomOld * 0.5f;
        const float pyOld   = canvasH * 0.5f + pan.y - contentSize.y * zoomOld * 0.5f;
        const float mu      = (pivotSx - canvasOrigin.x - pxOld) / zoomOld;
        const float mv      = (pivotSy - canvasOrigin.y - pyOld) / zoomOld;
        zoom                = std::clamp(zoom * factor, zoomMin, zoomMax);
        const float zoomNew = fz * zoom;
        pan.x = pivotSx - canvasOrigin.x - mu * zoomNew - canvasW * 0.5f + contentSize.x * zoomNew * 0.5f;
        pan.y = pivotSy - canvasOrigin.y - mv * zoomNew - canvasH * 0.5f + contentSize.y * zoomNew * 0.5f;
        updateDerived();
    }

    /// Pan by a screen-pixel delta (e.g. from mouse drag).
    void applyPanDelta(float dx, float dy) {
        pan.x += dx;
        pan.y += dy;
        updateDerived();
    }

    /// Reset pan and zoom so content fits the canvas.
    void fitToCanvas() {
        pan  = {};
        zoom = 1.f;
        updateDerived();
    }
};

} // namespace ofkitty
