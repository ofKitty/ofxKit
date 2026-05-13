# Appearance and Styling

`ofxKit` uses `ofxImGuiStyle` for reusable ImGui styling mechanics:

- loading the bundled Input Sans and Font Awesome fonts
- applying the built-in dark, light, and classic theme presets
- applying compact scrollbar/grab metrics
- generating random accent themes
- capturing and scaling the unscaled `ImGuiStyle` baseline
- saving and loading binary `ImGuiStyle` theme files

`ofxKit` owns the editor integration around those features:

- the Appearance preferences page
- the main-menu theme and UI-scale actions
- persistence of the selected theme and UI scale
- editor-window policies such as minimum docked window size

This keeps `ofxImGuiStyle` reusable for any `ofxImGui` app while letting
`ofxKit` present those controls as part of the ofKitty editor shell.
