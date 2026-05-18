#include "LayersPanel.h"
#include "../ReorderDragDrop.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <ofxImGuiStyle/src/ofxImGuiStyle.h>
#include <ofxImGuiStyle/src/IconsFontAwesome5.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include <functional>

namespace ofkitty {

// ============================================================================
// Relationship helpers (file-local)
// ============================================================================

static void rel_linkChild(entt::registry& reg,
                          entt::entity parent, entt::entity child)
{
    auto& rel        = reg.get<ecs::Relationship>(child);
    rel.parent       = parent;
    rel.next_sibling = entt::null;
    rel.prev_sibling = entt::null;

    if (parent != entt::null) {
        auto& pr = reg.get<ecs::Relationship>(parent);
        pr.children_count++;
        if (pr.first_child == entt::null) {
            pr.first_child = child;
        } else {
            entt::entity last = pr.first_child;
            while (reg.get<ecs::Relationship>(last).next_sibling != entt::null)
                last = reg.get<ecs::Relationship>(last).next_sibling;
            reg.get<ecs::Relationship>(last).next_sibling = child;
            rel.prev_sibling = last;
        }
    } else {
        // Root: append after the last existing root (next_sibling == null)
        entt::entity lastRoot = entt::null;
        auto view = reg.view<ecs::layer_component, ecs::Relationship>();
        for (auto e : view) {
            if (e == child) continue;
            auto& er = reg.get<ecs::Relationship>(e);
            if (er.parent == entt::null && er.next_sibling == entt::null)
                lastRoot = e;
        }
        if (lastRoot != entt::null) {
            reg.get<ecs::Relationship>(lastRoot).next_sibling = child;
            rel.prev_sibling = lastRoot;
        }
    }
}

static void rel_unlinkChild(entt::registry& reg, entt::entity e)
{
    if (!reg.valid(e)) return;
    auto& rel = reg.get<ecs::Relationship>(e);

    if (rel.prev_sibling != entt::null && reg.valid(rel.prev_sibling))
        reg.get<ecs::Relationship>(rel.prev_sibling).next_sibling = rel.next_sibling;
    if (rel.next_sibling != entt::null && reg.valid(rel.next_sibling))
        reg.get<ecs::Relationship>(rel.next_sibling).prev_sibling = rel.prev_sibling;

    if (rel.parent != entt::null && reg.valid(rel.parent)) {
        auto& pr = reg.get<ecs::Relationship>(rel.parent);
        if (pr.first_child == e) pr.first_child = rel.next_sibling;
        if (pr.children_count > 0) pr.children_count--;
    }

    rel.parent = rel.prev_sibling = rel.next_sibling = entt::null;
}

// ============================================================================
// Setup & callbacks
// ============================================================================

void LayersPanel::setup(entt::registry* reg, entt::entity* activeLayer)
{
    reg_         = reg;
    activeLayer_ = activeLayer;
}

void LayersPanel::setOnAddLayer(std::function<void()> cb)
    { onAdd_ = std::move(cb); }

void LayersPanel::setOnRemoveLayer(std::function<void(entt::entity)> cb)
    { onRemove_ = std::move(cb); }

void LayersPanel::setOnReparent(
    std::function<void(entt::entity, entt::entity, entt::entity)> cb)
    { onReparent_ = std::move(cb); }

void LayersPanel::setOnLayerChanged(std::function<void()> cb)
    { onChanged_ = std::move(cb); }

void LayersPanel::setGetBadge(std::function<std::string(entt::entity)> cb)
    { getBadge_ = std::move(cb); }

void LayersPanel::setDrawContextMenu(std::function<void(entt::entity)> cb)
    { drawContextMenu_ = std::move(cb); }

// ============================================================================
// Default reparent (Relationship surgery only — no flat-cache rebuild)
// ============================================================================

void LayersPanel::defaultReparent(entt::entity child,
                                  entt::entity newParent,
                                  entt::entity insertBefore)
{
    if (!reg_ || !reg_->valid(child)) return;
    if (newParent != entt::null && !reg_->valid(newParent)) return;
    if (child == newParent) return;

    // Cycle prevention
    {
        entt::entity check = newParent;
        while (check != entt::null && reg_->valid(check)) {
            if (check == child) return;
            check = reg_->get<ecs::Relationship>(check).parent;
        }
    }

    auto& reg = *reg_;
    rel_unlinkChild(reg, child);

    if (insertBefore != entt::null && reg.valid(insertBefore)) {
        auto& ibRel    = reg.get<ecs::Relationship>(insertBefore);
        auto& childRel = reg.get<ecs::Relationship>(child);

        childRel.parent       = newParent;
        childRel.next_sibling = insertBefore;
        childRel.prev_sibling = ibRel.prev_sibling;

        if (ibRel.prev_sibling != entt::null && reg.valid(ibRel.prev_sibling))
            reg.get<ecs::Relationship>(ibRel.prev_sibling).next_sibling = child;
        ibRel.prev_sibling = child;

        if (newParent != entt::null && reg.valid(newParent)) {
            auto& pr = reg.get<ecs::Relationship>(newParent);
            pr.children_count++;
            if (pr.first_child == insertBefore) pr.first_child = child;
        }
    } else {
        rel_linkChild(reg, newParent, child);
    }
}

// ============================================================================
// Helpers
// ============================================================================

entt::entity LayersPanel::findFirstRoot() const
{
    if (!reg_) return entt::null;
    auto view = reg_->view<ecs::layer_component, ecs::Relationship>();
    for (auto e : view) {
        auto& rel = reg_->get<ecs::Relationship>(e);
        if (rel.parent == entt::null && rel.prev_sibling == entt::null)
            return e;
    }
    return entt::null;
}

// ============================================================================
// Row draw helper
// ============================================================================

void LayersPanel::drawLayerRow(entt::entity e, int depth, float totalW,
                               bool showToggleCol)
{
    if (!reg_->valid(e)) return;

    auto& reg = *reg_;
    auto& lc  = reg.get<ecs::layer_component>(e);
    auto& rel = reg.get<ecs::Relationship>(e);
    const bool hasChildren = rel.first_child != entt::null;
    const uint32_t eKey    = entt::to_integral(e);

    ImGui::PushID((int)eKey);

    const ImVec2 rowScreenPos = ImGui::GetCursorScreenPos();
    const float  rowH         = ImGui::GetFrameHeight();
    const bool   selected     = (e == *activeLayer_);

    // Selection highlight
    if (selected) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            rowScreenPos,
            ImVec2(rowScreenPos.x + totalW, rowScreenPos.y + rowH),
            ImGui::GetColorU32(ImGuiCol_Header));
    }

    // Depth indent
    const float indentW = 14.f;
    if (depth > 0) ImGui::Indent(depth * indentW);

    // Collapse / expand toggle (or alignment spacer for leaves) —
    // only rendered when the tree actually has hierarchy.
    if (showToggleCol) {
        if (hasChildren) {
            bool& collapsed = collapsed_[eKey];
            if (ofxImGuiStyle::IconButtonGhost(
                    collapsed ? ICON_FA_CARET_RIGHT : ICON_FA_CARET_DOWN, "##tog"))
                collapsed = !collapsed;
        } else {
            // Non-interactive spacer to keep eye/lock/swatch aligned with toggled rows
            ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
        }
        ImGui::SameLine();
    }

    // Eye
    if (ofxImGuiStyle::IconButtonGhost(
            lc.visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH, "##eye")) {
        lc.visible = !lc.visible;
        if (onChanged_) onChanged_();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(lc.visible ? "Hide layer" : "Show layer");
    ImGui::SameLine();

    // Lock
    if (ofxImGuiStyle::IconButtonGhost(
            lc.locked ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN, "##lock"))
        lc.locked = !lc.locked;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(lc.locked ? "Unlock layer" : "Lock layer");
    ImGui::SameLine();

    // Colour swatch
    {
        const float swSz = ImGui::GetFrameHeight();
        ImVec4 col(lc.color.r / 255.f, lc.color.g / 255.f,
                   lc.color.b / 255.f, 1.f);
        ImGui::PushStyleColor(ImGuiCol_Button,        col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  col);
        if (ImGui::Button("##sw", ImVec2(swSz, swSz)))
            ImGui::OpenPopup("##colpop");
        ImGui::PopStyleColor(3);

        if (ImGui::BeginPopup("##colpop")) {
            float c[3] = { col.x, col.y, col.z };
            if (ImGui::ColorPicker3("##cp", c)) {
                lc.color.r = (unsigned char)(c[0] * 255.f);
                lc.color.g = (unsigned char)(c[1] * 255.f);
                lc.color.b = (unsigned char)(c[2] * 255.f);
                if (onChanged_) onChanged_();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::SameLine();

    const float nameX  = ImGui::GetCursorPosX();
    const float rightX = ImGui::GetContentRegionMax().x;

    // Badge
    std::string badgeStr;
    if (getBadge_) badgeStr = getBadge_(e);
    const float badgeW = badgeStr.empty()
        ? 0.f
        : ImGui::CalcTextSize(badgeStr.c_str()).x
          + ImGui::GetStyle().ItemSpacing.x;

    // Transparent selectable — covers the full name area for click + DnD.
    // Pass selected=false to suppress Selectable's own background draw;
    // our manual AddRectFilled above is the sole selection visual.
    // AllowDoubleClick lets us detect rename in the same callback.
    const float selectW = rightX - nameX;
    if (ImGui::Selectable("##sel", false,
            ImGuiSelectableFlags_AllowOverlap
            | ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(selectW, 0))) {
        if (ImGui::IsMouseDoubleClicked(0)) {
            renaming_          = e;
            justStartedRename_ = true;
            std::strncpy(renameBuf_, lc.name.c_str(), sizeof(renameBuf_) - 1);
            renameBuf_[sizeof(renameBuf_) - 1] = '\0';
        } else {
            *activeLayer_ = e;
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Double-click to rename");

    // 3-zone DnD
    {
        auto r = ReorderDragDropRow("OFKIT_LAYER", e, lc.name.c_str(),
                                    rowScreenPos.y, rowScreenPos.y + rowH);
        if (r.accepted && r.dragged != r.target) {
            entt::entity tgtParent = reg.get<ecs::Relationship>(r.target).parent;
            entt::entity insertBefore = entt::null;

            switch (r.zone) {
            case DropZone::Before:
                insertBefore = r.target;
                break;
            case DropZone::After:
                insertBefore = reg.get<ecs::Relationship>(r.target).next_sibling;
                break;
            case DropZone::Into:
                tgtParent    = r.target;
                insertBefore = entt::null;
                break;
            }

            if (onReparent_)
                onReparent_(r.dragged, tgtParent, insertBefore);
            else
                defaultReparent(r.dragged, tgtParent, insertBefore);

            if (onChanged_) onChanged_();
        }
    }

    // Name / rename
    ImGui::SameLine(nameX);
    const float maxNameW = selectW - badgeW - ImGui::GetStyle().ItemSpacing.x;

    if (renaming_ == e) {
        ImGui::SetNextItemWidth(maxNameW > 40.f ? maxNameW : 40.f);
        if (justStartedRename_) {
            ImGui::SetKeyboardFocusHere();
            justStartedRename_ = false;
            renameGrace_ = 3;
        }
        if (ImGui::InputText("##rename", renameBuf_, sizeof(renameBuf_),
                ImGuiInputTextFlags_EnterReturnsTrue
                | ImGuiInputTextFlags_AutoSelectAll)) {
            if (renameBuf_[0] != '\0') lc.name = renameBuf_;
            renaming_    = entt::null;
            renameGrace_ = 0;
            if (onChanged_) onChanged_();
        }
        if (renameGrace_ > 0) {
            renameGrace_--;
        } else if (!ImGui::IsItemActive() && !ImGui::IsItemFocused()) {
            renaming_ = entt::null;
        }
    } else {
        if (!lc.visible)
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

        ImGui::PushClipRect(
            ImVec2(ImGui::GetWindowPos().x + nameX,              rowScreenPos.y),
            ImVec2(ImGui::GetWindowPos().x + rightX - badgeW,    rowScreenPos.y + rowH),
            true);
        ImGui::TextUnformatted(lc.name.c_str());
        ImGui::PopClipRect();

        if (!lc.visible) ImGui::PopStyleColor();
    }

    // Badge
    if (!badgeStr.empty()) {
        ImGui::SameLine(rightX - badgeW + ImGui::GetStyle().ItemSpacing.x);
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(badgeStr.c_str());
        ImGui::PopStyleColor();
    }

    // Per-row context menu
    if (ImGui::BeginPopupContextItem("##ctx")) {
        if (drawContextMenu_) drawContextMenu_(e);
        ImGui::EndPopup();
    }

    if (depth > 0) ImGui::Unindent(depth * indentW);

    ImGui::PopID();
}

// ============================================================================
// Draw
// ============================================================================

void LayersPanel::draw(const char* imguiTitle, bool& visible)
{
    if (!reg_ || !activeLayer_) return;
    if (!ImGui::Begin(imguiTitle, &visible)) { ImGui::End(); return; }

    auto& reg = *reg_;

    // ---- Toolbar -----------------------------------------------------------
    const size_t totalLayers = reg.storage<ecs::layer_component>().size();
    const bool   canRemove   = totalLayers > 1;

    bool canUp = false, canDown = false;
    if (*activeLayer_ != entt::null && reg.valid(*activeLayer_)
        && reg.all_of<ecs::Relationship>(*activeLayer_))
    {
        auto& ar = reg.get<ecs::Relationship>(*activeLayer_);
        canUp    = ar.prev_sibling != entt::null;
        canDown  = ar.next_sibling != entt::null;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 2.f));

    if (ImGui::Button(ICON_FA_PLUS "##add")) {
        if (onAdd_) onAdd_();
        else {
            // Default: create a new root layer
            auto e = reg.create();
            ecs::layer_component lc;
            lc.index   = (int)totalLayers;
            lc.name    = "Layer " + std::to_string(lc.index + 1);
            lc.color   = ofColor(120, 140, 180);
            lc.visible = true;
            lc.locked  = false;
            reg.emplace<ecs::layer_component>(e, lc);
            if (!reg.all_of<ecs::Relationship>(e))
                reg.emplace<ecs::Relationship>(e);
            rel_linkChild(reg, entt::null, e);
            *activeLayer_ = e;
        }
        if (onChanged_) onChanged_();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add layer");

    ImGui::SameLine();
    if (!canRemove) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_TRASH_ALT "##remove")) {
        entt::entity target = *activeLayer_;
        if (onRemove_) {
            onRemove_(target);
        } else {
            rel_unlinkChild(reg, target);
            if (reg.valid(target)) reg.destroy(target);
            entt::entity fr = findFirstRoot();
            if (fr != entt::null) *activeLayer_ = fr;
        }
        if (onChanged_) onChanged_();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Remove layer");
    if (!canRemove) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!canUp) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_ARROW_UP "##up")) {
        entt::entity active = *activeLayer_;
        if (reg.valid(active) && reg.all_of<ecs::Relationship>(active)) {
            auto& ar = reg.get<ecs::Relationship>(active);
            entt::entity prev = ar.prev_sibling;
            if (prev != entt::null) {
                if (onReparent_)
                    onReparent_(active, ar.parent, prev);
                else
                    defaultReparent(active, ar.parent, prev);
                if (onChanged_) onChanged_();
            }
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Move up");
    if (!canUp) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!canDown) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_ARROW_DOWN "##down")) {
        entt::entity active = *activeLayer_;
        if (reg.valid(active) && reg.all_of<ecs::Relationship>(active)) {
            auto& ar   = reg.get<ecs::Relationship>(active);
            entt::entity next = ar.next_sibling;
            if (next != entt::null) {
                entt::entity insertBefore =
                    reg.get<ecs::Relationship>(next).next_sibling;
                if (onReparent_)
                    onReparent_(active, ar.parent, insertBefore);
                else
                    defaultReparent(active, ar.parent, insertBefore);
                if (onChanged_) onChanged_();
            }
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Move down");
    if (!canDown) ImGui::EndDisabled();

    ImGui::PopStyleVar();
    ImGui::Separator();

    // ---- Build DFS traversal list (pre-collect so DnD mods take next frame) -
    struct LayerRow { entt::entity e; int depth; };
    std::vector<LayerRow> rows;
    rows.reserve((int)totalLayers);

    // Show the collapse-toggle column only when there is actual hierarchy
    // (at least one entity has children). A flat list skips the column to
    // avoid the empty left margin.
    bool showToggleCol = false;
    {
        auto cv = reg.view<ecs::layer_component, ecs::Relationship>();
        for (auto ent : cv) {
            if (reg.get<ecs::Relationship>(ent).first_child != entt::null) {
                showToggleCol = true;
                break;
            }
        }
    }

    std::function<void(entt::entity, int)> collect = [&](entt::entity e, int d) {
        if (!reg.valid(e)) return;
        rows.push_back({e, d});
        auto& rel = reg.get<ecs::Relationship>(e);
        if (rel.first_child != entt::null
            && !collapsed_[entt::to_integral(e)])
        {
            entt::entity child = rel.first_child;
            while (child != entt::null) {
                collect(child, d + 1);
                child = reg.get<ecs::Relationship>(child).next_sibling;
            }
        }
    };

    entt::entity firstRoot = findFirstRoot();
    entt::entity root = firstRoot;
    while (root != entt::null) {
        collect(root, 0);
        root = reg.get<ecs::Relationship>(root).next_sibling;
    }

    // ---- Draw rows ---------------------------------------------------------
    const float totalW = ImGui::GetContentRegionAvail().x;
    for (auto& row : rows)
        drawLayerRow(row.e, row.depth, totalW, showToggleCol);

    ImGui::End();
}

} // namespace ofkitty
