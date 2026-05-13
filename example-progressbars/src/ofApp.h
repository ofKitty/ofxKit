#pragma once
// ============================================================================
//  example-progressbars
//
//  Demonstrates the ofkitty::ProgressWindow API:
//    • Step-based  — begin(title, N) + tick(label)
//    • Absolute    — begin(title)   + tick(label, 0..1)
//    • Indeterminate (animated spinner)
//    • Bottom-anchored vs floating
//    • cancel / hide
//    • configurable auto-hide delay
// ============================================================================

#include "ofMain.h"
#include "ofxKit.h"

class ofApp : public ofBaseApp {
public:
    void setup()  override;
    void update() override;
    void draw()   override;

private:
    // ---- simulation state ----
    enum class Demo { None, StepBased, Absolute, Indeterminate };

    Demo  m_activeDemo      {Demo::None};
    int   m_stepCurrent     {0};
    int   m_stepTotal       {80};
    float m_absolutePos     {0.f};   // 0..1 driven by the animation
    float m_stepAccumulator {0.f};   // fractional step carry-over

    // ---- control-panel state (ImGui) ----
    float m_simulationSpeed {1.0f};
    bool  m_bottomAnchored  {true};
    float m_autoHideDelay   {2.0f};
};
