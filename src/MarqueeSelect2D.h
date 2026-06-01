#pragma once

#include "ofMain.h"
#include <functional>

struct ImDrawList;
struct ImVec2;
using ImU32 = unsigned int;

namespace ofkitty {

/// Click-vs-drag rectangle selection in an arbitrary 2D coordinate space.
///
/// Usage:
///   1. Map the mouse to your pick space (paper mm, world units, …).
///   2. Call update() each frame with booleans from your input source.
///   3. Call draw()      from ofApp::draw() for OF rendering, or
///      call drawImGui() from an ImGui overlayDraw callback.
class MarqueeSelect2D {
public:
    struct Style {
        // OF colours (0–255 RGBA)
        unsigned char fillR  = 100, fillG  = 180, fillB  = 255, fillA  = 30;
        unsigned char lineR  = 100, lineG  = 180, lineB  = 255, lineA  = 200;
        float         lineWidth = 1.5f;

        // ImGui colours — only used by drawImGui(); defaults match the OF colours above.
        mutable ImU32 imFillColor   = 0;  ///< 0 = derive from fill RGBA on first drawImGui() call
        mutable ImU32 imStrokeColor = 0;  ///< 0 = derive from line RGBA on first drawImGui() call
    };

    /// Squared pixel drag before the gesture becomes a marquee (default = 4 px).
    float dragThresholdSq = 16.f;

    Style style;

    bool isPending() const { return pending_; }
    bool isActive()  const { return active_;  }

    void cancel();

    /// Drive state each frame. @p pt is in the caller's selection space.
    /// All inputs are plain booleans/floats — works with both ImGui and OF mouse events.
    void update(bool       canBegin,
                bool       canFinish,
                glm::vec2  pt,
                bool       mouseClicked,
                bool       mouseDown,
                bool       mouseReleased,
                glm::vec2  mouseDragDelta,   ///< screen-pixel drag since button-down
                bool       shiftHeld,
                const std::function<bool()>&                                    beginPredicate,
                const std::function<void(glm::vec2, bool)>&                     onClick,
                const std::function<void(glm::vec2, glm::vec2, bool)>&          onMarquee);

    /// Draw the marquee rect using OF drawing calls.
    /// @p toScreen maps a selection-space point to OF screen pixels (glm::vec2).
    void draw(const std::function<glm::vec2(float x, float y)>& toScreen) const;

    /// Draw the marquee rect into an ImGui draw list.
    /// @p toScreen maps a selection-space point to ImVec2 screen pixels.
    void drawImGui(ImDrawList* dl,
                   const std::function<ImVec2(float x, float y)>& toScreen) const;

private:
    bool      pending_ = false;
    bool      active_  = false;
    glm::vec2 start_   {};
    glm::vec2 end_     {};
};

} // namespace ofkitty
