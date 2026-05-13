#include "ofApp.h"

// ============================================================================
//  example-progressbars
//
//  Buttons in the "Progress Demos" panel trigger simulated operations.
//  The ProgressWindow draws at the bottom of the screen (or floating if you
//  toggle the checkbox) and auto-hides after the configured delay.
// ============================================================================

void ofApp::setup()
{
    ofBackground(40, 42, 54);
    ofSetWindowTitle("ofxKit — Progress Window Demo");

    ofkitty::progress().setBottomAnchored(m_bottomAnchored);
    ofkitty::progress().setAutoHideDelay(m_autoHideDelay);

    // Register the demo control panel with the Runtime overlay.
    ofkitty::runtime().registerWindow({
        "Progress Demos",
        "View",
        /*visible=*/    true,
        /*editModeOnly=*/ false,
        [this](bool& visible) {

            ImGui::SetNextWindowPos(ImVec2(20, 40), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Once);

            if (!ImGui::Begin("Progress Demos", &visible)) {
                ImGui::End();
                return;
            }

            // ---- Options ----
            ImGui::SeparatorText("Options");

            if (ImGui::Checkbox("Bottom-anchored", &m_bottomAnchored)) {
                ofkitty::progress().setBottomAnchored(m_bottomAnchored);
            }
            ImGui::SetItemTooltip("When ON the bar sticks to the viewport bottom edge.\n"
                                  "When OFF it floats as a normal window.");

            ImGui::SetNextItemWidth(160.f);
            if (ImGui::SliderFloat("Auto-hide delay (s)", &m_autoHideDelay, 0.f, 8.f, "%.1f s")) {
                ofkitty::progress().setAutoHideDelay(m_autoHideDelay);
            }

            ImGui::SetNextItemWidth(160.f);
            ImGui::SliderFloat("Sim speed", &m_simulationSpeed, 0.1f, 5.f, "%.1f×");
            ImGui::SetItemTooltip("Multiplier applied to the simulated step rate.");

            // ---- Demos ----
            ImGui::Spacing();
            ImGui::SeparatorText("Demos");

            // Step-based demo
            bool stepRunning = (m_activeDemo == Demo::StepBased);
            if (stepRunning) ImGui::BeginDisabled();
            if (ImGui::Button("Step-based  (80 steps)", ImVec2(-1, 0))) {
                m_activeDemo  = Demo::StepBased;
                m_stepCurrent = 0;
                ofkitty::progress().begin("Exporting layers", m_stepTotal);
            }
            if (stepRunning) ImGui::EndDisabled();
            ImGui::SetItemTooltip("begin(title, N)  +  tick(label)  — typical batch export.");

            ImGui::Spacing();

            // Absolute demo
            bool absRunning = (m_activeDemo == Demo::Absolute);
            if (absRunning) ImGui::BeginDisabled();
            if (ImGui::Button("Absolute progress", ImVec2(-1, 0))) {
                m_activeDemo  = Demo::Absolute;
                m_absolutePos = 0.f;
                ofkitty::progress().begin("Rendering frames");
            }
            if (absRunning) ImGui::EndDisabled();
            ImGui::SetItemTooltip("begin(title)  +  tick(label, 0..1)  — when total isn't known up front.");

            ImGui::Spacing();

            // Indeterminate demo
            bool indRunning = (m_activeDemo == Demo::Indeterminate);
            if (indRunning) ImGui::BeginDisabled();
            if (ImGui::Button("Indeterminate  (4 s)", ImVec2(-1, 0))) {
                m_activeDemo  = Demo::Indeterminate;
                m_absolutePos = 0.f;
                ofkitty::progress().begin("Connecting…");
                ofkitty::progress().tickIndeterminate("Please wait…");
            }
            if (indRunning) ImGui::EndDisabled();
            ImGui::SetItemTooltip("tickIndeterminate(label)  — animated marquee, no fraction known.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Cancel / hide
            bool anyRunning = (m_activeDemo != Demo::None);
            if (!anyRunning) ImGui::BeginDisabled();
            if (ImGui::Button("Cancel / hide", ImVec2(-1, 0))) {
                m_activeDemo = Demo::None;
                ofkitty::progress().hide();
            }
            if (!anyRunning) ImGui::EndDisabled();

            ImGui::End();
        }
    });
}

// ----------------------------------------------------------------------------
//  update — advance the simulated demo
// ----------------------------------------------------------------------------

void ofApp::update()
{
    const float dt    = ofGetLastFrameTime();
    const float speed = m_simulationSpeed;

    switch (m_activeDemo) {

    case Demo::StepBased: {
        // Advance approximately (speed * 12) steps per second.
        m_stepAccumulator += dt * speed * 12.f;
        while (m_stepAccumulator >= 1.f && m_stepCurrent < m_stepTotal) {
            m_stepAccumulator -= 1.f;
            ++m_stepCurrent;
            std::string label = "Layer " + ofToString(m_stepCurrent)
                              + " / " + ofToString(m_stepTotal);
            ofkitty::progress().tick(label);
        }
        if (m_stepCurrent >= m_stepTotal) {
            m_activeDemo      = Demo::None;
            m_stepAccumulator = 0.f;
            ofkitty::progress().finish("Export complete");
        }
        break;
    }

    case Demo::Absolute: {
        m_absolutePos += dt * speed * 0.18f;
        if (m_absolutePos < 1.f) {
            int frame  = static_cast<int>(m_absolutePos * 120.f);
            std::string label = "Frame " + ofToString(frame) + " / 120";
            ofkitty::progress().tick(label, m_absolutePos);
        } else {
            m_activeDemo  = Demo::None;
            m_absolutePos = 0.f;
            ofkitty::progress().finish("Render done");
        }
        break;
    }

    case Demo::Indeterminate: {
        // Run for 4 seconds then finish.
        m_absolutePos += dt * speed;
        if (m_absolutePos < 4.f) {
            ofkitty::progress().tickIndeterminate("Please wait…");
        } else {
            m_activeDemo  = Demo::None;
            m_absolutePos = 0.f;
            ofkitty::progress().finish("Connected");
        }
        break;
    }

    case Demo::None:
    default:
        break;
    }
}

// ----------------------------------------------------------------------------
//  draw — background canvas
// ----------------------------------------------------------------------------

void ofApp::draw()
{
    // Simple gradient backdrop so the bottom bar is visible against the canvas.
    ofMesh bg;
    bg.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
    float w = ofGetWidth(), h = ofGetHeight();
    bg.addVertex({0,   0,   0}); bg.addColor({40,  42,  54, 255});
    bg.addVertex({w,   0,   0}); bg.addColor({40,  42,  54, 255});
    bg.addVertex({w,   h,   0}); bg.addColor({20,  22,  32, 255});
    bg.addVertex({0,   h,   0}); bg.addColor({20,  22,  32, 255});
    bg.draw();

    ofSetColor(120, 122, 140);
    ofDrawBitmapString(
        "ofxKit — Progress Window Demo\n"
        "Open the 'Progress Demos' panel (View menu) to trigger demos.\n"
        "Cmd-E / Ctrl-E toggles the Edit mode overlay.",
        20, 180);
}
