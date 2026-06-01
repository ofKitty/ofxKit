#include "Runtime.h"
#include "ViewWindow.h"
#include "Runtime_private.h"

#include "component_editor_registration.h"
#include "ofxEnTTKit_all.h"
#include "ofxEnTTInspector.h"
#include "IconsFontAwesome5.h"
#include "ImFonts.h"

#include <imgui.h>

#include <algorithm>

namespace ofkitty {

namespace {

void drawEntityNode(entt::registry& reg, entt::entity e)
{
    if (!reg.valid(e))
        return;

    const entt::entity selected = Runtime::instance().selected();

    std::string name;
    if (reg.all_of<ecs::Relationship, ecs::LocalTransform, ecs::GlobalTransform>(e))
        name = ofxNode::fromEntity(reg, e).getName();
    if (name.empty()) {
        if (auto* nc = reg.try_get<ecs::node_component>(e))
            name = nc->getName();
    }
    if (name.empty())
        name = Runtime::instance().entityTreeLabel(reg, e);
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
        Runtime::instance().select(e);

    if (open && hasChildren) {
        if (reg.all_of<ecs::Relationship, ecs::LocalTransform, ecs::GlobalTransform>(e)) {
            ofxNode node = ofxNode::fromEntity(reg, e);
            node.forEachChild([&](ofxNode child) {
                drawEntityNode(reg, child.entity());
            });
        } else {
            entt::entity child = rel->first_child;
            while (child != entt::null) {
                drawEntityNode(reg, child);
                auto* cr = reg.try_get<ecs::Relationship>(child);
                child = cr ? cr->next_sibling : entt::null;
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void drawSwatchLibraryNode(entt::registry& reg, entt::entity e)
{
    if (!reg.valid(e) || !reg.all_of<ecs::swatch_library_component>(e))
        return;

    auto& lib = reg.get<ecs::swatch_library_component>(e);
    const entt::entity selected = Runtime::instance().selected();

    std::string name = lib.libraryName.empty() ? "Swatch Library" : lib.libraryName;
    name += " (" + std::to_string(lib.count()) + " colors)";

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (lib.empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (e == selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(static_cast<int>(e));
    const bool open = ImGui::TreeNodeEx(name.c_str(), flags);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        Runtime::instance().select(e);

    if (open && !lib.empty()) {
        for (int i = 0; i < lib.count(); i++) {
            ImGui::PushID(i + 40000);
            const std::string& swName = lib.colors[i].getDisplayName();
            ImGuiTreeNodeFlags leafFlags =
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen
                | ImGuiTreeNodeFlags_SpanAvailWidth;
            ImGui::TreeNodeEx(swName.c_str(), leafFlags);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                Runtime::instance().select(e);
            ImGui::PopID();
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
    const float fs   = ImGui::GetFontSize();
    const float pad  = std::max(2.f, fs * 0.12f);
    const float side = fs + pad * 2.f;
    const float padX = ImGui::GetStyle().WindowPadding.x;
    const float padY = ImGui::GetStyle().WindowPadding.y;
    const float narrowW = side + padX * 2.f + 4.f;
    const float narrowH = side + padY * 2.f + 4.f;

    if (m_toolbarHorizontal) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(64.f, narrowH),
                                            ImVec2(FLT_MAX, narrowH + 2.f));
    } else {
        ImGui::SetNextWindowSizeConstraints(ImVec2(narrowW, 64.f),
                                            ImVec2(narrowW + 2.f, FLT_MAX));
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 10, vp->WorkPos.y + 50),
                            ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

    if (ImGui::Begin("Toolbar###ofxkit.window.toolbar", &visible, flags)) {
        const bool floating = (ImGui::GetWindowDockID() == 0);

        bool horizontal = false;
        if (ImGuiDockNode* dockNode = ImGui::GetWindowDockNode()) {
            // Wide dock slot → icon row; tall narrow slot → icon column.
            horizontal = dockNode->Size.x > dockNode->Size.y * 1.15f;
        } else {
            const ImVec2 sz = ImGui::GetWindowSize();
            horizontal      = sz.x > sz.y * 1.25f;
        }

        m_toolbarHorizontal = horizontal;

        auto drawGroupSeparator = [&](bool afterFirst) {
            if (!afterFirst) {
                return;
            }
            if (horizontal) {
                ImGui::SameLine(0.f, 4.f);
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine(0.f, 4.f);
            } else {
                ImGui::Separator();
            }
        };

        if (floating) {
            const char* gripIcon =
                horizontal ? ICON_FA_GRIP_HORIZONTAL : ICON_FA_GRIP_VERTICAL;
            ImFonts::ToolbarIconButton(gripIcon, "##drag", true);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Drag to move (dock to top/bottom for a row, left/right for a "
                    "column)");
            }
            if (ImGui::IsItemActive()
                && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 d = ImGui::GetIO().MouseDelta;
                ImVec2 p = ImGui::GetWindowPos();
                ImGui::SetWindowPos(ImVec2(p.x + d.x, p.y + d.y));
            }

            ImGui::SameLine(0.f, 4.f);

            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImVec4(0.7f, 0.2f, 0.2f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
            if (ImFonts::ToolbarIconButton(ICON_FA_TIMES, "##close", true))
                visible = false;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Close Toolbar");
            ImGui::PopStyleColor(2);

            if (horizontal) {
                ImGui::SameLine(0.f, 4.f);
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine(0.f, 4.f);
            } else {
                ImGui::Separator();
            }
        }

        // Compact cluster — does not stretch tools across a wide/tall dock host.
        ImGui::BeginGroup();

        const ImVec4 activeCol(0.2f, 0.6f, 0.9f, 1.0f);
        std::string  prevGroup = "\x01";
        bool         firstTool = true;

        for (const auto& item : m_toolbarItems) {
            const bool newGroup =
                !item.group.empty() && item.group != prevGroup
                && prevGroup != "\x01";
            if (newGroup)
                drawGroupSeparator(!firstTool);
            prevGroup = item.group;

            if (horizontal && !firstTool)
                ImGui::SameLine(0.f, 2.f);

            bool active = item.isActive && item.isActive();
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, activeCol);

            ImGui::PushID(item.id.c_str());
            bool clicked = false;
            if (item.icon && item.icon[0] != '\0') {
                clicked = ImFonts::ToolbarIconButton(item.icon, "##tool");
            } else {
                clicked = ImGui::Button(item.id.c_str(), ImVec2(side, side));
            }
            ImGui::PopID();

            if (active)
                ImGui::PopStyleColor();

            if (!item.tooltip.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", item.tooltip.c_str());

            if (clicked && item.onSelect)
                item.onSelect();

            firstTool = false;
        }

        ImGui::EndGroup();
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
        registerWindow(makeViewWindow(
            "Toolbar",
            [this](bool& visible) { drawToolbarWindow(visible); },
            {.id = "ofxkit.window.toolbar"}));
    }

    if (wantWindow("Scene", "ofxkit.window.scene")) {
        registerWindow(makeViewWindow(
            "Scene",
            [this](bool& visible) { drawSceneWindow(visible); },
            {.id = "ofxkit.window.scene"}));
    }

    if (wantWindow("Properties", "ofxkit.window.properties")) {
        registerWindow(makeViewWindow(
            "Properties",
            [this](bool& visible) { drawPropertiesWindow(visible); },
            {.id = "ofxkit.window.properties"}));
    }

    if (wantWindow("Shortcuts", "ofxkit.window.shortcuts")) {
        registerWindow(makeViewWindow(
            "Shortcuts",
            [this](bool& visible) { drawShortcutsWindow(visible); },
            {.visible = false, .id = "ofxkit.window.shortcuts"}));
    }

    if (wantWindow("Preferences", "ofxkit.window.preferences")) {
        registerWindow(makeViewWindow(
            "Preferences",
            [this](bool& visible) { drawPreferencesWindow(visible); },
            {.menuGroup = "", .visible = false, .id = "ofxkit.window.preferences"}));
    }

    if (wantWindow("Code Editor", "ofxkit.window.code_editor")) {
        registerWindow(makeViewWindow(
            "Code Editor",
            [this](bool& visible) { drawCodeEditorWindow(visible); },
            {.visible = false, .id = "ofxkit.window.code_editor"}));
    }

    if (wantWindow("Path Editor", "ofxkit.window.path_editor")) {
        registerWindow(makeViewWindow(
            "Path Editor",
            [this](bool& visible) { drawPathEditorWindow(visible); },
            {.visible = false, .id = "ofxkit.window.path_editor"}));
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
            drawEntityNode(reg, e);

    for (auto [e, nc] : reg.view<ecs::node_component>().each())
        if (!reg.all_of<ecs::Relationship>(e))
            drawEntityNode(reg, e);

    for (auto [e, _] : reg.view<ecs::selectable_component>().each()) {
        if (reg.all_of<ecs::Relationship>(e) || reg.all_of<ecs::node_component>(e))
            continue;
        if (!Runtime::instance().entityTreeLabel(reg, e).empty())
            drawEntityNode(reg, e);
    }

    for (auto [e, _] : reg.view<ecs::swatch_library_component>().each()) {
        if (reg.all_of<ecs::Relationship>(e) || reg.all_of<ecs::node_component>(e))
            continue;
        drawSwatchLibraryNode(reg, e);
    }

    ImGui::End();
}

void Runtime::drawPropertiesWindow(bool& visible)
{
    auto& reg = registry();

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
            if (inspector::inspectEntity(reg, m_selected)) {
                if (m_onEntityInspectorChanged)
                    m_onEntityInspectorChanged(m_selected);
            }

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
