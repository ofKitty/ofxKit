#pragma once

#include "imgui.h"

#include <string>

namespace ofkitty::detail {

std::string makeImGuiIdFromLabel(const std::string& label);
void createParentDirectoryIfNeeded(const std::string& path);

/// True when the left button was released without a meaningful drag (click pick).
inline bool isClickWithoutDrag(float maxDragPx = 5.f)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        return false;
    const ImVec2 delta(io.MousePos.x - io.MouseClickedPos[0].x,
                       io.MousePos.y - io.MouseClickedPos[0].y);
    return (delta.x * delta.x + delta.y * delta.y) <= maxDragPx * maxDragPx;
}

}
