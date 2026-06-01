#include "ofMain.h"
#include "ofApp.h"

// ─────────────────────────────────────────────────────────────────────────────
// example-abletonLink — ofxVCVRack synced to Ableton Link via ofxKit
//
// • "Link Sync" panel (from ofxAbletonLinkKit) — BPM, peers, beat/phase bar
// • Gate and arp triggered on beat boundaries from Link transport
// • UDP bus streams param/LED state; receives remote commands on :9001
// • CTRL+E / TAB to toggle the ofxKit edit-mode overlay
// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    ofGLFWWindowSettings settings;
    settings.setSize(1440, 900);
    settings.windowMode = OF_WINDOW;
    settings.resizable  = true;
    settings.title      = "ofxVCVRack — Ableton Link";

    auto window = ofCreateWindow(settings);
    auto app    = std::make_shared<ofApp>();

    ofkitty::runtime().setAppName("ofxVCVRack Link");
    ofkitty::Runtime::attach(window, app, app->rackRegistry());

    ofRunApp(window, std::move(app));
    ofRunMainLoop();
}
