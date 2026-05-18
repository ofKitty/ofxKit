#include "ReorderDragDrop.h"
#include "imgui_internal.h"
#include <cstdint>
#include <cstdio>

namespace ofkitty {

ReorderDropResult ReorderDragDropRow(const char*  payloadTag,
                                     entt::entity entity,
                                     const char*  previewLabel,
                                     float        rowMinY,
                                     float        rowMaxY)
{
    ReorderDropResult result;
    const float rowH = rowMaxY - rowMinY;

    // ---- Drag source -------------------------------------------------------
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        uint32_t id = static_cast<uint32_t>(entt::to_integral(entity));
        ImGui::SetDragDropPayload(payloadTag, &id, sizeof(id));
        ImGui::TextUnformatted(previewLabel);
        ImGui::EndDragDropSource();
    }

    // ---- Drop target -------------------------------------------------------
    if (!ImGui::BeginDragDropTarget())
        return result;

    const float mouseY   = ImGui::GetIO().MousePos.y;
    const float zoneH    = rowH / 3.0f;
    DropZone    hoverZone = DropZone::Into;

    if      (mouseY < rowMinY + zoneH)          hoverZone = DropZone::Before;
    else if (mouseY > rowMaxY - zoneH)          hoverZone = DropZone::After;
    else                                        hoverZone = DropZone::Into;

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    const float winL  = ImGui::GetWindowPos().x;
    const float winR  = winL + ImGui::GetWindowSize().x;
    const ImU32 kLine = IM_COL32(0, 190, 255, 255);
    const ImU32 kFill = IM_COL32(0, 190, 255, 40);

    if (hoverZone == DropZone::Before || hoverZone == DropZone::After) {
        const float lineY = (hoverZone == DropZone::Before) ? rowMinY : rowMaxY;
        dl->AddLine(ImVec2(winL, lineY), ImVec2(winR, lineY), kLine, 2.f);
        dl->AddTriangleFilled(
            ImVec2(winL,     lineY - 4.f),
            ImVec2(winL,     lineY + 4.f),
            ImVec2(winL + 7, lineY),      kLine);
        dl->AddTriangleFilled(
            ImVec2(winR,     lineY - 4.f),
            ImVec2(winR,     lineY + 4.f),
            ImVec2(winR - 7, lineY),      kLine);
    } else {
        // Into: highlight the whole row
        dl->AddRect(ImVec2(winL, rowMinY), ImVec2(winR, rowMaxY), kLine, 0.f, 0, 2.f);
        dl->AddRectFilled(ImVec2(winL, rowMinY), ImVec2(winR, rowMaxY), kFill);
    }

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
            payloadTag, ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
        uint32_t id      = *static_cast<const uint32_t*>(payload->Data);
        result.accepted  = true;
        result.dragged   = static_cast<entt::entity>(id);
        result.target    = entity;
        result.zone      = hoverZone;
    }

    ImGui::EndDragDropTarget();
    return result;
}

} // namespace ofkitty
