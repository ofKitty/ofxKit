#pragma once

#include "ofMain.h"
#include "ofxKit.h"

// ─────────────────────────────────────────────────────────────────────────────
// ofKitty — ofxKit showcase
//
// This is a completely ordinary ofBaseApp. It owns the ECS registry and draws
// the 3D scene. The Runtime (attached in main.cpp) adds the Edit-mode overlay
// on top via OF's event system — no ImGui code lives here.
//
// Press CTRL+E / CMD+E (or TAB) to toggle the Edit-mode overlay.
// ─────────────────────────────────────────────────────────────────────────────

class ofApp : public ofBaseApp {
public:
    void setup()  override;
    void update() override;
    void draw()   override;

    // Exposed so main.cpp can call Runtime::attach(window, app, app->registry())
    entt::registry& registry() { return m_registry; }

private:
    entt::registry     m_registry;
    ofEasyCam          m_cam;
    float              m_time    = 0.f;
    bool               m_showGrid = true;
    bool               m_showAxes = true;

    void createDemoScene();
    void addMeshEntity(ecs::eMeshPrimitiveType type,
                       const std::string&      name,
                       glm::vec3               pos,
                       ofColor                 col);

    void drawGrid();
    void drawAxes();
};
