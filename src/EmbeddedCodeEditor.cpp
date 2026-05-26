#include "EmbeddedCodeEditor.h"

namespace ofkitty {

bool drawEmbeddedCodeEditor(TextEditor& editor, float heightPx, bool readOnly)
{
    editor.SetReadOnlyEnabled(readOnly);
    const bool changed = editor.Render(
        "##embedded_code",
        true,
        ImVec2(-1.f, heightPx),
        true);
    const int lines = std::max(1, editor.GetLineCount());
    ImGui::TextDisabled("%d lines", lines);
    return changed;
}

} // namespace ofkitty
