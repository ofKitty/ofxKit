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

---

## Ortho2D mode — 2D canvas with pan / zoom / rulers

`addViewportWindow2D()` creates a viewport that renders a flat 2-D canvas inside an FBO and exposes
pixel-accurate coordinate converters. Use it for any app that has a "paper / canvas" concept — plotters,
laser cutters, sprite editors, diagram tools, etc.

### Quick start

```cpp
// ofApp::setup()
m_vp = runtime().addViewportWindow2D(
    "Preview###my.preview",   // ImGui title (###id is the stable docking id)
    { 210.f, 297.f },         // content size in "mm"
    "mm",                     // unit label shown on ruler ticks
    /*editModeOnly=*/false);  // remain visible outside edit mode

m_vp->showRulers = true;
m_vp->guides     = &m_guides;   // optional GuideSet* for draggable guide lines

// App-specific menu items inserted before the built-in "View" menu.
m_vp->menuBarDraw = [this] {
    if (ImGui::BeginMenu("Canvas")) {
        ImGui::MenuItem("Show Grid", nullptr, &m_showGrid);
        ImGui::EndMenu();
    }
};

// Suppresses the canvas when content is not ready.
// Return true = hide canvas (show progress bar, error message, etc.)
m_vp->headerDraw = [this]() -> bool {
    if (m_loading) {
        ImGui::ProgressBar(m_progress);
        return true;
    }
    m_vp->contentSize = m_doc.paperSizeMM();  // keep in sync if dynamic
    return false;
};

// OF drawing callback — called inside fbo.begin()...fbo.end().
// Coordinate system: Y-DOWN, origin at content top-left, 1 unit = 1 content unit.
// Pan + zoom transforms are already applied; draw as if the full document is at (0,0).
m_vp->renderer2D = [this] {
    ofSetColor(255);
    ofDrawRectangle(0, 0, m_doc.paperW(), m_doc.paperH()); // paper background
    for (auto& path : m_doc.paths()) path.draw();          // draw in mm coords
};

// ImGui overlay callback — called after the FBO image is displayed.
// toScreen() / toContent() are valid here; use them for hit-testing and ImDrawList.
m_vp->overlayDraw = [this](ofkitty::Runtime::ViewportInstance& vp) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Draw a cross-hair at the origin.
    dl->AddLine(vp.toScreen(-5, 0), vp.toScreen(5, 0), IM_COL32_WHITE, 1.f);
    dl->AddLine(vp.toScreen(0, -5), vp.toScreen(0, 5), IM_COL32_WHITE, 1.f);

    // Hit-test a click.
    if (vp.isCanvasHovered() && ImGui::IsMouseClicked(0)) {
        auto [cx, cy] = vp.toContent(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
        ofLog() << "clicked at (" << cx << ", " << cy << ") mm";
    }
};
```

### Coordinate system

All content is rendered in a **Y-DOWN**, left-handed coordinate system.
The origin `(0, 0)` is the **top-left corner** of the content rectangle.
One unit = one "content unit" (e.g. millimetres, pixels — whatever you pass as `contentSize`).

| Direction | Screen | Content |
|---|---|---|
| X | right → | right → |
| Y | down ↓ | down ↓ |

### Coordinate converters

These are updated every frame and valid inside `overlayDraw`:

```cpp
ImVec2    vp.toScreen (float cx, float cy);  // content → screen pixels
glm::vec2 vp.toContent(float sx, float sy);  // screen pixels → content units
float     vp.contentZoom();                  // current effective zoom (fit * user)
ImVec2    vp.canvasOriginPx();               // top-left of canvas area (screen px)
float     vp.canvasW();
float     vp.canvasH();
bool      vp.isCanvasHovered();              // true when mouse is over canvas
```

### Built-in interaction

| Input | Action |
|---|---|
| Scroll wheel | Zoom around cursor |
| Middle-drag or Alt+LMB | Pan |
| Double-click | Fit to window (reset zoom & pan) |

The **View** menu in the panel's menu bar exposes "Rulers" and "Fit to Window" items.
App-specific menus are injected via `menuBarDraw` and appear **before** the built-in View menu.

### Zoom controls

If you want the ±% zoom buttons in the menu bar, add them in `menuBarDraw`:

```cpp
m_vp->menuBarDraw = [this] {
    // ...
    ImGui::Separator();
    if (ImGui::SmallButton(" - "))
        m_vp->zoom2D = std::max(0.1f, m_vp->zoom2D / 1.25f);
    ImGui::SameLine(0, 2);
    char buf[16]; snprintf(buf, 16, " %3.0f%% ", m_vp->zoom2D * 100.f);
    if (ImGui::SmallButton(buf)) { m_vp->zoom2D = 1.f; m_vp->pan2D = {}; }
    ImGui::SameLine(0, 2);
    if (ImGui::SmallButton(" + "))
        m_vp->zoom2D = std::min(50.f, m_vp->zoom2D * 1.25f);
};
```

### Rulers and guides

Set `vp->showRulers = true` and optionally `vp->guides = &m_guides` (a `GuideSet` you own).
Ruler strips are drawn along the top and left edges; dragging from them creates guide lines.

### FBO details

- Allocated lazily; reallocated on resize.
- Cleared to `(18, 18, 24)` before `renderer2D` is called.
- `GL_RGBA` / `GL_TEXTURE_2D`; ARB rect textures disabled during allocation.
- UV-flipped when blitted into ImGui (OpenGL FBOs are stored bottom-up).

### Difference from Orbit3D mode

| Feature | Orbit3D | Ortho2D |
|---|---|---|
| Camera type | Perspective orbit camera | Orthographic, top-down, fixed |
| Pan / zoom | Mouse-driven 3-D orbit | 2-D scroll + drag |
| Coordinate helpers | None | `toScreen`, `toContent`, `isCanvasHovered` |
| Callback split | Single `setViewportRenderer` | `renderer2D` + `overlayDraw` + `headerDraw` + `menuBarDraw` |
| Rulers / guides | Optional strip on main window | Per-panel via `showRulers` + `GuideSet*` |
| Use case | 3-D scene inspection | Flat documents, plotters, sprite editors |
