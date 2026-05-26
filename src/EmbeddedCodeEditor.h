#pragma once

#include "ofxImGuiTextEdit.h"
#include "imgui.h"

namespace ofkitty {

/// Draw a fixed-height TextEditor region (Properties panel, inline inspectors).
/// Returns true if text changed this frame.
bool drawEmbeddedCodeEditor(TextEditor& editor,
                            float heightPx,
                            bool readOnly = false);

} // namespace ofkitty
