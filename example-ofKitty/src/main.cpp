#include "ofMain.h"
#include "ofApp.h"

// ─────────────────────────────────────────────────────────────────────────────
// ofKitty — ofxKit showcase
//
// Attach the Runtime to the window + app, then hand off to ofRunApp().
// That's all main.cpp needs to do. The Runtime injects the Edit-mode overlay
// via OF's event system at OF_EVENT_ORDER_AFTER_APP.
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    ofGLFWWindowSettings settings;
    settings.setSize(1280, 720);
    settings.windowMode = OF_WINDOW;
    settings.resizable  = true;
    settings.title      = "ofKitty";
    auto window = ofCreateWindow(settings);

    auto app = std::make_shared<ofApp>();

    // Tell the Runtime which app name to show in the menu bar.
    ofkitty::runtime().setAppName("ofKitty");

    // Pass the app's own registry so the Runtime inspector sees our entities.
    ofkitty::Runtime::attach(window, app, app->registry());

    ofRunApp(window, std::move(app));
    ofRunMainLoop();
}
