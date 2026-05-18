#pragma once

#include <entt/entt.hpp>
#include <ofxEnTTKit/src/components/layer_components.h>
#include <ofxEnTTKit/src/components/hierarchy_components.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <atomic>

namespace ofkitty {

// ============================================================================
// LayersPanel
// ============================================================================
// A generic, reusable ImGui layer-management panel for any entt::registry
// that tags entities with ecs::layer_component + ecs::Relationship.
//
// The panel traverses the Relationship tree directly (depth-first via the
// sibling linked list), rendering an indented tree with collapse/expand per
// node.  Drag-and-drop supports three zones:
//   Before — reorder: insert dragged as previous sibling
//   Into   — reparent: make dragged a child of the target
//   After  — reorder: insert dragged as next sibling
//
// --- Minimum setup ---
//
//   ofkitty::LayersPanel layers;
//   layers.setup(&registry, &activeLayer);
//   // Inside a registerWindow lambda:
//   layers.draw("Layers###myapp.layers", visible);
//
// --- Domain-specific callbacks ---
//
//   setOnReparent  — called when DnD or Up/Down changes the hierarchy.
//                    Receives (child, newParent, insertBefore).
//                    insertBefore == entt::null means "append as last child".
//                    Default: modifies Relationship links directly in the
//                    registry (suitable when no flat-cache needs rebuilding).
//
//   setOnAddLayer  — override the "+" button.
//   setOnRemoveLayer — override the trash button.
//   setOnLayerChanged — visibility / lock / colour / rename changes.
//   setGetBadge    — right-aligned text per row (e.g. "12 paths").
//   setDrawContextMenu — extra items in the per-row right-click popup.
// ============================================================================

class LayersPanel {
public:
    LayersPanel() = default;

    /// Bind the panel to a registry.
    /// @param reg         Live registry owning the layer entities.
    /// @param activeLayer Currently selected entity; updated on row click.
    void setup(entt::registry* reg, entt::entity* activeLayer);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /// Called when the user clicks "+".
    /// Default: creates entity, emplaces ecs::layer_component as a new root.
    void setOnAddLayer(std::function<void()> cb);

    /// Called when the user deletes the active layer.
    /// Default: destroys entity and unlinks it from the Relationship tree.
    void setOnRemoveLayer(std::function<void(entt::entity)> cb);

    /// Called after DnD or Up/Down reorder.
    /// Signature: void(entt::entity child, entt::entity newParent,
    ///                 entt::entity insertBefore)
    ///   insertBefore == entt::null → append as last child of newParent.
    /// Default: performs Relationship surgery directly in the registry.
    void setOnReparent(std::function<void(entt::entity child,
                                          entt::entity newParent,
                                          entt::entity insertBefore)> cb);

    /// Called after visibility, lock, colour, rename, or reorder changes.
    void setOnLayerChanged(std::function<void()> cb);

    /// Return the right-aligned badge string for a row ("" to hide).
    void setGetBadge(std::function<std::string(entt::entity)> cb);

    /// Inject extra ImGui::MenuItem calls into the per-row right-click popup.
    void setDrawContextMenu(std::function<void(entt::entity)> cb);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(const char* imguiTitle, bool& visible);

private:
    entt::registry* reg_         {nullptr};
    entt::entity*   activeLayer_ {nullptr};

    std::function<void()>                        onAdd_;
    std::function<void(entt::entity)>            onRemove_;
    std::function<void(entt::entity, entt::entity, entt::entity)> onReparent_;
    std::function<void()>                        onChanged_;
    std::function<std::string(entt::entity)>     getBadge_;
    std::function<void(entt::entity)>            drawContextMenu_;

    // Per-entity collapse state (true = collapsed/closed)
    std::unordered_map<uint32_t, bool> collapsed_;

    // Inline rename state
    entt::entity renaming_          = entt::null;
    bool         justStartedRename_ = false;
    int          renameGrace_       = 0;
    char         renameBuf_[128]    = {};

    // Internal helpers
    void drawLayerRow(entt::entity e, int depth, float totalW, bool showToggleCol);
    entt::entity findFirstRoot() const;
    void defaultReparent(entt::entity child, entt::entity newParent,
                         entt::entity insertBefore);
};

} // namespace ofkitty
