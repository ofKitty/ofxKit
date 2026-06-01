#pragma once

#include "View2DState.h"
#include "ofEvents.h"

namespace ofkitty {

/// Drives a View2DState from OpenFrameworks mouse events.
///
/// Usage in ofApp:
///
///   void setup() {
///       m_view.contentSize = { paperW, paperH };
///       m_adapter.attach(m_view);
///       ofAddListener(ofEvents().mouseScrolled, this, &ofApp::mouseScrolled);
///       // etc.
///   }
///   void draw() {
///       // Set canvas geometry every frame from the OF window
///       m_view.canvasOrigin = { 0.f, 0.f };
///       m_view.canvasW      = ofGetWidth();
///       m_view.canvasH      = ofGetHeight();
///       m_view.updateDerived();
///       // now m_view.toScreen() / toContent() are valid
///   }
///   void mouseScrolled(ofMouseEventArgs& e) { m_adapter.mouseScrolled(e); }
///   void mouseDragged(ofMouseEventArgs& e)  { m_adapter.mouseDragged(e); }
///   void mouseDoubleClicked(ofMouseEventArgs& e) { m_adapter.mouseDoubleClicked(e); }
///
class View2DMouseAdapter {
public:
    /// Whether middle-mouse-button or Alt+LMB triggers pan.
    bool panOnMiddle = true;
    bool panOnAltLMB = true;

    void attach(View2DState& view) { view_ = &view; }

    void mouseScrolled(ofMouseEventArgs& e) {
        if (!view_ || !view_->hovered) return;
        view_->applyScrollZoom(e.scrollY, static_cast<float>(e.x), static_cast<float>(e.y));
    }

    void mouseDragged(ofMouseEventArgs& e) {
        if (!view_) return;
        const bool isPan = (panOnMiddle && e.button == OF_MOUSE_BUTTON_MIDDLE)
                        || (panOnAltLMB && e.button == OF_MOUSE_BUTTON_LEFT
                            && (ofGetKeyPressed(OF_KEY_ALT)));
        if (isPan) {
            view_->applyPanDelta(
                static_cast<float>(e.x) - prevX_,
                static_cast<float>(e.y) - prevY_);
        }
        prevX_ = static_cast<float>(e.x);
        prevY_ = static_cast<float>(e.y);
    }

    void mousePressed(ofMouseEventArgs& e) {
        prevX_ = static_cast<float>(e.x);
        prevY_ = static_cast<float>(e.y);
    }

    void mouseDoubleClicked(ofMouseEventArgs& e) {
        if (!view_ || !view_->hovered) return;
        if (e.button == OF_MOUSE_BUTTON_LEFT)
            // TODO Don't like this...
            // maybe right mouse click submenu View -> Fit to Canvas?
            // This needs to be optional and settible. Register keyboard shortcut?
            view_->fitToCanvas();
    }

private:
    View2DState* view_ = nullptr;
    float prevX_ = 0.f;
    float prevY_ = 0.f;
};

} // namespace ofkitty
