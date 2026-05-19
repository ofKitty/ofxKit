# Rulers

Rulers are thin measurement strips (horizontal + vertical) drawn along the edges of a region. They show coordinate values with tick marks and track the live mouse position with a red cursor marker.

Two use modes are available:

| Mode | What it measures | Draw list | Toggle |
|---|---|---|---|
| **Full-window** | Main viewport work area, always in pixels | `GetBackgroundDrawList` | `View → Rulers` / **F2** |
| **Per-panel** | Any ImGui content rectangle, configurable unit | `GetWindowDrawList` | per-panel option |

---

## Full-window rulers

The Runtime draws rulers along the top and left edges of the entire OS window when `m_showRulers` is true.

```
 [corner] ──────── horizontal ruler ────────────────────────────────────►  X
    │
    │   vertical ruler
    │
    │
    ▼
    Y
```

**Toggle:** `View → Rulers`  or  **F2**

```cpp
ofkitty::runtime().setShowRulers(true);
bool on = ofkitty::runtime().showRulers();
ofkitty::runtime().toggleRulers();
```

---

## Per-panel rulers — `drawRulersInRegion`

Any ImGui panel can draw its own rulers around an arbitrary content rectangle using the free function declared in `src/RulerUtil.h`:

```cpp
#include "RulerUtil.h"

ofkitty::drawRulersInRegion(
    dl,          // ImDrawList* — typically ImGui::GetWindowDrawList()
    origin,      // ImVec2 — screen-space top-left of the content area
    size,        // ImVec2 — width/height of the content area in screen pixels
    mouse,       // ImVec2 — ImGui::GetIO().MousePos
    pixPerUnit,  // float  — screen pixels per ruler unit (1.0 = px, scale for mm)
    unitLabel,   // const char* — "px", "mm", etc.
    uiScale,     // float  — ofkitty::runtime().uiScale()
    rulerScale); // float  — ofkitty::runtime().appPrefs().rulerScale
```

### Parameters

| Parameter | Typical value | Notes |
|---|---|---|
| `dl` | `ImGui::GetWindowDrawList()` | Using the window draw list keeps rulers clipped to the panel |
| `origin` | `ImGui::GetCursorScreenPos()` before the content | Top-left corner of the measured rectangle |
| `size` | Available content size in pixels | e.g. `ImGui::GetContentRegionAvail()` |
| `pixPerUnit` | `1.0` for pixels; `scale` for mm | `scale = avail_px / paper_mm` for a plotter panel |
| `unitLabel` | `"px"` or `"mm"` | Appended to tick labels |
| `uiScale` | `runtime().uiScale()` | Ruler strip thickness scales with this |
| `rulerScale` | `runtime().appPrefs().rulerScale` | User-set multiplier from Preferences |

### Tick interval

The helper auto-selects a nice tick interval (1, 2, 5, 10, 20, 25, 50, 100 … units) so labels stay readable regardless of zoom level or unit scale.

---

## Viewport panel rulers

`ViewportInstance` panels have a per-instance `showRulers` flag (default `false`). Toggle it from the panel's **View** menu or programmatically:

```cpp
auto* vp = runtime().addViewportWindow("Perspective");
vp->showRulers = true;
```

Viewport rulers always show pixels because the FBO fills the panel at 1:1 pixel ratio.

---

## Ruler unit preference

The **panel ruler unit** (Pixels / Millimetres) is set globally in **Preferences → Appearance → Rulers**. Panels that override their own `pixPerUnit` (e.g. the plotter Preview panel always uses mm when the paper scale is available) ignore this preference.

```cpp
// Read in a panel draw callback:
const auto& prefs = ofkitty::runtime().appPrefs();
bool useMM = (prefs.rulerUnit == ofkitty::Runtime::AppPrefs::RulerUnit::Millimetres);
```

---

## Coordinate system

- **Full-window rulers:** origin `(0, 0)` = top-left of the viewport work area (below menu bar). Units are always screen pixels.
- **Per-panel rulers:** origin `(0, 0)` = top-left of the passed `origin` point, measured in the chosen units.

---

## Implementation notes

- `drawRulersInRegion` is defined in `src/RulerUtil.cpp` and declared in `src/RulerUtil.h`.
- `Runtime::drawRulers()` is now a thin wrapper that passes the main viewport rect and `pixPerUnit = 1.0`.
- No inputs are captured by ruler strips — mouse events pass through.
- Per-panel rulers use `GetWindowDrawList()` so they are automatically clipped to the window bounds.
- Full-window rulers use `GetBackgroundDrawList()` so they draw behind all panels.
