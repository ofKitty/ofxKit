#include "ChainEditor.h"
#include "IconsFontAwesome5.h"

#include <algorithm>

namespace ofkitty {

void ChainEditor::draw()
{
    ImGui::PushID(m_payloadTag);

    const auto drawList = [&]() {
        int removeIdx = -1;

        for (int i = 0; i < m_count; ++i) {
            ImGui::PushID(i);

            const std::string label = m_label ? m_label(i) : ("Step " + std::to_string(i));
            bool enabled = m_isEnabled ? m_isEnabled(i) : true;

            ImGui::AlignTextToFramePadding();
            if (m_showDragHandle) {
                ImGui::TextDisabled(ICON_FA_GRIP_LINES);
                ImGui::SameLine();
            }

            if (m_setEnabled) {
                if (ImGui::Checkbox("##en", &enabled))
                    m_setEnabled(i, enabled);
                ImGui::SameLine();
            }

            const ImVec2 rowMin = ImGui::GetCursorScreenPos();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
            const bool open = ImGui::CollapsingHeader(label.c_str(), flags);
            const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetContentRegionAvail().x,
                                         rowMin.y + ImGui::GetFrameHeight());

            auto drop = ReorderDragDropIndexRow(m_payloadTag, i, label.c_str(),
                                                rowMin.y, rowMax.y + 4.f);
            if (drop.accepted && drop.dragged != drop.target && m_onMove) {
                int insert = drop.target;
                if (drop.zone == DropZone::After)
                    insert = drop.target + 1;
                if (drop.dragged < insert)
                    --insert;
                m_onMove(drop.dragged, insert);
            }

            if (open && m_drawStep) {
                ImGui::Indent();
                m_drawStep(i);
                if (m_showRemove && ImGui::Button("Remove")) removeIdx = i;
                ImGui::Unindent();
            }

            ImGui::PopID();
        }

        if (removeIdx >= 0 && m_onRemove)
            m_onRemove(removeIdx);
    };

    if (!m_sectionTitle.empty()) {
        const std::string hdr = m_sectionTitle + " (" + std::to_string(m_count) + ")";
        if (ImGui::CollapsingHeader(hdr.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            drawList();

            if (!m_addTypes.empty() && m_onAdd) {
                m_addIndex = std::clamp(m_addIndex, 0, (int)m_addTypes.size() - 1);
                std::vector<const char*> labels;
                labels.reserve(m_addTypes.size());
                for (const auto& s : m_addTypes) labels.push_back(s.c_str());

                const ImGuiStyle& st = ImGui::GetStyle();
                const float buttonW =
                    ImGui::CalcTextSize(m_addButtonLabel).x + st.FramePadding.x * 2.f;
                const float spacing = st.ItemSpacing.x;
                ImGui::SetNextItemWidth(
                    std::max(48.f, ImGui::GetContentRegionAvail().x - buttonW - spacing));
                ImGui::Combo("##addtype", &m_addIndex, labels.data(), (int)labels.size());
                ImGui::SameLine(0.f, spacing);
                if (ImGui::Button(m_addButtonLabel, ImVec2(buttonW, 0.f)) && m_addIndex > 0)
                    m_onAdd(m_addIndex);
            }

            if (!m_footerHint.empty())
                ImGui::TextDisabled("%s", m_footerHint.c_str());
        }
    } else {
        drawList();

        if (!m_addTypes.empty() && m_onAdd) {
            m_addIndex = std::clamp(m_addIndex, 0, (int)m_addTypes.size() - 1);
            std::vector<const char*> labels;
            labels.reserve(m_addTypes.size());
            for (const auto& s : m_addTypes) labels.push_back(s.c_str());

            const ImGuiStyle& st = ImGui::GetStyle();
            const float buttonW =
                ImGui::CalcTextSize(m_addButtonLabel).x + st.FramePadding.x * 2.f;
            const float spacing = st.ItemSpacing.x;
            ImGui::SetNextItemWidth(
                std::max(48.f, ImGui::GetContentRegionAvail().x - buttonW - spacing));
            ImGui::Combo("##addtype", &m_addIndex, labels.data(), (int)labels.size());
            ImGui::SameLine(0.f, spacing);
            if (ImGui::Button(m_addButtonLabel, ImVec2(buttonW, 0.f)) && m_addIndex > 0)
                m_onAdd(m_addIndex);
        }

        if (!m_footerHint.empty())
            ImGui::TextDisabled("%s", m_footerHint.c_str());
    }

    ImGui::PopID();
}

} // namespace ofkitty
