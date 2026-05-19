#include "ofApp.h"
#include <cmath>

namespace {

/// ImGui window title + ###id (must match `registerWindow` id below for docking / layout).
constexpr const char* kProgressTestWindowImGuiTitle = "Test window###ofxkit.example_progressbars.test_loaders";

} // namespace

float ofApp::canvasProgressFraction() const
{
    switch (m_activeDemo) {

    case Demo::StepBased:
        return m_stepTotal > 0
            ? ofClamp(static_cast<float>(m_stepCurrent) / static_cast<float>(m_stepTotal), 0.f, 1.f)
            : 0.f;

    case Demo::Absolute:
        return ofClamp(m_absolutePos, 0.f, 1.f);

    case Demo::Indeterminate:
        return 0.5f + 0.5f * std::sin(ofGetElapsedTimef() * 3.1f);

    case Demo::None:
    default:
        return -1.f;
    }
}

void ofApp::setup()
{
    ofBackground(26, 28, 38);
    ofSetWindowTitle("ofxKit \u2014 Progress Window Demo");

    ofkitty::progress().registerWithRuntime();
    ofkitty::progress().setUseStatusBar(m_useStatusBar);
    ofkitty::progress().setAutoHideDelay(m_autoHideDelay);

    // Dedicated Runtime window: fake loader demos (also listed under View when edit mode is on).
    ofkitty::runtime().registerWindow({
        "Test window",
        "View",
        /*visible=*/    true,
        /*editModeOnly=*/ true,
        [this](bool& visible) {

            ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);

            if (!ImGui::Begin(kProgressTestWindowImGuiTitle, &visible)) {
                ImGui::End();
                return;
            }

            ImGui::TextDisabled("Trigger fake loaders \u2014 progress shows in the Runtime status bar or a floating window.");
            ImGui::Spacing();

            // ---- Options ----
            ImGui::SeparatorText("Options");

            if (ImGui::Checkbox("Use status bar", &m_useStatusBar)) {
                ofkitty::progress().setUseStatusBar(m_useStatusBar);
            }
            ImGui::SetItemTooltip("When ON, progress is drawn in the bottom status bar with the other OF indicators.\n"
                                  "When OFF, progress uses a small centered floating window instead.");

            ImGui::SetNextItemWidth(160.f);
            if (ImGui::SliderFloat("Auto-hide delay (s)", &m_autoHideDelay, 0.f, 8.f, "%.1f s")) {
                ofkitty::progress().setAutoHideDelay(m_autoHideDelay);
            }

            ImGui::SetNextItemWidth(160.f);
            ImGui::SliderFloat("Sim speed", &m_simulationSpeed, 0.1f, 5.f, "%.1f\xc3\x97");
            ImGui::SetItemTooltip("Multiplier applied to the simulated step rate.");

            // ---- Fake loaders ----
            ImGui::Spacing();
            ImGui::SeparatorText("Fake loaders");

            // Step-based demo
            bool stepRunning = (m_activeDemo == Demo::StepBased);
            if (stepRunning) ImGui::BeginDisabled();
            if (ImGui::Button("Step-based  (80 steps)", ImVec2(-1, 0))) {
                m_activeDemo  = Demo::StepBased;
                m_stepCurrent = 0;
                ofkitty::progress().begin("Exporting layers", m_stepTotal);
            }
            if (stepRunning) ImGui::EndDisabled();
            ImGui::SetItemTooltip("begin(title, N)  +  tick(label)  \u2014 typical batch export.");

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
            ImGui::SetItemTooltip("begin(title)  +  tick(label, 0..1)  \u2014 when total isn't known up front.");

            ImGui::Spacing();

            // Indeterminate demo
            bool indRunning = (m_activeDemo == Demo::Indeterminate);
            if (indRunning) ImGui::BeginDisabled();
            if (ImGui::Button("Indeterminate  (4 s)", ImVec2(-1, 0))) {
                m_activeDemo  = Demo::Indeterminate;
                m_absolutePos = 0.f;
                ofkitty::progress().begin("Connecting\xe2\x80\xa6");
                ofkitty::progress().tickIndeterminate("Please wait\xe2\x80\xa6");
            }
            if (indRunning) ImGui::EndDisabled();
            ImGui::SetItemTooltip("tickIndeterminate(label)  \u2014 animated marquee, no fraction known.");

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
        },
        "ofxkit.example_progressbars.test_loaders"
    });

    ofkitty::runtime().addDefaultLayoutLeftDock(kProgressTestWindowImGuiTitle);
    ofkitty::runtime().setEditMode(true);
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
            ofkitty::progress().tickIndeterminate("Please wait\xe2\x80\xa6");
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
    const float w = static_cast<float>(ofGetWidth());
    const float h = static_cast<float>(ofGetHeight());
    const float t = ofGetElapsedTimef();

    float pr = canvasProgressFraction();
    const bool hasProgress = (pr >= 0.f);
    // Subtle background pulse when idle; shift a bit more while a loader runs
    const float fade = hasProgress ? ofClamp(pr, 0.f, 1.f)
                                   : 0.35f + 0.08f * std::sin(t * 0.55f);

    // Soft full-window colour fade (no vignette, no second "ribbon" layer)
    ofMesh base;
    base.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
    const float drift = t * 18.f + fade * 55.f;
    auto corner = [&](float hueBase, float sat, float br) {
        ofColor c;
        c.setHsb(
            static_cast<unsigned char>(ofWrap(hueBase + drift, 0.f, 255.f)),
            static_cast<unsigned char>(ofClamp(sat, 0.f, 255.f)),
            static_cast<unsigned char>(ofClamp(br, 0.f, 255.f)),
            255);
        return c;
    };
    base.addVertex({0, 0, 0});
    base.addColor(corner(128.f + 8.f * std::sin(t * 0.22f), 75.f, 42.f + 14.f * fade));
    base.addVertex({w, 0, 0});
    base.addColor(corner(142.f + 6.f * std::sin(t * 0.19f), 72.f, 40.f + 12.f * fade));
    base.addVertex({w, h, 0});
    base.addColor(corner(158.f + 7.f * std::sin(t * 0.21f), 78.f, 36.f + 16.f * fade));
    base.addVertex({0, h, 0});
    base.addColor(corner(135.f + 9.f * std::sin(t * 0.24f), 70.f, 38.f + 13.f * fade));
    base.draw();

    const std::string help =
        "ofxKit \u2014 Progress window demo\n"
        "View > Test window \u2014 fake loader buttons (dock left on a fresh layout).\n"
        "Ctrl+E \u2014 toggle whole UI (windows + menu bar + status bar).\n"
        "Tab    \u2014 toggle edit-mode windows only (menu bar stays visible).\n"
        "With \"Use status bar\" on, progress shares the bottom status strip.";

    ofSetColor(228, 232, 245);
    ofPushMatrix();
    ofTranslate(28.f, 44.f);
    ofScale(2.f, 2.f);
    ofDrawBitmapString(help, 0.f, 0.f);
    ofPopMatrix();
    ofSetColor(255);
}
