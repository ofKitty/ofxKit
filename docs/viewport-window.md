# Viewport Windows

Optional secondary view panels that render the scene into an FBO and display it as an interactive image inside dockable ImGui windows. Each panel has its own independent camera — useful for checking the scene from a different angle without moving the main camera, or for a classic 3-D editor quad-view layout.

Enable panels from **View > \<panel name\>** in the menu bar.

---

## Quick start — single panel

```cpp
// ofApp::setup()
runtime().setViewportRenderer([this] {
    if (m_showGrid) drawGrid();
    for (auto [e, nd, mesh, render] :
         m_registry.view<ecs::node_component,
                         ecs::mesh_component,
                         ecs::render_component>().each()) {
        if (!render.visible) continue;
        ofPushMatrix();
        ofMultMatrix(nd.node.getGlobalTransformMatrix());
        ofSetColor(mesh.color);
        mesh.m_mesh.draw();
        ofPopMatrix();
    }
});

runtime().addViewportWindow("Perspective");   // shows up in View > Perspective
```

---

## Quad-view 3-D editor

```cpp
// ofApp::setup()
runtime().setViewportRenderer([this] { drawSceneObjects(); });

runtime().addViewportWindow("Perspective");   // az=30, el=20 (default)

auto* top   = runtime().addViewportWindow("Top");
top->elevation = 89.9f;  top->azimuth = 0.f;

auto* front = runtime().addViewportWindow("Front");
front->azimuth = 0.f;    front->elevation = 0.f;

auto* right = runtime().addViewportWindow("Right");
right->azimuth = 90.f;   right->elevation = 0.f;
```

Dock all four panels into a 2×2 grid for a classic layout. Each panel renders independently at its own angle.

---

## API

Declared in `addons/ofxKit/src/Runtime.h`, accessible via `runtime()`.

```cpp
using ViewportRenderer = std::function<void()>;

// Register the scene-draw callback (shared across all viewport panels).
void setViewportRenderer(ViewportRenderer fn);
void clearViewportRenderer();

// Create a named panel. Returns a pointer for setting the initial camera.
// The title must be unique — it becomes the ImGui window title.
ViewportInstance* addViewportWindow(std::string title = "Viewport");

// Remove a panel by title. No-op if not found.
void removeViewportWindow(const std::string& title);
```

### `ViewportInstance` fields

After calling `addViewportWindow()` you can adjust the pointer's fields before the first frame:

```cpp
struct ViewportInstance {
    std::string title;         // ImGui window title (set by addViewportWindow)
    float       azimuth;       // horizontal angle around Y axis, degrees (default 30)
    float       elevation;     // vertical angle, degrees (default 20)
    float       distance;      // camera distance from target (default 500)
    glm::vec3   target;        // look-at point in world space (default {0,0,0})
    // (fbo, cam, lastPanelSize are managed internally)
};
```

---

## Renderer callback contract

The callback draws scene content only — no camera setup:

| Allowed | Not allowed |
|---|---|
| `ofPushMatrix` / `ofPopMatrix` / `ofMultMatrix` | `someCamera.begin()` / `.end()` |
| `ofSetColor`, `mesh.draw()`, grid helpers | `ofClear()` (Runtime clears before each call) |
| Iterating the ECS registry | Setting the OpenGL viewport directly |

The FBO is cleared to `(18, 18, 24)` and depth testing is enabled automatically, then restored after the callback.

---

## Camera controls

Each panel has its own camera driven by ImGui mouse input — never conflicts with the main `ofEasyCam`.

| Input | Action |
|---|---|
| Left drag | Orbit (azimuth + elevation) |
| Middle drag | Pan the look-at target |
| Scroll wheel | Zoom (proportional to current distance) |

**Preset buttons** in each panel's header snap the camera to standard orientations (Persp, Front, Back, Top, Bottom, Right, Left).

A **Dist** drag field sets the camera distance directly.

---

## FBO allocation

Each panel allocates its own `ofFbo`, lazily on the first render, and reallocates whenever the panel is resized. Settings:

- `GL_RGBA` color format, `GL_TEXTURE_2D` target (ARB rect textures are disabled for the allocation via `ofDisableArbTex()` / `ofEnableArbTex()` — ImGui only supports `GL_TEXTURE_2D`)
- Near clip: `1.0` / Far clip: `10000.0`

Texture displayed with vertical UV flip (`{0,1}` → `{1,0}`) for OpenGL's bottom-up FBO convention.

---

## Implementation notes

- Panels are registered as `RuntimeWindow` entries with `editModeOnly = true` — they only render while edit mode is active (when the dockspace is present). Disabling edit mode hides them cleanly without leaving floating panels over the OF scene.
- Camera state (azimuth, elevation, distance, target) persists for the Runtime's lifetime.
- `ViewportInstance` is heap-allocated via `unique_ptr` inside `m_viewportInstances`. The raw pointer returned by `addViewportWindow()` is stable even if more panels are added later.
- `removeViewportWindow()` ejects both the `RuntimeWindow` entry and the `ViewportInstance`, releasing the FBO.
- The shared renderer callback is called N times per frame (once per visible panel) — keep it lightweight.
