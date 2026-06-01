#include "ResourcesPanel.h"

#include "../Runtime.h"
#include "ImLed.h"

#include "ofxEnTTKit_all.h"
#include "imgui.h"
#include "ofFileUtils.h"

namespace ofkitty {

namespace {

entt::registry& resourceRegistry()
{
    return runtime().registry();
}

const char* resourceTypeLabel(ResourceType t)
{
    switch (t) {
        case ResourceType::Image:          return "IMG";
        case ResourceType::VectorSVG:      return "SVG";
        case ResourceType::VectorDXF:      return "DXF";
        case ResourceType::Video:          return "VID";
        case ResourceType::GCodeSnippet:   return "GCO";
        case ResourceType::GeneratedGCode: return "JOB";
        case ResourceType::TextSnippet:    return "TXT";
        case ResourceType::GenericFile:    return "FILE";
        default:                           return "???";
    }
}

ImU32 resourceTypeBadgeColor(ResourceType t)
{
    switch (t) {
        case ResourceType::Image:          return IM_COL32( 60, 100, 160, 220);
        case ResourceType::VectorSVG:      return IM_COL32( 80, 140,  80, 220);
        case ResourceType::VectorDXF:      return IM_COL32(120, 120,  60, 220);
        case ResourceType::Video:          return IM_COL32(140,  60, 140, 220);
        case ResourceType::GCodeSnippet:   return IM_COL32(160, 110,  40, 220);
        case ResourceType::GeneratedGCode: return IM_COL32(160,  60,  60, 220);
        case ResourceType::TextSnippet:    return IM_COL32( 80, 110, 130, 220);
        default:                           return IM_COL32( 70,  70,  80, 220);
    }
}

bool isImageExt(const std::string& ext)
{
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp"
        || ext == "tga" || ext == "gif" || ext == "tif" || ext == "tiff";
}

bool isVideoExt(const std::string& ext)
{
    return ext == "mp4" || ext == "mov" || ext == "avi" || ext == "mkv"
        || ext == "webm" || ext == "wmv" || ext == "m4v" || ext == "flv";
}

bool isGCodeExt(const std::string& ext)
{
    return ext == "gcode" || ext == "ngc" || ext == "nc";
}

bool isTextSnippetExt(const std::string& ext)
{
    return ext == "txt" || ext == "md" || ext == "csv" || ext == "json"
        || ext == "xml" || ext == "yaml" || ext == "yml" || ext == "py"
        || ext == "js" || ext == "glsl" || ext == "vert" || ext == "frag";
}

/// OF dragEvent position is window-relative (see ofAppGLFWWindow::drop_cb).
void drawDashedRect(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 col)
{
    const float dash = 6.f, gap = 4.f;
    for (float x = p0.x; x < p1.x; x += dash + gap) {
        const float xe = std::min(x + dash, p1.x);
        dl->AddLine(ImVec2(x, p0.y),     ImVec2(xe, p0.y),     col, 1.f);
        dl->AddLine(ImVec2(x, p1.y - 1), ImVec2(xe, p1.y - 1), col, 1.f);
    }
    for (float y = p0.y; y < p1.y; y += dash + gap) {
        const float ye = std::min(y + dash, p1.y);
        dl->AddLine(ImVec2(p0.x, y),     ImVec2(p0.x, ye),     col, 1.f);
        dl->AddLine(ImVec2(p1.x - 1, y), ImVec2(p1.x - 1, ye), col, 1.f);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// ECS helpers
// ---------------------------------------------------------------------------

entt::entity ResourcesPanel::createResourceEntity(Resource& r)
{
    auto& reg = resourceRegistry();
    auto  e   = reg.create();

    reg.emplace<ecs::tag_component>(e, r.name);
    reg.emplace<ecs::filepath_component>(e, r.path);
    reg.emplace<ecs::selectable_component>(e, false);

    if (r.type == ResourceType::Image && r.loaded)
        reg.emplace<ecs::image_component>(e, r.path);

    if (r.type == ResourceType::GCodeSnippet
        || r.type == ResourceType::GeneratedGCode
        || r.type == ResourceType::TextSnippet) {
        ecs::code_snippet_component snip(r.text, ecs::code_language::Gcode);
        snip.readOnly = (r.type == ResourceType::GeneratedGCode);
        reg.emplace<ecs::code_snippet_component>(e, std::move(snip));
    }

    r.entity = e;
    return e;
}

void ResourcesPanel::updateResourceEntity(Resource& r)
{
    auto& reg = resourceRegistry();
    if (r.entity == entt::null || !reg.valid(r.entity)) {
        createResourceEntity(r);
        return;
    }

    if (reg.all_of<ecs::tag_component>(r.entity))
        reg.get<ecs::tag_component>(r.entity).tag = r.name;
    if (reg.all_of<ecs::filepath_component>(r.entity))
        reg.get<ecs::filepath_component>(r.entity).path = r.path;

    if (r.type == ResourceType::GCodeSnippet
        || r.type == ResourceType::GeneratedGCode
        || r.type == ResourceType::TextSnippet) {
        if (!reg.all_of<ecs::code_snippet_component>(r.entity))
            reg.emplace<ecs::code_snippet_component>(r.entity);
        auto& snip    = reg.get<ecs::code_snippet_component>(r.entity);
        snip.text     = r.text;
        snip.readOnly = (r.type == ResourceType::GeneratedGCode);
    }
}

void ResourcesPanel::destroyResourceEntity(entt::entity e)
{
    auto& reg = resourceRegistry();
    if (e == entt::null || !reg.valid(e))
        return;

    if (runtime().selected() == e)
        runtime().select(entt::null);

    reg.destroy(e);
}

void ResourcesPanel::selectResourceIndex(int index)
{
    m_selected = index;
    if (index < 0 || index >= (int)m_resources.size())
        return;

    Resource& r = m_resources[index];
    if (r.entity != entt::null && resourceRegistry().valid(r.entity))
        runtime().select(r.entity);

    if (m_onResourceSelected)
        m_onResourceSelected(r);
}

std::string ResourcesPanel::snippetTextFromEntity(const Resource& r) const
{
    if (r.entity != entt::null && resourceRegistry().valid(r.entity)
        && resourceRegistry().all_of<ecs::code_snippet_component>(r.entity))
        return resourceRegistry().get<ecs::code_snippet_component>(r.entity).text;
    return r.text;
}

void ResourcesPanel::syncTextFromEntity(entt::entity entity)
{
    auto& reg = resourceRegistry();
    if (entity == entt::null || !reg.valid(entity)
        || !reg.all_of<ecs::code_snippet_component>(entity))
        return;

    const auto& text = reg.get<ecs::code_snippet_component>(entity).text;
    for (auto& r : m_resources) {
        if (r.entity == entity) {
            r.text = text;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void ResourcesPanel::setOnPlace(std::function<void(const Resource&)> cb)
{
    m_onPlace = std::move(cb);
}

void ResourcesPanel::setOnResourceSelected(std::function<void(const Resource&)> cb)
{
    m_onResourceSelected = std::move(cb);
}

void ResourcesPanel::setOnResourceLoaded(std::function<void(Resource&)> cb)
{
    m_onResourceLoaded = std::move(cb);
}

void ResourcesPanel::setOnResourceEntityChanged(std::function<void(entt::entity)> cb)
{
    m_onResourceEntityChanged = std::move(cb);
}

// ---------------------------------------------------------------------------
// Public add / OS drag-drop
// ---------------------------------------------------------------------------

void ResourcesPanel::addResource(Resource r)
{
    if (!r.loaded) {
        switch (r.type) {
            case ResourceType::Image:          loadImage(r.path);       return;
            case ResourceType::VectorSVG:      loadSVG(r.path);         return;
            case ResourceType::VectorDXF:      loadDXF(r.path);         return;
            case ResourceType::Video:          loadVideo(r.path);       return;
            case ResourceType::GCodeSnippet:   loadGCode(r.path);       return;
            case ResourceType::TextSnippet:    loadTextSnippet(r.path); return;
            case ResourceType::GenericFile:    loadGenericFile(r.path); return;
            case ResourceType::GeneratedGCode: break;
        }
    }
    m_resources.push_back(std::move(r));
    createResourceEntity(m_resources.back());
    selectResourceIndex((int)m_resources.size() - 1);
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

void ResourcesPanel::acceptFileDrop(const std::vector<std::filesystem::path>& paths, glm::vec2 /*dropPos*/)
{
    if (paths.empty()) return;
    for (const auto& p : paths)
        dispatchFilePath(p.string());
}

void ResourcesPanel::dispatchFilePath(const std::string& path)
{
    const std::string ext = ofToLower(ofFilePath::getFileExt(path));

    if (isImageExt(ext))            loadImage(path);
    else if (ext == "svg")          loadSVG(path);
    else if (ext == "dxf")          loadDXF(path);
    else if (isVideoExt(ext))       loadVideo(path);
    else if (isGCodeExt(ext))       loadGCode(path);
    else if (isTextSnippetExt(ext)) loadTextSnippet(path);
    else                            loadGenericFile(path);
}

// ---------------------------------------------------------------------------
// Generated G-code slot
// ---------------------------------------------------------------------------

entt::entity ResourcesPanel::addOrUpdateGeneratedJob(const std::string& text,
                                                     const std::string& suggestedName)
{
    const std::string dir  = ofToDataPath("generated/", true);
    const std::string path = ofFilePath::join(dir, suggestedName);

    if (m_generatedIdx >= 0 && m_generatedIdx < (int)m_resources.size()) {
        Resource& r  = m_resources[m_generatedIdx];
        r.text   = text;
        r.path   = path;
        r.name   = ofFilePath::getFileName(path);
        r.loaded = !text.empty();
        updateResourceEntity(r);
        ofBuffer buf;
        buf.set(text);
        ofBufferToFile(path, buf);
        selectResourceIndex(m_generatedIdx);
        if (m_onResourceEntityChanged)
            m_onResourceEntityChanged(r.entity);
        return r.entity;
    }

    Resource r;
    r.type   = ResourceType::GeneratedGCode;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    r.text   = text;
    r.loaded = !text.empty();
    ofFilePath::createEnclosingDirectory(dir, false, true);
    ofBuffer buf;
    buf.set(text);
    ofBufferToFile(path, buf);

    m_resources.push_back(std::move(r));
    m_generatedIdx = (int)m_resources.size() - 1;
    createResourceEntity(m_resources[m_generatedIdx]);
    selectResourceIndex(m_generatedIdx);
    if (m_onResourceEntityChanged)
        m_onResourceEntityChanged(m_resources[m_generatedIdx].entity);
    return m_resources[m_generatedIdx].entity;
}

// ---------------------------------------------------------------------------
// Loaders
// ---------------------------------------------------------------------------

void ResourcesPanel::loadImage(const std::string& path)
{
    Resource r;
    r.type   = ResourceType::Image;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    r.loaded = r.thumbnail.load(path);
    if (r.loaded) {
        const float maxDim = kThumbSize * 2.f;
        float w = (float)r.thumbnail.getWidth();
        float h = (float)r.thumbnail.getHeight();
        if (w > maxDim || h > maxDim) {
            float scale = maxDim / std::max(w, h);
            r.thumbnail.resize((int)(w * scale), (int)(h * scale));
        }
    }
    m_resources.push_back(std::move(r));
    createResourceEntity(m_resources.back());
    selectResourceIndex((int)m_resources.size() - 1);
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

void ResourcesPanel::loadSVG(const std::string& path)
{
    Resource r;
    r.type   = ResourceType::VectorSVG;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    r.loaded = ofFile(path).exists();
    m_resources.push_back(std::move(r));
    createResourceEntity(m_resources.back());
    selectResourceIndex((int)m_resources.size() - 1);
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

void ResourcesPanel::loadDXF(const std::string& path)
{
    Resource r;
    r.type   = ResourceType::VectorDXF;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    r.loaded = ofFile(path).exists();
    m_resources.push_back(std::move(r));
    createResourceEntity(m_resources.back());
    selectResourceIndex((int)m_resources.size() - 1);
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

void ResourcesPanel::loadVideo(const std::string& path)
{
    Resource r;
    r.type   = ResourceType::Video;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    r.loaded = ofFile(path).exists();
    m_resources.push_back(std::move(r));
    createResourceEntity(m_resources.back());
    selectResourceIndex((int)m_resources.size() - 1);
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

void ResourcesPanel::loadGCode(const std::string& path)
{
    Resource r;
    r.type   = ResourceType::GCodeSnippet;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    ofBuffer buf = ofBufferFromFile(path);
    r.text   = buf.getText();
    r.loaded = !r.text.empty();
    m_resources.push_back(std::move(r));
    createResourceEntity(m_resources.back());
    selectResourceIndex((int)m_resources.size() - 1);
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

void ResourcesPanel::loadTextSnippet(const std::string& path)
{
    Resource r;
    r.type   = ResourceType::TextSnippet;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    ofBuffer buf = ofBufferFromFile(path);
    r.text   = buf.getText();
    r.loaded = ofFile(path).exists();
    m_resources.push_back(std::move(r));
    createResourceEntity(m_resources.back());
    selectResourceIndex((int)m_resources.size() - 1);
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

void ResourcesPanel::loadGenericFile(const std::string& path)
{
    Resource r;
    r.type   = ResourceType::GenericFile;
    r.path   = path;
    r.name   = ofFilePath::getFileName(path);
    r.loaded = ofFile(path).exists();
    m_resources.push_back(std::move(r));
    createResourceEntity(m_resources.back());
    selectResourceIndex((int)m_resources.size() - 1);
    if (m_onResourceLoaded)
        m_onResourceLoaded(m_resources.back());
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

void ResourcesPanel::saveSelected(bool saveAs)
{
    if (m_selected < 0 || m_selected >= (int)m_resources.size()) return;
    Resource& r = m_resources[m_selected];
    if (r.type != ResourceType::GCodeSnippet
        && r.type != ResourceType::GeneratedGCode
        && r.type != ResourceType::TextSnippet) return;

    const std::string text = snippetTextFromEntity(r);
    if (text.empty()) return;

    auto writeTo = [this, &r, text](const std::string& path) {
        ofBuffer buf;
        buf.set(text);
        if (!ofBufferToFile(path, buf)) return;
        r.path = path;
        r.name = ofFilePath::getFileName(path);
        r.text = text;
        updateResourceEntity(r);
        if (m_onResourceEntityChanged)
            m_onResourceEntityChanged(r.entity);
    };

    if (!saveAs && !r.path.empty()) {
        writeTo(r.path);
        return;
    }

    const std::string filters = (r.type == ResourceType::TextSnippet)
        ? ".txt,.md,.csv,.json,.xml,.yaml,.yml"
        : ".gcode,.ngc,.nc,.txt";
    const std::string defaultName = r.name.empty()
        ? (r.type == ResourceType::TextSnippet ? "snippet.txt" : "snippet.gcode")
        : r.name;

    runtime().saveFileDialog(
        "resources.save.snippet", "Save file", filters, defaultName,
        [writeTo](const std::string& path) { writeTo(path); });
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

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

void ResourcesPanel::drawMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("Load")) {
        if (ImGui::MenuItem("Image...")) {
            runtime().openFileDialog(
                "resources.load.image", "Load Image",
                ".png,.jpg,.jpeg,.bmp,.tga,.gif,.tif,.tiff",
                [this](const std::string& path) { loadImage(path); });
        }
        if (ImGui::MenuItem("SVG...")) {
            runtime().openFileDialog(
                "resources.load.svg", "Load SVG", ".svg",
                [this](const std::string& path) { loadSVG(path); });
        }
        if (ImGui::MenuItem("DXF...")) {
            runtime().openFileDialog(
                "resources.load.dxf", "Load DXF", ".dxf",
                [this](const std::string& path) { loadDXF(path); });
        }
        if (ImGui::MenuItem("Video...")) {
            runtime().openFileDialog(
                "resources.load.video", "Load Video",
                ".mp4,.mov,.avi,.mkv,.webm,.wmv,.m4v,.flv",
                [this](const std::string& path) { loadVideo(path); });
        }
        if (ImGui::MenuItem("G-code snippet...")) {
            runtime().openFileDialog(
                "resources.load.gcode", "Load G-code",
                ".gcode,.ngc,.nc",
                [this](const std::string& path) { loadGCode(path); });
        }
        if (ImGui::MenuItem("Text snippet...")) {
            runtime().openFileDialog(
                "resources.load.text", "Load Text",
                ".txt,.md,.csv,.json,.xml,.yaml,.yml,.py,.js,.glsl,.vert,.frag",
                [this](const std::string& path) { loadTextSnippet(path); });
        }
        if (ImGui::MenuItem("Any file...")) {
            runtime().openFileDialog(
                "resources.load.any", "Load File", ".*",
                [this](const std::string& path) { dispatchFilePath(path); });
        }
        ImGui::EndMenu();
    }

    const bool canSave = m_selected >= 0 && m_selected < (int)m_resources.size()
        && (m_resources[m_selected].type == ResourceType::GCodeSnippet
            || m_resources[m_selected].type == ResourceType::GeneratedGCode
            || m_resources[m_selected].type == ResourceType::TextSnippet);
    if (!canSave) ImGui::BeginDisabled();
    if (ImGui::MenuItem("Save")) saveSelected(false);
    if (ImGui::MenuItem("Save As...")) saveSelected(true);
    if (!canSave) ImGui::EndDisabled();

    ImGui::EndMenuBar();
}

void ResourcesPanel::drawDropStrip(float height)
{
    ImGui::BeginChild("##res_drop", ImVec2(-1.f, height), true, ImGuiWindowFlags_NoScrollbar);
    const char*  hint  = "Drop files here";
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 tsz   = ImGui::CalcTextSize(hint);
    ImGui::SetCursorPos(ImVec2(std::max(0.f, (avail.x - tsz.x) * 0.5f),
                                 std::max(0.f, (avail.y - tsz.y) * 0.5f)));
    ImGui::TextDisabled("%s", hint);

    const ImVec2 p0 = ImGui::GetWindowPos();
    const ImVec2 p1 = ImVec2(p0.x + ImGui::GetWindowWidth(), p0.y + ImGui::GetWindowHeight());
    drawDashedRect(ImGui::GetWindowDrawList(), p0, p1, IM_COL32(120, 120, 140, 120));

    ImGui::EndChild();
}

void ResourcesPanel::drawResourceRows(int& removeIdx, int& placeIdx)
{
    const entt::entity runtimeSel = runtime().selected();
    m_selected = -1;

    for (int i = 0; i < (int)m_resources.size(); ++i) {
        Resource& r = m_resources[i];
        if (r.type == ResourceType::GeneratedGCode)
            m_generatedIdx = i;

        if (r.entity != entt::null && r.entity == runtimeSel)
            m_selected = i;

        ImGui::PushID(i);

        const bool hovered = ImLed::Draw(
            r.loaded ? ImLed::ColorsOk() : ImLed::ColorsWarn(),
            { .lit = true });
        if (hovered) {
            ImGui::SetTooltip("%s", r.loaded ? "Loaded" : "Not loaded");
        }
        ImGui::SameLine();

        const bool isSelected = (r.entity != entt::null && r.entity == runtimeSel);
        ImGui::SetNextItemAllowOverlap();
        if (ImGui::Selectable(r.name.c_str(), isSelected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            selectResourceIndex(i);
            if (ImGui::IsMouseDoubleClicked(0)) {
                if (r.type == ResourceType::GeneratedGCode)
                    placeIdx = i;
            }
        }

        if (ImGui::BeginPopupContextItem("##res_ctx")) {
            if (r.type == ResourceType::VectorSVG
                || r.type == ResourceType::VectorDXF
                || r.type == ResourceType::Image)
                if (ImGui::MenuItem("Place")) placeIdx = i;
            if (r.type == ResourceType::GeneratedGCode)
                if (ImGui::MenuItem("Open in Code Editor")) placeIdx = i;
            if (r.type == ResourceType::GCodeSnippet
                || r.type == ResourceType::GeneratedGCode
                || r.type == ResourceType::TextSnippet) {
                if (ImGui::MenuItem("Save"))       { m_selected = i; saveSelected(false); }
                if (ImGui::MenuItem("Save As...")) { m_selected = i; saveSelected(true);  }
            }
            if (ImGui::MenuItem("Remove")) removeIdx = i;
            ImGui::EndPopup();
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", r.path.c_str());

        ImGui::PopID();
    }
}

void ResourcesPanel::drawList()
{
    const float totalW    = ImGui::GetContentRegionAvail().x;
    const float stripMinH = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.f;
    const float spacingY  = ImGui::GetStyle().ItemSpacing.y;
    const float bodyH     = ImGui::GetContentRegionAvail().y;

    ImGui::BeginChild("##res_body", ImVec2(totalW, bodyH), false);

    const float maxListH = std::max(0.f, ImGui::GetContentRegionAvail().y - stripMinH - spacingY);
    if (!m_resources.empty() && maxListH > 0.f) {
        ImGui::BeginChild("##res_list", ImVec2(totalW, maxListH), true);
        int removeIdx = -1;
        int placeIdx  = -1;
        drawResourceRows(removeIdx, placeIdx);
        ImGui::EndChild();

        if (placeIdx >= 0 && placeIdx < (int)m_resources.size() && m_onPlace)
            m_onPlace(m_resources[placeIdx]);
        if (removeIdx >= 0 && removeIdx < (int)m_resources.size()) {
            if (removeIdx == m_generatedIdx) m_generatedIdx = -1;
            destroyResourceEntity(m_resources[removeIdx].entity);
            m_resources.erase(m_resources.begin() + removeIdx);
        }
    }

    const float dropH = std::max(stripMinH, ImGui::GetContentRegionAvail().y);
    drawDropStrip(dropH);

    ImGui::EndChild();
}

void ResourcesPanel::placeSelected()
{
    if (m_selected >= 0 && m_selected < (int)m_resources.size() && m_onPlace)
        m_onPlace(m_resources[m_selected]);
}

} // namespace ofkitty
