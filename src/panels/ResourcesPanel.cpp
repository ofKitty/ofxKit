#include "ResourcesPanel.h"

#include "../Runtime.h"



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

        case ResourceType::VectorSVG:     return "[SVG]";

        case ResourceType::GeneratedGCode: return "[JOB]";

        default:                          return "[GCO]";

    }

}



} // namespace



entt::entity ResourcesPanel::createResourceEntity(Resource& r)

{

    auto& reg = resourceRegistry();

    auto  e   = reg.create();



    reg.emplace<ecs::tag_component>(e, r.name);

    reg.emplace<ecs::filepath_component>(e, std::filesystem::path(r.path));

    reg.emplace<ecs::selectable_component>(e, false);



    if (r.type == ResourceType::Image && r.loaded)

        reg.emplace<ecs::image_component>(e, std::filesystem::path(r.path));



    if (r.type == ResourceType::GCodeSnippet || r.type == ResourceType::GeneratedGCode) {

        ecs::code_snippet_component snip(r.text,

            r.type == ResourceType::GeneratedGCode

                ? ecs::code_language::Gcode

                : ecs::code_language::Gcode);

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

        reg.get<ecs::filepath_component>(r.entity).path = std::filesystem::path(r.path);



    if (r.type == ResourceType::GCodeSnippet || r.type == ResourceType::GeneratedGCode) {

        if (!reg.all_of<ecs::code_snippet_component>(r.entity))

            reg.emplace<ecs::code_snippet_component>(r.entity);

        auto& snip = reg.get<ecs::code_snippet_component>(r.entity);

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



void ResourcesPanel::setOnPlace(std::function<void(const Resource&)> cb)

{

    m_onPlace = std::move(cb);

}



void ResourcesPanel::setOnResourceLoaded(std::function<void(Resource&)> cb)

{

    m_onResourceLoaded = std::move(cb);

}



void ResourcesPanel::setOnResourceEntityChanged(std::function<void(entt::entity)> cb)

{

    m_onResourceEntityChanged = std::move(cb);

}



void ResourcesPanel::addResource(Resource r)

{

    if (!r.loaded) {

        switch (r.type) {

            case ResourceType::Image:          loadImage(r.path);         return;

            case ResourceType::VectorSVG:      loadSVG(r.path);           return;

            case ResourceType::GCodeSnippet:     loadGCode(r.path);         return;

            case ResourceType::GeneratedGCode:   break;

        }

    }

    m_resources.push_back(std::move(r));

    createResourceEntity(m_resources.back());

    selectResourceIndex((int)m_resources.size() - 1);

    if (m_onResourceLoaded)

        m_onResourceLoaded(m_resources.back());

}



entt::entity ResourcesPanel::addOrUpdateGeneratedJob(const std::string& text,

                                                     const std::string& suggestedName)

{

    const std::string dir  = ofToDataPath("generated/", true);

    const std::string path = ofFilePath::join(dir, suggestedName);



    if (m_generatedIdx >= 0 && m_generatedIdx < (int)m_resources.size()) {

        Resource& r = m_resources[m_generatedIdx];

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

    createResourceEntity(m_resources.back());

    selectResourceIndex((int)m_resources.size() - 1);

    if (m_onResourceLoaded)

        m_onResourceLoaded(m_resources.back());

}



void ResourcesPanel::saveSelected(bool saveAs)

{

    if (m_selected < 0 || m_selected >= (int)m_resources.size()) return;

    Resource& r = m_resources[m_selected];

    if (r.type != ResourceType::GCodeSnippet && r.type != ResourceType::GeneratedGCode) return;



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



    runtime().saveFileDialog(

        "resources.save.gcode", "Save G-code", ".gcode,.ngc,.nc,.txt",

        r.name.empty() ? "snippet.gcode" : r.name,

        [writeTo](const std::string& path) { writeTo(path); });

}



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

        if (ImGui::MenuItem("G-code snippet...")) {

            runtime().openFileDialog(

                "resources.load.gcode", "Load G-code",

                ".gcode,.ngc,.nc,.txt",

                [this](const std::string& path) { loadGCode(path); });

        }

        ImGui::EndMenu();

    }



    const bool canSave = m_selected >= 0 && m_selected < (int)m_resources.size()

        && (m_resources[m_selected].type == ResourceType::GCodeSnippet

            || m_resources[m_selected].type == ResourceType::GeneratedGCode);

    if (!canSave) ImGui::BeginDisabled();

    if (ImGui::MenuItem("Save")) saveSelected(false);

    if (ImGui::MenuItem("Save As...")) saveSelected(true);

    if (!canSave) ImGui::EndDisabled();



    ImGui::EndMenuBar();

}



void ResourcesPanel::drawList()

{

    const float totalW = ImGui::GetContentRegionAvail().x;

    const float btnH     = ImGui::GetFrameHeight();

    const float listH    = ImGui::GetContentRegionAvail().y - btnH - ImGui::GetStyle().ItemSpacing.y;



    ImGui::BeginChild("##res_list", ImVec2(totalW, listH), false);



    int removeIdx = -1;

    int placeIdx  = -1;



    const entt::entity runtimeSel = runtime().selected();

    m_selected = -1;



    for (int i = 0; i < (int)m_resources.size(); ++i) {

        Resource& r = m_resources[i];

        if (r.type == ResourceType::GeneratedGCode)

            m_generatedIdx = i;



        if (r.entity != entt::null && r.entity == runtimeSel)

            m_selected = i;



        ImGui::PushID(i);



        ImVec4 dotCol = r.loaded

            ? ImVec4(0.2f, 0.85f, 0.2f, 1.f)

            : ImVec4(0.85f, 0.2f, 0.2f, 1.f);

        ImGui::TextColored(dotCol, r.loaded ? "●" : "●");

        ImGui::SameLine();



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

            const char* label = resourceTypeLabel(r.type);

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



        const bool isSelected = (r.entity != entt::null && r.entity == runtimeSel);

        ImGui::SetNextItemAllowOverlap();

        if (ImGui::Selectable(r.name.c_str(), isSelected,

                              ImGuiSelectableFlags_AllowDoubleClick,

                              ImVec2(0, kThumbSize))) {

            selectResourceIndex(i);

            if (ImGui::IsMouseDoubleClicked(0)) {

                if (r.type == ResourceType::GeneratedGCode)

                    placeIdx = i;

            }

        }



        if (ImGui::BeginPopupContextItem("##res_ctx")) {

            if (r.type == ResourceType::VectorSVG || r.type == ResourceType::Image)

                if (ImGui::MenuItem("Place")) placeIdx = i;

            if (r.type == ResourceType::GeneratedGCode)

                if (ImGui::MenuItem("Open in Code Editor")) placeIdx = i;

            if (ImGui::MenuItem("Save")) { m_selected = i; saveSelected(false); }

            if (ImGui::MenuItem("Save As...")) { m_selected = i; saveSelected(true); }

            if (ImGui::MenuItem("Remove")) removeIdx = i;

            ImGui::EndPopup();

        }



        if (ImGui::IsItemHovered())

            ImGui::SetTooltip("%s", r.path.c_str());



        ImGui::PopID();

    }



    ImGui::EndChild();



    bool canPlace = (m_selected >= 0 && m_selected < (int)m_resources.size()

                     && m_resources[m_selected].loaded

                     && (m_resources[m_selected].type == ResourceType::Image

                         || m_resources[m_selected].type == ResourceType::VectorSVG

                         || m_resources[m_selected].type == ResourceType::GeneratedGCode));

    if (!canPlace) ImGui::BeginDisabled();

    if (ImGui::Button("Place##res", ImVec2(-1.f, 0.f)))

        placeIdx = m_selected;

    if (!canPlace) ImGui::EndDisabled();



    if (placeIdx >= 0 && placeIdx < (int)m_resources.size() && m_onPlace)

        m_onPlace(m_resources[placeIdx]);



    if (removeIdx >= 0 && removeIdx < (int)m_resources.size()) {

        if (removeIdx == m_generatedIdx) m_generatedIdx = -1;

        destroyResourceEntity(m_resources[removeIdx].entity);

        m_resources.erase(m_resources.begin() + removeIdx);

    }

}



void ResourcesPanel::placeSelected()

{

    if (m_selected >= 0 && m_selected < (int)m_resources.size() && m_onPlace)

        m_onPlace(m_resources[m_selected]);

}



} // namespace ofkitty

