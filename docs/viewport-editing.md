# Viewport Editing

Interactive editing controls (bezier handles, pixel grids, gizmos) that live in the OF viewport — not inside a floating ImGui panel.

## The coordinate battle

Mouse input always arrives in screen pixels. Content lives in world coordinates. Without help, every interactive tool ends up writing the same camera-unproject boilerplate:

```cpp
// repeated everywhere — the "battle"
glm::vec3 world = cam_.screenToWorld(glm::vec3(ofGetMouseX(), ofGetMouseY(), 0));
bool hit = glm::length(glm::vec2(world.x, world.y) - handle) < 8;
```

`ofxImGui::Viewport` wraps this once. After calling `vp_.update(cam_)`, all interaction is expressed in world coordinates.

## The pattern

Three-phase draw loop:

```
1. cam_.begin() / cam_.end()       — scene content (OF, world coords)
2. gui_.begin() / gui_.end()       — vp_.update(), drag logic, ImGui panels
3. cam_.begin() / cam_.end()       — editing handles (OF, world coords,
                                     drawn AFTER gui_.end() → above panels)
```

The Viewport bridges phases 1 and 2: it reads the camera to convert mouse screen pixels into the same world coordinate space the scene uses.

## Viewport API

Declared in `addons/ofxImGui/src/ImViewport.h`, included via `ofxImGui.h`.

```cpp
ofxImGui::Viewport vp_;
```

### update

```cpp
vp_.update();                           // no camera — screen == world
vp_.update(cam_);                       // with camera
vp_.update(cam_, ofRectangle(x,y,w,h)); // restrict to a sub-region
```

Call once per frame inside the ImGui frame (between `gui_.begin()` and `gui_.end()`).

### Input state

| Method | Description |
|---|---|
| `isHovered()` | Mouse in viewport rect, ImGui not capturing |
| `isActive()` | Left mouse held, press started in this viewport |
| `isClicked()` | Left mouse just pressed this frame |
| `isReleased()` | Left mouse just released this frame |

### Mouse position

| Method | Returns |
|---|---|
| `getScreenMouse()` | Screen pixels |
| `getWorldMouse()` | World coords via camera (or screen coords if no camera) |
| `getWorldMouseDelta()` | World-space delta since last frame |

### Hit testing

```cpp
vp_.isNear(worldPoint, screenRadius = 8.f)
```

Projects `worldPoint` to screen and checks if the mouse is within `screenRadius` pixels. **Zoom-invariant** — handles remain pickable at a consistent visual size regardless of camera zoom level. Accepts `glm::vec2` or `glm::vec3`.

### Coordinate conversion

```cpp
glm::vec2 screen = vp_.worldToScreen(worldPt);  // world → screen px
glm::vec2 world  = vp_.screenToWorld(screenPt); // screen px → world
```

### Drag tracking

```cpp
vp_.beginDrag(handleId);  // associate an index with the current drag
vp_.endDrag();            // clear drag state
vp_.isDragging()          // true while a drag is in progress
vp_.getDragId()           // index passed to beginDrag (-1 if none)
```

## With a camera — bezier editor

```cpp
// ofApp.h
ofxImGui::Gui      gui_;
ofxImGui::Viewport vp_;
ofEasyCam          cam_;
glm::vec2          pts_[4];

// ofApp::draw()
void ofApp::draw() {
    ofBackground(22, 22, 30);

    // 1. Scene
    cam_.begin();
    drawGrid();
    cam_.end();

    // 2. ImGui frame
    gui_.begin();
    vp_.update(cam_);

    if (vp_.isActive()) {
        if (!vp_.isDragging())
            for (int i = 0; i < 4; ++i)
                if (vp_.isNear(pts_[i])) { vp_.beginDrag(i); break; }
        if (vp_.isDragging())
            pts_[vp_.getDragId()] = vp_.getWorldMouse();
    } else {
        vp_.endDrag();
    }

    ImGui::Begin("Controls");
    ImGui::ColorEdit3("Colour", col_);
    ImGui::End();

    gui_.end();  // renders ImGui panels

    // 3. Handles above panels
    cam_.begin();
    ofSetColor(255, 200, 0);  ofNoFill();
    ofDrawBezier(pts_[0].x, pts_[0].y, pts_[1].x, pts_[1].y,
                 pts_[2].x, pts_[2].y, pts_[3].x, pts_[3].y);

    for (int i = 0; i < 4; ++i) {
        ofSetColor(vp_.isNear(pts_[i]) ? ofColor::white : ofColor(100,180,255));
        ofFill();
        ofDrawCircle(pts_[i].x, pts_[i].y, 8);
    }
    cam_.end();
}
```

Key points:
- `pts_` are in **world coords** throughout — no conversion needed.
- `isNear(pts_[i])` uses a screen-space radius — handles stay pickable when zoomed.
- Handles are drawn after `gui_.end()` so they appear above the control panel.

## Without a camera — pixel editor

When no camera is passed, `getWorldMouse()` returns screen pixels directly. Useful for editors where screen space and content space are the same (pixel editors, fixed-layout canvases).

```cpp
void ofApp::draw() {
    // Canvas geometry fixed in screen space
    ofRectangle canvasRect(200, 12, cellSz_ * kW, cellSz_ * kH);

    // 1. Draw grid with plain OF
    drawGrid(canvasRect);

    // 2. ImGui frame
    gui_.begin();
    vp_.update(canvasRect);  // no camera — restrict to canvas region

    if (vp_.isActive()) {
        glm::vec2 mouse = vp_.getWorldMouse();  // screen pixels
        int cx = (mouse.x - canvasRect.x) / cellSz_;
        int cy = (mouse.y - canvasRect.y) / cellSz_;
        paintCell(cx, cy, ImGui::GetIO().MouseDown[1] /* erase */);
    }

    ImGui::Begin("Brush");
    ImGui::ColorEdit4("Colour", brushCol_);
    ImGui::End();
    gui_.end();
}
```

## Rendering order — why handles go after `gui_.end()`

In autodraw mode, `gui_.end()` flushes the ImGui render pass. Any OF drawing after that call appears on top of all ImGui windows.

```
gui_.begin()      ← start ImGui frame
  vp_.update()    ← capture input
  ImGui::Begin()  ← control panels
  ImGui::End()
gui_.end()        ← ImGui rendered here (autodraw)

cam_.begin()      ← OF drawing here appears ABOVE ImGui
  drawHandles()
cam_.end()
```

To draw handles *below* panels (e.g. underneath a floating inspector), draw them before `gui_.begin()` instead.

## vs. Canvas

`ofxImGui::Canvas` (in `ImCanvas.h`) is for widgets embedded *inside* an ImGui window panel. `Viewport` is for interactive surfaces in the main OF window. They are complementary and designed to be used together in the same application.

| | `Canvas` | `Viewport` |
|---|---|---|
| Lives inside | ImGui `Begin/End` window | OF main window |
| Coordinates | Canvas-local (0,0 = widget TL) | World coords via camera |
| Input | `InvisibleButton` | OF + `WantCaptureMouse` |
| Drawing API | `ofxImGui::DrawList` | plain OF (`ofDrawCircle` etc.) |
| Use case | Compact embedded widget, minimap, preview | Full-viewport interactive editing |

Both are included via `#include "ofxImGui.h"`.

## Combining both in one application

A common pattern is to use `Viewport` as the **primary editing surface** and `Canvas` as a **secondary panel widget** showing the same data in a different context. The examples demonstrate this concretely:

**`example-beziereditor`**
- Main window: drag bezier handles with `Viewport` + `ofEasyCam` (pan/zoom, world coords)
- Sidebar panel: `Canvas` preview of the same `pts_[]`, mapped to local bbox coords — useful as a compact easing-curve view that isn't affected by the camera

**`example-pixeleditor`**
- Main window: paint pixels with `Viewport` (no camera, screen == world)
- Sidebar panel: `Canvas` magnifier showing a 7×7 zoom of the area around the cursor

```cpp
// Pattern: Viewport edits, Canvas previews — both share the same data
class ofApp : public ofBaseApp {
    ofxImGui::Viewport vp_;      // main window — world-space input
    ofxImGui::Canvas   canvas_;  // ImGui panel — local-coord rendering
    Data               data_;    // shared, edited via vp_, displayed via canvas_
};
```

The two helpers never conflict: `Viewport` runs during the ImGui frame but reads from the OF event system, while `Canvas` is a pure ImGui draw widget that only calls into `ImDrawList`.
