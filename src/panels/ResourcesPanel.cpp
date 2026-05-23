#include "ResourcesPanel.h"
#include "../Runtime.h"

#include "imgui.h"
#include "ofFileUtils.h"

namespace ofkitty {

// ============================================================================
// Public API
// ============================================================================

void ResourcesPanel::setOnPlace(std::function<void(const Resource&)> cb)
{
    m_onPlace = std::move(cb);
}

void ResourcesPanel::setOnResourceLoaded(std::function<void(Resource&)> cb)
{
    m_onResourceLoaded = std::move(cb);
}

void ResourcesPanel::addResource(Resource r)
{
    if (!r.loaded) {
        switch (r.type) {
            case ResourceType::Image:        loadImage(r.path);  return;
            case ResourceType::VectorSVG:    loadSVG(r.path);    return;
            case ResourceType::GCodeSnippet: loadGCode(r.path);  return;
        }
    }
    m_resources.push_back(std::move(r));
    m_selected = (int)m_resources.size() - 1;
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

// ============================================================================
// File loading helpers
// ============================================================================

void ResourcesPanel::loadImage(const std::string& path)
{
    Resource r;
    r.type   = ResourceType::Image;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    r.loaded = r.thumbnail.load(path);
    if (r.loaded) {
        // Scale thumbnail down to a sensible maximum while preserving aspect ratio.
        const float maxDim = kThumbSize * 2.f;
        float w = (float)r.thumbnail.getWidth();
        float h = (float)r.thumbnail.getHeight();
        if (w > maxDim || h > maxDim) {
            float scale = maxDim / std::max(w, h);
            r.thumbnail.resize((int)(w * scale), (int)(h * scale));
        }
    }
    m_resources.push_back(std::move(r));
    m_selected = (int)m_resources.size() - 1;
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

void ResourcesPanel::loadSVG(const std::string& path)
{
    Resource r;
    r.type   = ResourceType::VectorSVG;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    // SVG thumbnails are generated externally by the host app via onPlace.
    // We just mark it as loaded if the file exists.
    r.loaded = ofFile(path).exists();
    m_resources.push_back(std::move(r));
    m_selected = (int)m_resources.size() - 1;
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

void ResourcesPanel::loadGCode(const std::string& path)
{
    Resource r;
    r.type = ResourceType::GCodeSnippet;
    r.path = path;
    r.name = ofFilePath::getFileName(path);
    ofBuffer buf = ofBufferFromFile(path);
    r.text   = buf.getText();
    r.loaded = !r.text.empty();
    m_resources.push_back(std::move(r));
    m_selected = (int)m_resources.size() - 1;
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

// ============================================================================
// Draw
// ============================================================================

void ResourcesPanel::draw(const char* title, bool& visible)
{
    if (!ImGui::Begin(title, &visible, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    drawMenuBar();
    drawList();

    ImGui::End();
}

// ----------------------------------------------------------------------------
void ResourcesPanel::drawMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("Load")) {
        if (ImGui::MenuItem("Image...")) {
            ofkitty::runtime().openFileDialog(
                "resources.load.image", "Load Image",
                ".png,.jpg,.jpeg,.bmp,.tga,.gif,.tif,.tiff",
                [this](const std::string& path) { loadImage(path); });
        }
        if (ImGui::MenuItem("SVG...")) {
            ofkitty::runtime().openFileDialog(
                "resources.load.svg", "Load SVG",
                ".svg",
                [this](const std::string& path) { loadSVG(path); });
        }
        if (ImGui::MenuItem("G-code snippet...")) {
            ofkitty::runtime().openFileDialog(
                "resources.load.gcode", "Load G-code",
                ".gcode,.ngc,.nc,.txt",
                [this](const std::string& path) { loadGCode(path); });
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

// ----------------------------------------------------------------------------
void ResourcesPanel::drawList()
{
    const float totalW   = ImGui::GetContentRegionAvail().x;
    const float btnH     = ImGui::GetFrameHeight();
    const float listH    = ImGui::GetContentRegionAvail().y - btnH - ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild("##res_list", ImVec2(totalW, listH), false);

    int  removeIdx = -1;
    int  placeIdx  = -1;

    for (int i = 0; i < (int)m_resources.size(); ++i) {
        Resource& r = m_resources[i];

        ImGui::PushID(i);

        // ---- status dot ----
        ImVec4 dotCol = r.loaded
            ? ImVec4(0.2f, 0.85f, 0.2f, 1.f)
            : ImVec4(0.85f, 0.2f, 0.2f, 1.f);
        ImGui::TextColored(dotCol, r.loaded ? "●" : "●");
        ImGui::SameLine();

        // ---- thumbnail or type label ----
        if (r.type == ResourceType::Image && r.thumbnail.isAllocated()
                && r.thumbnail.getTexture().isAllocated()) {
            ImTextureID texId = (ImTextureID)(uintptr_t)r.thumbnail.getTexture().getTextureData().textureID;
            float tw = (float)r.thumbnail.getWidth();
            float th = (float)r.thumbnail.getHeight();
            float aspect = (th > 0.f) ? tw / th : 1.f;
            float dh = kThumbSize;
            float dw = dh * aspect;
            ImGui::Image(texId, ImVec2(dw, dh));
            ImGui::SameLine();
        } else {
            const char* label = (r.type == ResourceType::VectorSVG) ? "[SVG]" : "[GCO]";
            ImGui::BeginGroup();
            const float iconSz = kThumbSize;
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                p, ImVec2(p.x + iconSz * 0.6f, p.y + iconSz * 0.6f),
                IM_COL32(60, 60, 70, 200), 4.f);
            ImGui::SetCursorScreenPos(ImVec2(p.x + 4.f, p.y + iconSz * 0.2f));
            ImGui::TextDisabled("%s", label);
            ImGui::SetCursorScreenPos(ImVec2(p.x + iconSz * 0.6f + 4.f, p.y));
            ImGui::EndGroup();
            ImGui::SameLine();
        }

        // ---- selectable name ----
        bool isSelected = (m_selected == i);
        ImGui::SetNextItemAllowOverlap();
        if (ImGui::Selectable(r.name.c_str(), isSelected,
                              ImGuiSelectableFlags_AllowDoubleClick,
                              ImVec2(0, kThumbSize))) {
            m_selected = i;
            if (ImGui::IsMouseDoubleClicked(0))
                placeIdx = i;
        }

        // ---- context menu ----
        if (ImGui::BeginPopupContextItem("##res_ctx")) {
            if (ImGui::MenuItem("Place")) placeIdx  = i;
            if (ImGui::MenuItem("Remove")) removeIdx = i;
            ImGui::EndPopup();
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", r.path.c_str());

        ImGui::PopID();
    }

    ImGui::EndChild();

    // ---- Place button (bottom) ----
    bool canPlace = (m_selected >= 0 && m_selected < (int)m_resources.size()
                     && m_resources[m_selected].loaded);
    if (!canPlace) ImGui::BeginDisabled();
    if (ImGui::Button("Place##res", ImVec2(-1.f, 0.f)))
        placeIdx = m_selected;
    if (!canPlace) ImGui::EndDisabled();

    // ---- deferred actions ----
    if (placeIdx >= 0 && placeIdx < (int)m_resources.size() && m_onPlace)
        m_onPlace(m_resources[placeIdx]);

    if (removeIdx >= 0 && removeIdx < (int)m_resources.size()) {
        m_resources.erase(m_resources.begin() + removeIdx);
        if (m_selected >= (int)m_resources.size())
            m_selected = (int)m_resources.size() - 1;
    }
}

void ResourcesPanel::placeSelected()
{
    if (m_selected >= 0 && m_selected < (int)m_resources.size() && m_onPlace)
        m_onPlace(m_resources[m_selected]);
}

} // namespace ofkitty
