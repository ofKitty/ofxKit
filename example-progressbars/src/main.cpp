#include "ofMain.h"
#include "ofApp.h"

int main()
{
    ofGLWindowSettings settings;
    settings.setSize(1280, 720);
    settings.setGLVersion(3, 2);
    auto window = ofCreateWindow(settings);

    auto app = std::make_shared<ofApp>();
    ofkitty::Runtime::attach(window, app);
    ofRunApp(window, std::move(app));
    ofRunMainLoop();
}
