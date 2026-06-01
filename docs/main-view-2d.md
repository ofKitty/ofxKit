# Main-Scene 2-D View (ofSpace pan/zoom)

An alternative to the FBO-based `addViewportWindow2D()` for apps that render their
content **directly in `ofApp::draw()`** — no framebuffer object, no ImGui panel
wrapper. The Runtime manages the passthru central-node geometry, input (pan / zoom /
fit), and an ImGui overlay window for interactive handles.

---

## When to use this vs `addViewportWindow2D`

| | `addViewportWindow2D` | `setMainView2D` |
|---|---|---|
| Rendering target | `ofFbo` inside an ImGui panel | Main OF window (`ofApp::draw()`) |
| Viewport framing | Dockable panel with title/menu bar | Transparent passthru central node |
| DPI / zoom sharpness | FBO scales with zoom | Native OF rendering, always sharp |
| Multiple views | Yes — one panel per call | No — one main view |
| Suitable for | Preview-in-panel tools | Full-canvas editors, plotters |

---

## Prerequisites

```cpp
// ofApp::setup()
ofkitty::runtime().setPassthruCentralNode(true);  // the default — do NOT call setPassthruCentralNode(false)
```

The passthru central node is the transparent "hole" in the dockspace through which
the OF scene shows.  The Runtime tracks its exact bounds (after side panels take
their space) and uses them as the canvas for pan/zoom geometry.

---

## Quick start

```cpp
// ofApp.h
ofkitty::Runtime::MainView2D* m_view = nullptr;

// ofApp::setup()
m_view = ofkitty::runtime().setMainView2D({210.f, 297.f}, "mm");
m_view->showRulers = true;
m_view->guides     = &m_guides;  // optional GuideSet* for draggable guides

m_view->menuBarDraw = [this] {
    if (ImGui::BeginMenu("Canvas")) {
        ImGui::MenuItem("Show Grid", nullptr, &m_showGrid);
        ImGui::EndMenu();
    }
};

m_view->overlayDraw = [this](ofkitty::Runtime::MainView2D& v) {
    drawHandles(v);
};

// ofApp::update()  — keep contentSize in sync (see below)
m_view->view2D.contentSize = myDoc.paperSizeMM();

// ofApp::draw()
void ofApp::draw() {
    if (!m_view) return;
    const auto& v = m_view->view2D;   // ox/oy/zoom_ already computed
    ofPushMatrix();
    ofTranslate(v.ox, v.oy);
    ofScale(v.zoom_);
    drawContent();   // draw in content units (mm), Y-DOWN, origin = content TL
    ofPopMatrix();
}
```

---

## Content coordinate system

Identical to the FBO Ortho2D path:

- **Y-DOWN**, origin `(0, 0)` = top-left of the content rectangle.
- One unit = one "content unit" (mm, px — whatever you pass as `contentSize`).
- `ox/oy` are the screen-pixel position of the content origin; `zoom_` is pixels per unit.

Draw as if the full document is at `(0, 0)`:

```cpp
ofDrawRectangle(0, 0, paperW, paperH);   // paper background
for (auto& path : doc.paths()) path.draw();
```

---

## Keeping contentSize current

`contentSize` drives `fitZoom` (the zoom level at which content fills the visible
area). Update it in `ofApp::update()`, **before** the draw phase — the Runtime's
`BEFORE_APP` hook reads it to compute geometry for both `draw()` and the overlay:

```cpp
// ofApp::update()
if (m_view)
    m_view->view2D.contentSize = myDoc.boundsInMM().size();
```

---

## Overlay handles (`overlayDraw` callback)

The callback runs **inside the ImGui frame** (after `ofApp::draw()`). `view2D.toScreen()` and
`toContent()` are valid. Interactive handles, selection rectangles, zone grids, etc.
go here.

### The desktop-global offset

When `ImGuiConfigFlags_ViewportsEnable` is active (the default on desktop), ImGui
`DrawList` vertices and `io.MousePos` are in **desktop-global coordinates** (origin =
top-left of the primary monitor). `view2D.toScreen()` returns **window-relative**
coordinates (origin = top-left of the OF window). They differ by
`v.imguiScreenOffset` (= `ImGui::GetMainViewport()->Pos`).

Always apply the offset when drawing to an `ImDrawList`:

```cpp
void MyApp::drawHandles(ofkitty::Runtime::MainView2D& v) {
    const glm::vec2 off = v.imguiScreenOffset;
    ImDrawList*     dl  = ImGui::GetWindowDrawList();
    const ImGuiIO&  io  = ImGui::GetIO();

    // Convert content → desktop-global for DrawList
    auto scrPt = [&](float cx, float cy) -> ImVec2 {
        auto p = v.view2D.toScreen(cx, cy);
        return { p.x + off.x, p.y + off.y };
    };

    // Convert desktop-global io.MousePos → content units
    auto cntPt = [&](float sx, float sy) {
        return v.view2D.toContent(sx - off.x, sy - off.y);
    };

    // Draw a handle circle at a content point
    dl->AddCircleFilled(scrPt(myPt.x, myPt.y), 5.f, IM_COL32(255, 200, 0, 220));

    // Hit-test a click in content space
    if (io.MouseClicked[0] && v.view2D.hovered) {
        auto c = cntPt(io.MousePos.x, io.MousePos.y);
        selectNearest(c.x, c.y);
    }
}
```

On single-window builds (Emscripten, mobile, `ViewportsEnable` off),
`imguiScreenOffset` is `{0, 0}` — the pattern above works without changes.

---

## Built-in input

| Gesture | Action |
|---|---|
| Scroll wheel | Zoom around cursor |
| Middle-mouse drag | Pan |
| Alt + LMB drag | Pan |
| Double-click LMB | Fit content to the passthru area |

Disable individual gestures via the config flags:

```cpp
m_view->panOnMiddle = false;  // disable middle-mouse pan
m_view->panOnAltLMB = false;  // disable Alt+LMB pan
```

---

## Coordinate converters (valid in `overlayDraw`)

```cpp
// Content → window-relative screen pixels
glm::vec2 v.view2D.toScreen(float cx, float cy);

// Window-relative screen pixels → content units
glm::vec2 v.view2D.toContent(float sx, float sy);

float     v.view2D.contentZoom();   // current effective zoom (fit × user)

// For DrawList: add imguiScreenOffset to toScreen() result.
// For mouse:   subtract imguiScreenOffset from io.MousePos before toContent().
```

---

## API reference

```cpp
// Register (call in setup). Replaces any previous view.
MainView2D* setMainView2D(glm::vec2 contentSize, std::string contentUnit = "mm");

// Access (returns nullptr if not set).
MainView2D* mainView2D();

// Remove and unregister.
void clearMainView2D();
```

### `MainView2D` fields

```cpp
struct MainView2D {
    View2DState view2D;          // pan/zoom state — update contentSize in update()
    std::string contentUnit;     // label shown on ruler ticks ("mm", "px", …)
    GuideSet*   guides;          // optional draggable guide lines
    bool        showRulers;      // ruler strips along top/left edges

    bool panOnMiddle;            // middle-mouse drag pans (default true)
    bool panOnAltLMB;            // Alt+LMB drag pans (default true)

    glm::vec2 imguiScreenOffset; // iv->Pos; add to toScreen() for DrawList calls

    std::function<void(MainView2D&)> overlayDraw;  // handles, grids, hit-testing
    std::function<void()>            menuBarDraw;  // injected into the View menu
};
```

---

## Implementation notes

- Canvas geometry (`canvasOrigin`, `canvasW/H`) is derived from the **passthru central node** bounds, not `ofGetWidth/Height`. This ensures the fit-zoom and pan centre account for the space taken by docked side panels, the menu bar, and the status bar.
- `canvasOrigin` is in window-relative coordinates — `ox/oy` in `draw()` and the overlay are consistent.
- Pan/zoom state (`view2D.pan`, `view2D.zoom`) survives across frames; `view2D.ox/oy/zoom_` are recomputed each `BEFORE_APP` draw event.
- The overlay is a full-screen transparent `NoInputs` ImGui window — it sits above the OF scene but below all docked panels, so handles are clipped by panels naturally.
