# Appearance and Styling

`ofxKit` does not own theming or font loading — it consumes both from
[`ofxImGuiStyle`](../../ofxImGuiStyle/README.md):

- **`ImTheme`** — built-in themes (Darcula, Material Flat, ImGui dark, ...)
  plus a `RegisterCustom` registry that addons populate at startup, and
  the `ShowSelector` / `ShowThemeTweakGui` UI.
- **`ImFonts`** — bundled Input Sans + Font Awesome 5 Solid, loaded into
  the `ofxImGui::Gui` font atlas at runtime startup.

`ofxKit` provides the *editor integration* around those pieces:

- The **Appearance** preferences page drives `ImTheme::ShowSelector`,
  `ImTheme::ApplyRandomAccent`, `ImTheme::ShowThemeTweakGui`, and
  `.bin` `SaveStyle` / `LoadStyle`.
- The **main-menu Theme** group exposes a quick-pick subset of the
  built-in `ImTheme` themes (Darcula Darker, Darcula, ImGui Dark,
  Material Flat, Light Rounded, ImGui Light), with "more themes in
  Preferences..." for the full registry.
- The **UI Scale** action and persistence of the selected theme + scale
  to `data/` (or `data/<Runtime::dataSubdir()>/` if configured).

---

## What gets persisted

`data/theme.json`:

```json
{ "theme": "DarculaDarker" }
```

`data/uiScale.json`:

```json
{ "uiScale": 1.0 }
```

- `theme` is whatever `ImTheme::ApplyByName(id)` accepts — either a
  built-in `ImTheme::Name(Theme_*)` string (`Darcula`, `DarculaDarker`,
  `ImGuiColorsDark`, `MaterialFlat`, ...) or a custom-registered id from
  some addon (`tb303`, `tr808`, `tr909`, `wasp`, ...).
- `uiScale` is fed straight to `ImTheme::ApplyScale(scale)`.

The default is `Runtime::kDefaultThemeId` (`"DarculaDarker"`). If the
persisted id resolves to nothing (addon uninstalled, typo in a hand-edited
file, ...) `Runtime::applyTheme()` logs a warning and falls back to that
default.

---

## Adding your own theme from an addon

Split the palette body from the registration so the body has no
dependency on `ofxKit`:

```cpp
// myAddonTheme.h — palette body (no class state).
#pragma once
#include <ofxImGuiStyle/src/ImTheme.h>

namespace myaddon {
inline void applyThemeBody() {
    ImGui::GetStyle() = ImGuiStyle{};
    ImGui::StyleColorsDark();
    ImVec4* c = ImGui::GetStyle().Colors;
    c[ImGuiCol_CheckMark] = ImVec4(1.f, 0.55f, 0.f, 1.f);
    // ... your palette ...
    ImTheme::ApplyCompactMetrics();
}
} // namespace myaddon
```

```cpp
// myAddonKit.h — registers with the ofxKit Runtime when present.
#pragma once
#include <ofxKit/src/Runtime.h>

#if __has_include(<ofxImGuiStyle/src/ImThemeRegistry.h>)
  #include <ofxImGuiStyle/src/ImThemeRegistry.h>
  #include "myAddonTheme.h"
  #define MYADDON_HAS_IMTHEME 1
#endif

namespace myaddon::kit {
inline void applyTheme(ofkitty::Runtime& rt) {
#if MYADDON_HAS_IMTHEME
    ImTheme::RegisterCustom({"myaddon", "My Addon", &myaddon::applyThemeBody});
    rt.setTheme("myaddon");   // persists to theme.json
#else
    ofLogWarning("myaddon") << "ofxImGuiStyle (ImTheme) not available - theme will not be registered.";
#endif
}
} // namespace myaddon::kit
```

Then in `ofApp::setup()`:

```cpp
myaddon::kit::applyTheme(ofkitty::runtime());
```

The theme then appears in:

- **Preferences > Appearance** combo (next to the built-ins).
- The persisted `theme.json` if it remains selected at shutdown.

---

## Editing the active theme live

`ImTheme::ShowThemeTweakGui(&tweaks)` is a tabbed widget that surfaces:

- **Theme Tweaks** — high-level dials (rounding, hue rotation, value
  shift, ...) backed by `ImTheme::TweakedTheme`. Calling
  `ImTheme::ApplyTweakedTheme(tweaks)` re-renders the theme without
  losing your palette identity.
- **Style Editor** — the raw `ImGui::ShowStyleEditor()` for ad-hoc
  per-colour tweaks.

The Preferences > Appearance page hosts this widget directly; standalone
apps can host it in any window.
