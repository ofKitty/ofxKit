#include "MarqueeSelect2D.h"

#include "ofMain.h"
#include "imgui.h"

namespace ofkitty {

void MarqueeSelect2D::cancel()
{
    pending_ = false;
    active_  = false;
}

void MarqueeSelect2D::update(bool      canBegin,
                             bool      canFinish,
                             glm::vec2 pt,
                             bool      mouseClicked,
                             bool      mouseDown,
                             bool      mouseReleased,
                             glm::vec2 mouseDragDelta,
                             bool      shiftHeld,
                             const std::function<bool()>&                           beginPredicate,
                             const std::function<void(glm::vec2, bool)>&            onClick,
                             const std::function<void(glm::vec2, glm::vec2, bool)>& onMarquee)
{
    if (canBegin && mouseClicked && beginPredicate && beginPredicate()) {
        start_   = pt;
        end_     = pt;
        pending_ = true;
        active_  = false;
    }

    if (pending_ && mouseDown) {
        end_ = pt;
        const float dragSq = mouseDragDelta.x * mouseDragDelta.x
                           + mouseDragDelta.y * mouseDragDelta.y;
        if (dragSq >= dragThresholdSq)
            active_ = true;
    }

    if (pending_ && mouseReleased) {
        if (canFinish) {
            if (active_) {
                if (onMarquee)
                    onMarquee(start_, end_, shiftHeld);
            } else if (onClick) {
                onClick(end_, shiftHeld);
            }
        }
        pending_ = false;
        active_  = false;
    }
}

// ---------------------------------------------------------------------------
// OF draw — default path, called from ofApp::draw()
// ---------------------------------------------------------------------------

void MarqueeSelect2D::draw(const std::function<glm::vec2(float, float)>& toScreen) const
{
    if (!active_ || !toScreen) return;

    const glm::vec2 s0 = toScreen(start_.x, start_.y);
    const glm::vec2 s1 = toScreen(end_.x,   end_.y);

    const float x = std::min(s0.x, s1.x);
    const float y = std::min(s0.y, s1.y);
    const float w = std::abs(s1.x - s0.x);
    const float h = std::abs(s1.y - s0.y);

    ofPushStyle();

    ofSetColor(style.fillR, style.fillG, style.fillB, style.fillA);
    ofFill();
    ofDrawRectangle(x, y, w, h);

    ofSetColor(style.lineR, style.lineG, style.lineB, style.lineA);
    ofSetLineWidth(style.lineWidth);
    ofNoFill();
    ofDrawRectangle(x, y, w, h);

    ofPopStyle();
}

// ---------------------------------------------------------------------------
// ImGui draw — called from inside an ImGui overlayDraw / window
// ---------------------------------------------------------------------------

void MarqueeSelect2D::drawImGui(ImDrawList* dl,
                                const std::function<ImVec2(float, float)>& toScreen) const
{
    if (!active_ || !dl || !toScreen) return;

    // Lazily build ImU32 colours from the RGBA bytes the first time.
    if (style.imFillColor == 0)
        style.imFillColor = IM_COL32(style.fillR, style.fillG, style.fillB, style.fillA);
    if (style.imStrokeColor == 0)
        style.imStrokeColor = IM_COL32(style.lineR, style.lineG, style.lineB, style.lineA);

    const ImVec2 s0 = toScreen(start_.x, start_.y);
    const ImVec2 s1 = toScreen(end_.x,   end_.y);
    dl->AddRectFilled(s0, s1, style.imFillColor);
    dl->AddRect(s0, s1, style.imStrokeColor, 0.f, 0, style.lineWidth);
}

} // namespace ofkitty
