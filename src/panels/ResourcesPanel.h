#pragma once

#include "ofMain.h"
#include <entt.hpp>
#include <functional>
#include <string>
#include <vector>

namespace ofkitty {

enum class ResourceType {
    Image,
    VectorSVG,
    VectorDXF,
    Video,
    GCodeSnippet,
    GeneratedGCode,
    TextSnippet,
    GenericFile,
};

struct Resource {
    ResourceType type;
    std::string  path;
    std::string  name;
    bool         loaded {false};
    entt::entity entity {entt::null};
    ofImage      thumbnail;
    std::string  text;
};

class ResourcesPanel {
public:
    void setOnPlace(std::function<void(const Resource&)> cb);
    void setOnResourceSelected(std::function<void(const Resource&)> cb);
    void setOnResourceLoaded(std::function<void(Resource&)> cb);
    void setOnResourceEntityChanged(std::function<void(entt::entity)> cb);

    void addResource(Resource r);

    /// Upsert the single generated-job slot (pipeline output).
    entt::entity addOrUpdateGeneratedJob(const std::string& text,
                                         const std::string& suggestedName = "Generated.gcode");

    /// Call from ofApp::dragEvent — routes each file by extension and adds it.
    void acceptFileDrop(const std::vector<std::filesystem::path>& paths, glm::vec2 dropPos = {});

    const std::vector<Resource>& resources() const { return m_resources; }
    int selectedIndex() const { return m_selected; }

    /// Copy `code_snippet_component.text` from @p entity into the matching Resource row.
    void syncTextFromEntity(entt::entity entity);

    void draw(const char* title, bool& visible);

private:
    void loadImage(const std::string& path);
    void loadSVG(const std::string& path);
    void loadDXF(const std::string& path);
    void loadVideo(const std::string& path);
    void loadGCode(const std::string& path);
    void loadTextSnippet(const std::string& path);
    void loadGenericFile(const std::string& path);

    /// Route a single file path to the correct loader based on extension.
    void dispatchFilePath(const std::string& path);

    entt::entity createResourceEntity(Resource& r);
    void         updateResourceEntity(Resource& r);
    void         destroyResourceEntity(entt::entity e);
    void         selectResourceIndex(int index);
    void         saveSelected(bool saveAs);
    std::string  snippetTextFromEntity(const Resource& r) const;

    void drawMenuBar();
    void drawList();
    void drawDropStrip(float height);
    void drawResourceRows(int& removeIdx, int& placeIdx);
    void placeSelected();

    std::vector<Resource>                  m_resources;
    std::function<void(const Resource&)>   m_onPlace;
    std::function<void(const Resource&)>   m_onResourceSelected;
    std::function<void(Resource&)>         m_onResourceLoaded;
    std::function<void(entt::entity)>      m_onResourceEntityChanged;
    int                                    m_selected    {-1};
    int                                    m_generatedIdx {-1};

    static constexpr float kThumbSize = 48.f;
};

} // namespace ofkitty
