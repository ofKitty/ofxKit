#pragma once
// ============================================================================
//  example-progressbars
//
//  Demonstrates the ofkitty::ProgressWindow API:
//    • Step-based  — begin(title, N) + tick(label)
//    • Absolute    — begin(title)   + tick(label, 0..1)
//    • Indeterminate (animated spinner)
//    • Status bar vs floating progress display
//    • cancel / hide
//    • configurable auto-hide delay
//
//  Registers a Runtime “Test window” (View menu) with buttons for fake loaders;
//  starts in edit mode with that panel docked left on a fresh default layout.
// ============================================================================

#include "ofMain.h"
#include "ofxKit.h"

class ofApp : public ofBaseApp {
public:
    void setup()  override;
    void update() override;
    void draw()   override;

private:
    /// 0..1 while a demo reports progress; negative when idle (background uses a slow ambient motion).
    float canvasProgressFraction() const;
    // ---- simulation state ----
    enum class Demo { None, StepBased, Absolute, Indeterminate };

    Demo  m_activeDemo      {Demo::None};
    int   m_stepCurrent     {0};
    int   m_stepTotal       {80};
    float m_absolutePos     {0.f};   // 0..1 driven by the animation
    float m_stepAccumulator {0.f};   // fractional step carry-over

    // ---- control-panel state (ImGui) ----
    float m_simulationSpeed {1.0f};
    bool  m_useStatusBar     {true};
    float m_autoHideDelay   {2.0f};
};
