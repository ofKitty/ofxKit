#include "ofMain.h"
#include "ofApp.h"

// ─────────────────────────────────────────────────────────────────────────────
// example-udp — VCVRack modular synth inside ofxKit
//
// The Runtime injects the Edit-mode overlay (CTRL+E / TAB).
// The rack modules appear as free-floating dockable ImGui windows.
// A UDP bus streams param/LED state and accepts remote commands on :9001.
// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    ofGLFWWindowSettings settings;
    settings.setSize(1440, 900);
    settings.windowMode = OF_WINDOW;
    settings.resizable  = true;
    settings.title      = "ofxVCVRack — UDP";

    auto window = ofCreateWindow(settings);
    auto app    = std::make_shared<ofApp>();

    ofkitty::runtime().setAppName("ofxVCVRack UDP");

    // Pass the rack's own ECS registry so the Runtime Properties panel
    // can inspect module/param/port components as ECS entities.
    ofkitty::Runtime::attach(window, app, app->rackRegistry());

    ofRunApp(window, std::move(app));
    ofRunMainLoop();
}
