# App Preferences Window

The **Preferences** window is a two-pane category/page editor that exposes openFrameworks global settings and lets addons contribute their own pages.

Open it from **View → Preferences** in the main menu bar.

---

## Layout

```
┌─────────────────────────────────────────────────────────┐
│  openFrameworks          General                         │
│  ──────────────          ──────────────────              │
│    General               Frame                           │
│    Rendering               Target FPS  [60]       ▲     │
│    Logging                 Actual FPS  60.0        │     │
│    Status Bar              Vertical Sync  ☑         │     │
│                          Background                       │
│  My Addon                  Auto Clear  ☑             │     │
│  ──────────────            Background Colour  ████  ▼   │
│    Settings                                              │
└─────────────────────────────────────────────────────────┘
```

- **Left pane** — category headers (bold label + separator) followed by selectable page entries.
- **Right pane** — the content of the selected page, scrollable.

---

## Built-in pages

### openFrameworks / General

| Control           | OF call                     | Default      |
|-------------------|-----------------------------|--------------|
| Target FPS        | `ofSetFrameRate(n)`         | 60           |
| Actual FPS        | read-only display           | —            |
| Vertical Sync     | `ofSetVerticalSync(bool)`   | on           |
| Auto Clear        | `ofSetBackgroundAuto(bool)` | on           |
| Background Colour | `ofBackground(ofColor)`     | (18, 18, 24) |
| Window size/pos   | read-only display           | —            |
| Set Title         | `ofSetWindowTitle(s)`       | —            |

### openFrameworks / Rendering

| Control           | OF call                                         | Default |
|-------------------|-------------------------------------------------|---------|
| Circle Resolution | `ofSetCircleResolution(n)`                      | 22      |
| Line Width        | `ofSetLineWidth(px)`                            | 1.0 px  |
| Smooth Lighting   | `ofSetSmoothLighting(bool)`                     | on      |
| Depth Test        | `ofEnableDepthTest()` / `ofDisableDepthTest()`  | off     |

### openFrameworks / Logging

Combo box mapping to `ofLogLevel`:

| Index | Label       | OF constant          |
|-------|-------------|----------------------|
| 0     | Verbose     | `OF_LOG_VERBOSE`     |
| 1     | Notice      | `OF_LOG_NOTICE`      |
| 2     | Warning     | `OF_LOG_WARNING`     |
| 3     | Error       | `OF_LOG_ERROR`       |
| 4     | Fatal Error | `OF_LOG_FATAL_ERROR` |
| 5     | Silent      | `OF_LOG_SILENT`      |

### openFrameworks / Status Bar

A list of all registered status bar items with a checkbox per item.
Toggling a checkbox immediately shows/hides that item in the bar.
See [`status-bar.md`](status-bar.md) for the full status item registry API.

---

## Persistence

Settings are saved to `data/ofxKit/appPrefs.json` on every change and applied on startup:

```json
{
  "circleRes": 64,
  "targetFps": 60,
  "vsync": true,
  "backgroundAuto": true,
  "lineWidth": 1.0,
  "smoothLighting": true,
  "depthTest": false,
  "logLevel": 1,
  "bgR": 18, "bgG": 18, "bgB": 24, "bgA": 255
}
```

---

## Addon integration — `registerPreferencePage()`

Addons can contribute their own pages to any category. Built-in pages are always registered first; addon pages follow in registration order.

```cpp
// Minimal example — one page under "My Addon"
ofkitty::runtime().registerPreferencePage({
    "My Addon",              // category label (becomes a header in the left pane)
    "Settings",              // page name shown as a selectable item
    "myaddon.prefs.main",    // unique id — must be globally unique
    [&]() {                  // draw callback — just emit widgets, no Begin/End needed
        ImGui::Checkbox("Enable feature", &myFeature);
        if (ImGui::SliderInt("Detail level", &myDetail, 1, 8))
            myAddon.setDetail(myDetail);
    }
});
```

### `PreferencePage` descriptor

| Field      | Type                  | Description                                         |
|------------|-----------------------|-----------------------------------------------------|
| `category` | `std::string`         | Category header label (e.g. `"openFrameworks"`)     |
| `name`     | `std::string`         | Selectable page label (e.g. `"Rendering"`)          |
| `id`       | `std::string`         | Globally unique page id (e.g. `"ofxkit.prefs.rendering"`) |
| `draw`     | `std::function<void()>` | Widget draw callback                              |

### API

```cpp
// Register
ofkitty::runtime().registerPreferencePage(page);

// Unregister — returns false if id not found
bool removed = ofkitty::runtime().unregisterPreferencePage("myaddon.prefs.main");

// Read all registered pages
const std::vector<Runtime::PreferencePage>& pages = ofkitty::runtime().preferencePages();
```

### When to register

Register in `setup()` or in a one-time kit-init helper. The Runtime does not guard against registration after the first frame; it is safe to register or unregister pages at any time.

---

## Extending an existing category

Multiple addons can share the same category string — they will be listed under the same header:

```cpp
// ofxMyAddon contributes two pages to the "My Addon" category
ofkitty::runtime().registerPreferencePage({"My Addon", "Connection", "myaddon.prefs.conn", drawConn});
ofkitty::runtime().registerPreferencePage({"My Addon", "Export",     "myaddon.prefs.exp",  drawExp});
```
