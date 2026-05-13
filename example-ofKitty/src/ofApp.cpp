#include "ofApp.h"

using namespace ofkitty;

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────

void ofApp::setup() {
    ofBackground(18, 18, 24);
    ofSetFrameRate(60);
    ofSetVerticalSync(true);

    m_cam.setDistance(550.f);
    m_cam.setNearClip(1.f);
    m_cam.setFarClip(10000.f);

    // Register the main camera so the Runtime can draw a transform gizmo
    // over selected entities in the main OF scene.
    runtime().setSceneCamera(&m_cam);

    // TAB as an alias for the built-in CTRL/CMD+E toggle.
    runtime().keys().bind(OF_KEY_TAB, 0, "Toggle Edit mode (TAB)",
                          [] { runtime().toggleEditMode(); });

    // "Scene" top-level menu group injected between the app menu and View.
    runtime().addMenuBarGroup("Scene", [this] {
        if (ImGui::BeginMenu("Add Entity")) {
            auto addAt = [&](ecs::eMeshPrimitiveType t, const char* name, ofColor col) {
                if (ImGui::MenuItem(name)) {
                    glm::vec3 p = { ofRandom(-250.f, 250.f),
                                    ofRandom(-120.f, 120.f),
                                    ofRandom(-250.f, 250.f) };
                    addMeshEntity(t, name, p, col);
                }
            };
            addAt(ecs::MESH_BOX,       "Box",       ofColor( 80, 160, 240));
            addAt(ecs::MESH_SPHERE,    "Sphere",    ofColor(240, 120,  60));
            addAt(ecs::MESH_CONE,      "Cone",      ofColor(140, 220, 100));
            addAt(ecs::MESH_CYLINDER,  "Cylinder",  ofColor(220, 100, 180));
            addAt(ecs::MESH_ICOSPHERE, "Icosphere", ofColor(255, 220,  60));
            addAt(ecs::MESH_PLANE,     "Plane",     ofColor(160, 160, 180));
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Show Grid",  nullptr, m_showGrid)) m_showGrid = !m_showGrid;
        if (ImGui::MenuItem("Show Axes",  nullptr, m_showAxes)) m_showAxes = !m_showAxes;
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Camera")) {
            m_cam.reset();
            m_cam.setDistance(550.f);
        }
        if (ImGui::MenuItem("Reset Scene")) {
            m_registry.clear();
            runtime().select(entt::null);
            createDemoScene();
        }
    });

    createDemoScene();

    // Register the scene renderer shared by all Viewport panels.
    // This callback must NOT set up a camera — each panel's Runtime-owned
    // ofCamera wraps the call, so panels can be orbited independently.
    runtime().setViewportRenderer([this] {
        if (m_showGrid) drawGrid();
        if (m_showAxes) drawAxes();
        for (auto [e, node, mesh, render] :
             m_registry.view<ecs::node_component,
                             ecs::mesh_component,
                             ecs::render_component>().each()) {
            if (!render.visible) continue;
            ofPushMatrix();
            ofMultMatrix(node.node.getGlobalTransformMatrix());
            ofSetColor(mesh.color);
            if (mesh.drawFaces)     mesh.m_mesh.draw();
            if (mesh.drawWireframe) mesh.m_mesh.drawWireframe();
            ofPopMatrix();
        }
    });

    // Add a default Scene View panel (auto-named "Scene View").
    // Open more via View > New Scene View, or call addViewportWindow() again.
    runtime().addViewportWindow();
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene helpers
// ─────────────────────────────────────────────────────────────────────────────

void ofApp::addMeshEntity(ecs::eMeshPrimitiveType type,
                           const std::string&      name,
                           glm::vec3               pos,
                           ofColor                 col) {
    auto e    = m_registry.create();
    auto& nd  = m_registry.emplace<ecs::node_component>(e, name);
    nd.node.setPosition(pos);

    auto& mesh         = m_registry.emplace<ecs::mesh_component>(e);
    mesh.primitiveType = type;
    mesh.color         = col;
    switch (type) {
        case ecs::MESH_SPHERE:   mesh.radius = 55.f;                            break;
        case ecs::MESH_CONE:     mesh.radius = 45.f; mesh.height = 130.f;       break;
        case ecs::MESH_CYLINDER: mesh.radius = 40.f; mesh.height = 120.f;       break;
        case ecs::MESH_PLANE:    mesh.width  = 200.f; mesh.height = 200.f;      break;
        case ecs::MESH_ICOSPHERE:mesh.radius = 60.f;                            break;
        default:                 mesh.width = mesh.height = mesh.depth = 100.f; break;
    }
    mesh.rebuild();
    m_registry.emplace<ecs::render_component>(e).visible = true;

    // Auto-select first entity so the Properties panel is populated on launch.
    if (runtime().selected() == entt::null) runtime().select(e);
}

void ofApp::createDemoScene() {
    addMeshEntity(ecs::MESH_BOX,       "Box",       {-200.f,   0.f,  0.f}, ofColor( 80, 160, 240));
    addMeshEntity(ecs::MESH_SPHERE,    "Sphere",    {   0.f,   0.f,  0.f}, ofColor(240, 120,  60));
    addMeshEntity(ecs::MESH_CONE,      "Cone",      { 200.f,   0.f,  0.f}, ofColor(140, 220, 100));
    addMeshEntity(ecs::MESH_CYLINDER,  "Cylinder",  {-100.f, 180.f,  0.f}, ofColor(220, 100, 180));
    addMeshEntity(ecs::MESH_ICOSPHERE, "Gem",       { 100.f, 180.f,  0.f}, ofColor(255, 220,  60));

    // Camera stand-in entity (inspectable but not rendered as a mesh)
    auto cam_e = m_registry.create();
    auto& n    = m_registry.emplace<ecs::node_component>(cam_e, std::string("Camera"));
    n.node.setPosition({0.f, -200.f, 400.f});
    m_registry.emplace<ecs::tag_component>(cam_e, std::string("camera"));
    m_registry.emplace<ecs::camera_component>(cam_e);
}

// ─────────────────────────────────────────────────────────────────────────────
// Update
// ─────────────────────────────────────────────────────────────────────────────

void ofApp::update() {
    m_time += ofGetLastFrameTime();

    int idx = 0;
    for (auto [e, node, mesh] :
         m_registry.view<ecs::node_component, ecs::mesh_component>().each()) {
        float s = 0.30f + idx * 0.06f;
        node.node.rotateDeg( s,        glm::vec3(0, 1, 0));
        node.node.rotateDeg( s * 0.5f, glm::vec3(1, 0, 0));
        ++idx;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw  (Runtime overlay fires after this at OF_EVENT_ORDER_AFTER_APP)
// ─────────────────────────────────────────────────────────────────────────────

void ofApp::draw() {
    m_cam.begin();
    ofEnableDepthTest();

    if (m_showGrid) drawGrid();
    if (m_showAxes) drawAxes();

    for (auto [e, node, mesh, render] :
         m_registry.view<ecs::node_component,
                         ecs::mesh_component,
                         ecs::render_component>().each()) {
        if (!render.visible) continue;
        ofPushMatrix();
        ofMultMatrix(node.node.getGlobalTransformMatrix());
        ofSetColor(mesh.color);
        if (mesh.drawFaces)     mesh.m_mesh.draw();
        if (mesh.drawWireframe) mesh.m_mesh.drawWireframe();
        ofPopMatrix();
    }

    ofDisableDepthTest();
    m_cam.end();

    // When the Edit-mode overlay is hidden, show a subtle hint so the user
    // knows the editor exists. The Runtime draws its own UI on top of this.
    if (!runtime().isEditMode()) {
        ofSetColor(255, 255, 255, 50);
        ofDrawBitmapString("CTRL+E / TAB   edit mode", 14, ofGetHeight() - 14);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene decoration helpers
// ─────────────────────────────────────────────────────────────────────────────

void ofApp::drawGrid() {
    constexpr int   lines = 24;
    constexpr float step  = 50.f;
    constexpr float half  = lines * step * 0.5f;
    ofSetColor(55, 55, 68, 110);
    ofSetLineWidth(1.f);
    for (int i = 0; i <= lines; ++i) {
        float t = -half + i * step;
        ofDrawLine( t, 0, -half,  t, 0,  half);
        ofDrawLine(-half, 0, t,   half, 0, t);
    }
}

void ofApp::drawAxes() {
    ofSetLineWidth(2.f);
    ofSetColor(220,  60,  60); ofDrawLine({0,0,0}, {120,  0,  0});
    ofSetColor( 80, 200,  80); ofDrawLine({0,0,0}, {  0,120,  0});
    ofSetColor( 60, 120, 240); ofDrawLine({0,0,0}, {  0,  0,120});
    ofSetLineWidth(1.f);
}
