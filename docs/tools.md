# ofxKit — Built-in Tools

ofxKit ships four integrated tools that make the editor overlay production-ready
for creative-coding workflows.

| Tool           | Addon back-end            | Open via         |
|----------------|---------------------------|------------------|
| File Dialog    | `ofxImGuiFileDialog`      | API call         |
| Gizmo          | `ofxImGuizmo`             | Auto in Viewport windows; opt-in for main scene |
| Code Editor    | `ofxImGuiTextEdit`        | View → Code Editor |
| Path Editor    | `ofxImGuiVectorEditor`    | View → Path Editor |

---

## File Dialog

A cross-platform, Dear ImGui-native file browser (no system dependencies, no
`zenity`, no OS sheet).

### Open a file

```cpp
runtime().openFileDialog(
    "open_img",               // unique key
    "Open Image",             // dialog title
    ".png,.jpg,.gif,.tga",    // filter string
    [this](const std::string& path) {
        image.load(path);
    });
```

### Save a file

```cpp
runtime().saveFileDialog(
    "save_shader",
    "Save Shader",
    ".glsl,.vert,.frag",
    "untitled.glsl",          // default file name
    [this](const std::string& path) {
        ofSaveTextFile(path, shaderSource);
    });
```

Dialogs are processed inside the ImGui block every frame — just call the API
and the dialog appears automatically. Multiple dialogs can be queued with
distinct keys; they are shown one at a time (modal).

---

## Transform Gizmo (ofxImGuizmo)

A 3-D translate/rotate/scale manipulator drawn over the selected entity's node.

### In Viewport windows

Gizmos are drawn automatically inside every Viewport window when an entity with
a `node_component` is selected. No extra code is needed.

### In the main OF scene

Supply your scene camera once in `setup()`, then call `captureSceneView()` once per frame inside your camera's `begin/end` block:

```cpp
void ofApp::setup()
{
    ofkitty::Runtime::attach(window, shared_from_this());
    runtime().setSceneCamera(&cam);
}

void ofApp::draw()
{
    cam.begin();
    runtime().captureSceneView();  // ← captures exact GL matrices for gizmo
    drawScene();
    cam.end();
}
```

`captureSceneView()` reads `GL_MODELVIEW_MATRIX` and `GL_PROJECTION_MATRIX` directly from OpenGL. This is necessary because `ofEasyCam` applies extra transforms inside `begin()` (handedness correction, orientation matrix) that are not accessible via `getGlobalTransformMatrix()`. Without it the gizmo compiles from the camera object and may be offset.

The gizmo is then rendered on the ImGui foreground draw list, clipped to the work area (below menu bar, above status bar).

### Menu and keyboard shortcuts

In Edit mode, use **Edit →** (Translate, Rotate, Scale, Universal, World / Local) or:

| Key | Action                          |
|-----|---------------------------------|
| W   | Translate                       |
| E   | Rotate                          |
| R   | Scale                           |
| X   | Toggle World / Local space      |

The optional **Toolbar** built-in shows the same operations as icon buttons.

### API

```cpp
runtime().setGizmoOperation(Runtime::GizmoOperation::Translate);
runtime().setGizmoOperation(Runtime::GizmoOperation::Rotate);
runtime().setGizmoOperation(Runtime::GizmoOperation::Scale);
runtime().setGizmoOperation(Runtime::GizmoOperation::Universal);

runtime().setGizmoMode(Runtime::GizmoMode::World);
runtime().setGizmoMode(Runtime::GizmoMode::Local);
```

---

## Code Editor

A syntax-highlighted text/code editor window backed by
[ImGuiColorTextEdit](https://github.com/santaclose/ImGuiColorTextEdit).

Open it from **View → Code Editor** or toggle visibility via the window
registry.

### Features

- Syntax highlighting for GLSL, C++, C, C#, Python, Lua, JSON, SQL, HLSL and
  more.
- Line numbers, multi-cursor, undo/redo.
- Read-only toggle.
- **Open** / **Save** / **Save As** toolbar buttons (backed by the file dialog).
- Auto-detects language from file extension on Open.

### Loading content from your app

```cpp
// Seed the editor with a shader source string
runtime().codeEditorSetText(myShader.getSource());
runtime().codeEditorSetLanguage(TextEditor::LanguageDefinitionId::Glsl);

// Read back edited source
std::string src = runtime().codeEditorGetText();
```

---

## Path Editor (ofxImGuiVectorEditor)

An immediate-mode bezier path editor for drawing and editing `ofPath` curves.

Open it from **View → Path Editor**.

### Tools

| Tool   | Description                                |
|--------|--------------------------------------------|
| Pen    | Click to add anchors; drag to set handles  |
| Select | Click / drag anchors and Bézier handles    |

### Entity sync

When an entity with an `ecs::path_component` is selected, its `ofPath` is
loaded into the editor automatically. After editing, click **Apply to Entity**
to write the modified path back to the component.

The conversion bridge maps:
- `ofPath::Command::moveTo` / `lineTo` → anchor without handles
- `ofPath::Command::bezierTo(cp1, cp2, to)` → anchor with `handleIn` / `handleOut`
- `ofPath::Command::close` → `path.closed = true`

### Standalone usage

The Path Editor can also be used independently of the ECS — just open the
window, draw a path, and read the result:

```cpp
// The runtime stores the ImVectorEditor::Path internally.
// You can read it by drawing into the window and applying to an entity,
// or extend the API to expose the path for custom use.
```

---

## Adding these addons to your own project

Add them to your `addons.make`:

```
ofxImGuiFileDialog
ofxImGuiTextEdit
ofxImGuiVectorEditor
ofxImGuizmo
ofxKit
```

No extra include is needed in your `ofApp.h` — Runtime.h pulls all four headers
transitively.
