#pragma once

#include "ofMain.h"
#include <functional>
#include <string>
#include <vector>

namespace ofkitty {

// =============================================================================
// ResourceType — file categories handled by the Resources panel.
// =============================================================================
enum class ResourceType {
    Image,         ///< raster image (png, jpg, bmp, tga, …)
    VectorSVG,     ///< SVG vector file
    GCodeSnippet,  ///< G-code text file (ngc, gcode, txt, …)
};

// =============================================================================
// Resource — one entry in the panel.
// =============================================================================
struct Resource {
    ResourceType type;
    std::string  path;          ///< absolute path on disk
    std::string  name;          ///< filename only, for display
    bool         loaded {false}; ///< true after the file was successfully read

    // Image type: small thumbnail for visual preview.
    ofImage      thumbnail;

    // GCodeSnippet type: the text content loaded from the file.
    std::string  text;
};

// =============================================================================
// ResourcesPanel
// =============================================================================
// A standalone ImGui panel that manages a list of file-based resources
// (images, SVGs, G-code snippets).  Apps instantiate one, call
// setOnPlace(), register a window with the ofxKit runtime, and the
// panel handles all file-picking and list rendering internally.
//
// Minimal setup in ofApp::setup():
//
//   m_resources.setOnPlace([this](const ofkitty::Resource& r) {
//       if (r.type == ofkitty::ResourceType::VectorSVG)
//           m_engine.loadVectorSVG(r.path);
//       else if (r.type == ofkitty::ResourceType::Image)
//           m_engine.loadImage(r.path);
//   });
//   ofkitty::runtime().registerWindow({
//       "Resources", "View", true, true,
//       [this](bool& v){ m_resources.draw("Resources###myapp.resources", v); }
//   });
// =============================================================================
class ResourcesPanel {
public:
    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /// Called when the user double-clicks or presses the "Place" button.
    /// The callback owns what "placing" means (load into engine, open editor,
    /// etc.). It is allowed to call any engine/runtime methods — it runs on
    /// the main thread outside the ImGui rendering pass.
    void setOnPlace(std::function<void(const Resource&)> cb);

    // -------------------------------------------------------------------------
    // Resource management
    // -------------------------------------------------------------------------

    /// Add a resource programmatically (e.g. a default snippet bundled with
    /// the app). For file-backed resources the panel auto-loads on add.
    void addResource(Resource r);

    const std::vector<Resource>& resources() const { return m_resources; }

    // -------------------------------------------------------------------------
    // Draw — call this inside a registered RuntimeWindow callback.
    // -------------------------------------------------------------------------
    void draw(const char* title, bool& visible);

private:
    // ---- internal file loading helpers ----
    void loadImage(const std::string& path);
    void loadSVG(const std::string& path);
    void loadGCode(const std::string& path);

    // ---- drawing helpers ----
    void drawMenuBar();
    void drawList();
    void placeSelected();

    std::vector<Resource>                  m_resources;
    std::function<void(const Resource&)>   m_onPlace;
    int                                    m_selected    {-1};

    // Thumbnail texture size drawn in the list.
    static constexpr float kThumbSize = 48.f;
};

} // namespace ofkitty
