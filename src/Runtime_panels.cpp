#include "Runtime.h"
#include "Runtime_private.h"

#include "component_editor_registration.h"
#include "ofxEnTTKit.h"
#include "ofxEnTTInspector.h"
#include "IconsFontAwesome5.h"
#include "ImFonts.h"

#include <imgui.h>

#include <algorithm>

namespace ofkitty {

namespace {

void drawEntityNode(entt::registry& reg, entt::entity e, entt::entity& selected)
{
    if (!reg.valid(e))
        return;

    std::string name;
    if (reg.all_of<ecs::Relationship, ecs::LocalTransform, ecs::GlobalTransform>(e))
        name = ofxNode::fromEntity(reg, e).getName();
    if (name.empty()) {
        if (auto* nc = reg.try_get<ecs::node_component>(e))
            name = nc->getName();
    }
    if (name.empty())
        name = "Entity " + std::to_string(static_cast<uint32_t>(e));

    auto* rel         = reg.try_get<ecs::Relationship>(e);
    bool  hasChildren = rel && rel->children_count > 0;

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (e == selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(static_cast<int>(e));
    bool open = ImGui::TreeNodeEx(name.c_str(), flags);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        selected = e;

    if (open && hasChildren) {
        if (reg.all_of<ecs::Relationship, ecs::LocalTransform, ecs::GlobalTransform>(e)) {
            ofxNode node = ofxNode::fromEntity(reg, e);
            node.forEachChild([&](ofxNode child) {
                drawEntityNode(reg, child.entity(), selected);
            });
        } else {
            entt::entity child = rel->first_child;
            while (child != entt::null) {
                drawEntityNode(reg, child, selected);
                auto* cr = reg.try_get<ecs::Relationship>(child);
                child = cr ? cr->next_sibling : entt::null;
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

} // anonymous namespace

void Runtime::registerToolbarItem(ToolbarItem item)
{
    if (item.id.empty()) {
        ofLogWarning("ofxKit") << "Cannot register a toolbar item with no id.";
        return;
    }
    for (const auto& existing : m_toolbarItems) {
        if (existing.id == item.id) {
            ofLogWarning("ofxKit") << "Toolbar item '" << item.id << "' is already registered.";
            return;
        }
    }
    m_toolbarItems.push_back(std::move(item));
}

bool Runtime::unregisterToolbarItem(const std::string& id)
{
    auto it = std::find_if(m_toolbarItems.begin(), m_toolbarItems.end(),
                           [&](const ToolbarItem& t) { return t.id == id; });
    if (it == m_toolbarItems.end())
        return false;
    m_toolbarItems.erase(it);
    return true;
}

void Runtime::drawToolbarWindow(bool& visible)
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 10, vp->WorkPos.y + 50),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

    if (ImGui::Begin("Toolbar###ofxkit.window.toolbar", &visible, flags)) {

        ImFonts::ToolbarIconButton(ICON_FA_GRIP_VERTICAL, "##drag", true);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drag to move toolbar");
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            ImVec2 p = ImGui::GetWindowPos();
            ImGui::SetWindowPos(ImVec2(p.x + d.x, p.y + d.y));
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
        if (ImFonts::ToolbarIconButton(ICON_FA_TIMES, "##close", true))
            visible = false;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Close Toolbar");
        ImGui::PopStyleColor(2);

        ImGui::Separator();

        const ImVec4 activeCol(0.2f, 0.6f, 0.9f, 1.0f);

        std::string prevGroup = "\x01";

        for (const auto& item : m_toolbarItems) {
            if (!item.group.empty() && item.group != prevGroup && prevGroup != "\x01")
                ImGui::Separator();
            prevGroup = item.group;

            bool active = item.isActive && item.isActive();
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, activeCol);

            ImGui::PushID(item.id.c_str());
            bool clicked = false;
            if (item.icon && item.icon[0] != '\0') {
                clicked = ImFonts::ToolbarIconButton(item.icon, "##tool");
            } else {
                const float side = ImGui::GetFontSize() + 8.f;
                clicked = ImGui::Button(item.id.c_str(), ImVec2(side, side));
            }
            ImGui::PopID();

            if (active)
                ImGui::PopStyleColor();

            if (!item.tooltip.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", item.tooltip.c_str());

            if (clicked && item.onSelect)
                item.onSelect();
        }
    }
    ImGui::PopStyleVar(2);
    ImGui::End();
}

void Runtime::registerBuiltInWindows()
{
    if (m_builtInWindowsRegistered)
        return;

    // Returns true when the window should be registered: either auto-registration
    // is on (default), or the app explicitly requested this window via
    // addBuiltInWindow() using its display name or stable ID.
    auto wantWindow = [&](const std::string& name, const std::string& id) -> bool {
        if (m_autoRegisterBuiltIns) return true;
        return m_requestedBuiltInWindows.count(name) > 0
            || m_requestedBuiltInWindows.count(id)   > 0;
    };

    if (wantWindow("Toolbar", "ofxkit.window.toolbar")) {
        registerToolbarItem({"ofkitty.gizmo_translate",
                             ICON_FA_ARROWS_ALT,
                             "Translate (W)",
                             "gizmo",
                             [this] { m_gizmoOp = GizmoOperation::Translate; },
                             [this] { return m_gizmoOp == GizmoOperation::Translate; }});
        registerToolbarItem({"ofkitty.gizmo_rotate",
                             ICON_FA_SYNC_ALT,
                             "Rotate (E)",
                             "gizmo",
                             [this] { m_gizmoOp = GizmoOperation::Rotate; },
                             [this] { return m_gizmoOp == GizmoOperation::Rotate; }});
        registerToolbarItem({"ofkitty.gizmo_scale",
                             ICON_FA_EXPAND_ALT,
                             "Scale (R)",
                             "gizmo",
                             [this] { m_gizmoOp = GizmoOperation::Scale; },
                             [this] { return m_gizmoOp == GizmoOperation::Scale; }});
        registerToolbarItem({"ofkitty.gizmo_universal",
                             ICON_FA_CROSSHAIRS,
                             "Universal",
                             "gizmo",
                             [this] { m_gizmoOp = GizmoOperation::Universal; },
                             [this] { return m_gizmoOp == GizmoOperation::Universal; }});
        registerToolbarItem({"ofkitty.gizmo_world",
                             ICON_FA_GLOBE,
                             "World space",
                             "gizmo_mode",
                             [this] { m_gizmoMode = GizmoMode::World; },
                             [this] { return m_gizmoMode == GizmoMode::World; }});
        registerToolbarItem({"ofkitty.gizmo_local",
                             ICON_FA_CUBE,
                             "Local space",
                             "gizmo_mode",
                             [this] { m_gizmoMode = GizmoMode::Local; },
                             [this] { return m_gizmoMode == GizmoMode::Local; }});
        registerWindow({"Toolbar",
                        "View",
                        true,
                        true,
                        [this](bool& visible) { drawToolbarWindow(visible); },
                        "ofxkit.window.toolbar"});
    }

    if (wantWindow("Scene", "ofxkit.window.scene")) {
        registerWindow({"Scene",
                        "View",
                        true,
                        true,
                        [this](bool& visible) { drawSceneWindow(visible); },
                        "ofxkit.window.scene"});
    }

    if (wantWindow("Properties", "ofxkit.window.properties")) {
        registerWindow({"Properties",
                        "View",
                        true,
                        true,
                        [this](bool& visible) { drawPropertiesWindow(visible); },
                        "ofxkit.window.properties"});
    }

    if (wantWindow("Shortcuts", "ofxkit.window.shortcuts")) {
        registerWindow({"Shortcuts",
                        "View",
                        false,
                        true,
                        [this](bool& visible) { drawShortcutsWindow(visible); },
                        "ofxkit.window.shortcuts"});
    }

    if (wantWindow("Preferences", "ofxkit.window.preferences")) {
        registerWindow({"Preferences",
                        "",
                        false,
                        true,
                        [this](bool& visible) { drawPreferencesWindow(visible); },
                        "ofxkit.window.preferences"});
    }

    if (wantWindow("Code Editor", "ofxkit.window.code_editor")) {
        registerWindow({"Code Editor",
                        "View",
                        false,
                        true,
                        [this](bool& visible) { drawCodeEditorWindow(visible); },
                        "ofxkit.window.code_editor"});
    }

    if (wantWindow("Path Editor", "ofxkit.window.path_editor")) {
        registerWindow({"Path Editor",
                        "View",
                        false,
                        true,
                        [this](bool& visible) { drawPathEditorWindow(visible); },
                        "ofxkit.window.path_editor"});
    }

    m_builtInWindowsRegistered = true;
}

void Runtime::drawSceneWindow(bool& visible)
{
    auto& reg = registry();

    ImGui::SetNextWindowPos(ImVec2(10, 40), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(280, 500), ImGuiCond_Once);

    if (!ImGui::Begin("Scene###ofxkit.window.scene", &visible)) {
        ImGui::End();
        return;
    }

    int total = static_cast<int>(reg.storage<entt::entity>().free_list());
    ImGui::TextDisabled("%d entities", total);
    ImGui::Separator();
    ImGui::Spacing();

    for (auto [e, rel] : reg.view<ecs::Relationship>().each())
        if (rel.parent == entt::null)
            drawEntityNode(reg, e, m_selected);

    for (auto [e, nc] : reg.view<ecs::node_component>().each())
        if (!reg.all_of<ecs::Relationship>(e))
            drawEntityNode(reg, e, m_selected);

    ImGui::End();
}

void Runtime::drawPropertiesWindow(bool& visible)
{
    auto& reg = registry();

    ImGui::SetNextWindowPos(ImVec2(320, 40), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(360, 500), ImGuiCond_Once);

    if (ImGui::Begin("Properties###ofxkit.window.properties", &visible)) {
        const bool hasEntity =
            m_selected != entt::null && reg.valid(m_selected);

        if (m_propertiesSupplement) {
            m_propertiesSupplement();
            if (hasEntity) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
        }

        if (hasEntity) {
            inspector::inspectEntity(reg, m_selected);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("+ Add Component", {-1.f, 0.f}))
                ImGui::OpenPopup("##addComponentPopup");

            if (ImGui::BeginPopup("##addComponentPopup")) {
                ImGui::TextDisabled("── Add Component ──");
                ImGui::Separator();

                std::string lastCat;
                for (const auto& desc : ecs::componentMenuEntries()) {
                    if (desc.category != lastCat) {
                        if (!lastCat.empty())
                            ImGui::Separator();
                        ImGui::TextDisabled("%s", desc.category.c_str());
                        lastCat = desc.category;
                    }

                    bool alreadyHas = desc.has && desc.has(reg, m_selected);
                    if (alreadyHas) {
                        ImGui::PushStyleColor(ImGuiCol_Text,
                                              ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                        ImGui::MenuItem(("  " + desc.name).c_str(), nullptr, false, false);
                        ImGui::PopStyleColor();
                    } else {
                        if (ImGui::MenuItem(("  " + desc.name).c_str())) {
                            if (desc.add)
                                desc.add(reg, m_selected);
                        }
                    }
                    if (!desc.description.empty() && ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", desc.description.c_str());
                }
                ImGui::EndPopup();
            }
        } else if (!m_propertiesSupplement) {
            ImGui::TextDisabled("Select an entity in the Scene window.");
        }
    }
    ImGui::End();
}

void Runtime::drawShortcutsWindow(bool& visible)
{
    ImGui::SetNextWindowPos(ImVec2(10, 550), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Once);

    if (ImGui::Begin("Shortcuts###ofxkit.window.shortcuts", &visible)) {
        ImGui::TextWrapped("Named shortcuts can be remapped below; changes are saved to %s",
                           ShortcutManager::defaultBindingsPath().c_str());
        ImGui::Spacing();

        if (ImGui::BeginTable("shortcut_rows", 3, ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 90.f);
            ImGui::TableHeadersRow();

            for (const auto& s : m_shortcuts.all()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(s.description.c_str());
                ImGui::TableSetColumnIndex(1);
                const bool capturing =
                    m_shortcuts.isCapturing() && m_shortcuts.captureActionId() == s.actionId;
                if (capturing) {
                    ImGui::TextColored(ImVec4(1.f, 0.85f, 0.2f, 1.f),
                                       "Press new shortcut... (Esc cancel)");
                } else {
                    ImGui::TextUnformatted(
                        ShortcutManager::formatBindingLabel(s.key, s.modifiers).c_str());
                }
                ImGui::TableSetColumnIndex(2);
                if (!s.actionId.empty()) {
                    ImGui::PushID(s.actionId.c_str());
                    if (capturing) {
                        if (ImGui::Button("Cancel"))
                            m_shortcuts.cancelCapture();
                    } else if (ImGui::Button("Change...")) {
                        m_shortcuts.beginCapture(s.actionId);
                    }
                    ImGui::PopID();
                } else {
                    ImGui::TextDisabled("—");
                }
            }
            ImGui::EndTable();
        }

        if (!m_shortcuts.lastCaptureError().empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "%s",
                               m_shortcuts.lastCaptureError().c_str());
        }

        ImGui::Spacing();
        if (ImGui::Button("Save bindings now"))
            m_shortcuts.saveBindingsToFile(ShortcutManager::defaultBindingsPath());
        ImGui::SameLine();
        if (ImGui::Button("Reload from disk"))
            m_shortcuts.loadBindingsFromFile(ShortcutManager::defaultBindingsPath());
    }
    ImGui::End();
}

} // namespace ofkitty
