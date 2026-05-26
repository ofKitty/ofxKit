#pragma once

#include <entt/entt.hpp>
#include "imgui.h"

// ============================================================================
// ReorderDragDrop
// ============================================================================
// A reusable ImGui drag-and-drop helper for reorderable trees and lists.
// Uses entt::entity as the canonical payload.
//
// Three hit zones per row (top 1/3 / middle 1/3 / bottom 1/3):
//   Before — insert dragged as previous sibling of the target row
//   Into   — reparent dragged as a child of the target row
//   After  — insert dragged as next sibling of the target row
//
// --- Usage (call immediately after the row's interactive item) ---
//
//   auto r = ofkitty::ReorderDragDropRow("MY_TAG", entity, label,
//                                        rowMinY, rowMaxY);
//   if (r.accepted) {
//       switch (r.zone) {
//       case ofkitty::DropZone::Before: ...
//       case ofkitty::DropZone::Into:   ...
//       case ofkitty::DropZone::After:  ...
//       }
//   }
// ============================================================================

namespace ofkitty {

enum class DropZone { Before, Into, After };

struct ReorderDropResult {
    bool         accepted = false;
    entt::entity dragged  = entt::null;
    entt::entity target   = entt::null;
    DropZone     zone     = DropZone::Before;
};

/// Call immediately after the ImGui item (Selectable, TreeNodeEx, etc.) for
/// this row.
///
/// @param payloadTag   Unique string scoping this drag-drop list (e.g. "OFKIT_LAYER")
/// @param entity       The entity represented by this row
/// @param previewLabel Text shown in the drag tooltip
/// @param rowMinY      Screen Y of the row top    (ImGui::GetItemRectMin().y)
/// @param rowMaxY      Screen Y of the row bottom (ImGui::GetItemRectMax().y)
/// Index-based reorder (flat lists: pipeline steps, effect chains).
struct IndexDropResult {
    bool accepted = false;
    int  dragged  = -1;
    int  target   = -1;
    DropZone zone = DropZone::Before;
};

ReorderDropResult ReorderDragDropRow(const char*  payloadTag,
                                     entt::entity entity,
                                     const char*  previewLabel,
                                     float        rowMinY,
                                     float        rowMaxY);

IndexDropResult ReorderDragDropIndexRow(const char* payloadTag,
                                        int         index,
                                        const char* previewLabel,
                                        float       rowMinY,
                                        float       rowMaxY);

} // namespace ofkitty
