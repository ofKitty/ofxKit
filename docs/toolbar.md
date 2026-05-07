# Toolbar

The **Toolbar** is a compact floating ImGui panel that is always visible (not restricted to Edit mode). It shows a vertical stack of 32×32 icon buttons — one per registered `ToolbarItem`. Addons and sketches register items to give the user a tool-picker without building their own window.

The Toolbar window is hidden automatically when there are no items registered, so it costs nothing if unused.

## Registering a tool

```cpp
ofkitty::runtime().registerToolbarItem({
    "myapp.select",          // id — unique dot-namespaced string
    ICON_FA_MOUSE_POINTER,   // icon — Font Awesome glyph or any short UTF-8 string
    "Select (V)",            // tooltip shown on hover
    [&]{ return tool == SELECT; },   // isActive — button is highlighted when true
    [&]{ tool = SELECT; }            // onSelect — called on click
    // group omitted → no separator grouping
});
```

All fields except `id` and `icon` are optional (callbacks can be `nullptr`).

## Groups and separators

Items with the same non-empty `group` string are clustered together. A horizontal separator is drawn between each group transition.

```cpp
// Selection tools group
runtime().registerToolbarItem({"app.select", ICON_FA_MOUSE_POINTER, "Select",
    [&]{ return tool == SELECT; }, [&]{ tool = SELECT; }, "selection"});
runtime().registerToolbarItem({"app.direct", ICON_FA_LOCATION_ARROW, "Direct Select",
    [&]{ return tool == DIRECT; }, [&]{ tool = DIRECT; }, "selection"});

// Draw tools group — separator is automatically inserted above
runtime().registerToolbarItem({"app.pen", ICON_FA_BEZIER_CURVE, "Pen",
    [&]{ return tool == PEN; }, [&]{ tool = PEN; }, "draw"});
runtime().registerToolbarItem({"app.rect", ICON_FA_SQUARE, "Rectangle",
    [&]{ return tool == RECT; }, [&]{ tool = RECT; }, "draw"});
```

Items with no group and items in different groups are separated as follows:

- A separator appears between any two consecutive items whose `group` fields differ and at least one of them is non-empty.
- Items with an empty `group` are treated as their own group of one.

## ToolbarItem fields

| Field | Type | Description |
|---|---|---|
| `id` | `std::string` | Unique identifier; required |
| `icon` | `const char*` | FA glyph literal or short label displayed on the button |
| `tooltip` | `std::string` | Shown in an ImGui tooltip on hover |
| `isActive` | `function<bool()>` | Returns `true` when the button should be highlighted |
| `onSelect` | `function<void()>` | Called when the button is clicked |
| `group` | `std::string` | Clustering key for separator placement |

## Using Font Awesome icons

ofxKit doesn't load a Font Awesome font itself — that's left to the host app so it can choose atlas size, glyph ranges, and scale. Load the font in a `addPostSetupHook` callback:

```cpp
#include "IconsFontAwesome5.h"  // from ofxImGui or your own copy

runtime().addPostSetupHook([](ofxImGui::Gui& gui) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 14.0f);

    static const ImWchar faRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.GlyphMinAdvanceX = 14.0f;
    io.Fonts->AddFontFromFileTTF("fonts/" FONT_ICON_FILE_NAME_FAS, 14.0f, &cfg, faRanges);
});
```

If no icon font is loaded, pass a short ASCII string (e.g. `"Sel"`, `"Pen"`) as the `icon` field — the button will still work, just without vector glyphs.

## Removing an item at runtime

```cpp
ofkitty::runtime().unregisterToolbarItem("myapp.select");
```

Returns `true` if the item was found and removed.

## Toggling toolbar visibility

The Toolbar is registered as a `RuntimeWindow` and appears in the **View** menu:

```cpp
ofkitty::runtime().setWindowVisible("Toolbar", false); // hide
ofkitty::runtime().setWindowVisible("Toolbar", true);  // show
```
