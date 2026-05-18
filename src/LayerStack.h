#pragma once

#include "ofMain.h"
#include "imgui.h"
#include <ofxImGuiStyle/src/IconsFontAwesome5.h>

#include <atomic>
#include <cstring>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// ============================================================================
// ofkitty::LayerStack — generic named-layer stack
// ============================================================================
//
// Usage:
//   struct MyLayer : public ofkitty::LayerBase { /* domain fields */ };
//   ofkitty::LayerStack<MyLayer> stack;
//
//   // UI — wire once at startup:
//   ofkitty::LayersPanelWindow<MyLayer> panel;
//   panel.setStack(&stack);
//   panel.setOnLayerChanged([&]{ rebuildStuff(); });
//   panel.setExtraRowFn([&](MyLayer& l, int idx, bool sel){
//       /* right-aligned badge, context menu, etc. */
//   });
//   // Each frame (inside an ofkitty registered window):
//   panel.draw("Layers###my_layers", visible);
// ============================================================================

namespace ofkitty {

// ============================================================================
// LayerBase
// ============================================================================
/// Minimum identity fields every layer carries.  Domain-specific layer types
/// should inherit from this struct (no virtual destructor needed — slicing is
/// prevented by LayerStack<T> holding concrete T objects).
struct LayerBase {
    std::string name;
    bool        visible     = true;
    bool        locked      = false;
    ofColor     color;

    /// Index into the owning LayerStack::layers[] of this layer's parent.
    /// -1 means the layer is a root (no parent).
    ///
    /// Invariant: if parentIndex >= 0, layers[parentIndex] must exist.
    /// Callers must remap or clear this field when layers are removed or
    /// reordered (LayerStack::remove() does NOT yet cascade — intentional
    /// so that tree management can be delegated to the application).
    int parentIndex = -1;
};

// ============================================================================
// LayerStack<T>
// ============================================================================
/// A flat, reorderable stack of layers where T inherits LayerBase.
/// Keeps at least one layer alive at all times.
template<typename T>
class LayerStack {
    static_assert(std::is_base_of<LayerBase, T>::value,
                  "LayerStack<T>: T must inherit from ofkitty::LayerBase");
public:
    std::vector<T> layers;
    int            currentLayer = 0;

    LayerStack() { addLayer("Layer 1"); }

    // ---- Active layer access -----------------------------------------------
    T& getActive() {
        if (layers.empty()) addLayer("Layer 1");
        currentLayer = std::clamp(currentLayer, 0, (int)layers.size() - 1);
        return layers[currentLayer];
    }
    const T& getActive() const {
        return layers[std::clamp(currentLayer, 0, std::max(0, (int)layers.size() - 1))];
    }

    // ---- Mutations ---------------------------------------------------------
    /// Add a new default-constructed layer with the given name.
    /// Returns the index of the new layer.
    int addLayer(const std::string& name = "") {
        T layer;
        layer.name = name.empty()
            ? ("Layer " + std::to_string(layers.size() + 1))
            : name;
        layers.push_back(std::move(layer));
        return (int)layers.size() - 1;
    }

    /// Remove layer at idx, keeping at least one layer.
    void removeLayer(int idx) {
        if (idx < 0 || idx >= (int)layers.size()) return;
        if (layers.size() <= 1) return;
        layers.erase(layers.begin() + idx);
        currentLayer = std::clamp(currentLayer, 0, (int)layers.size() - 1);
    }

    void moveUp(int idx) {
        if (idx <= 0 || idx >= (int)layers.size()) return;
        std::swap(layers[idx], layers[idx - 1]);
        if (currentLayer == idx)       --currentLayer;
        else if (currentLayer == idx - 1) ++currentLayer;
    }

    void moveDown(int idx) {
        if (idx < 0 || idx >= (int)layers.size() - 1) return;
        std::swap(layers[idx], layers[idx + 1]);
        if (currentLayer == idx)       ++currentLayer;
        else if (currentLayer == idx + 1) --currentLayer;
    }

    int size() const { return (int)layers.size(); }
};

// ============================================================================
// LayersPanelWindow<T>
// ============================================================================
/// Reusable ImGui layers panel for any LayerStack<T>.
///
/// Generic row content (eye / lock / colour swatch / inline-rename name) is
/// drawn automatically.  Pass a ExtraRowFn to add domain-specific widgets to
/// the right side of each row (badges, context-menu items, etc.).
template<typename T>
class LayersPanelWindow {
public:
    using OnChangedCb = std::function<void()>;
    using ExtraRowFn  = std::function<void(T& layer, int idx, bool selected)>;

    void setStack(LayerStack<T>* stack)                    { stack_ = stack; }
    void setOnLayerChanged(OnChangedCb cb)                 { onChanged_ = std::move(cb); }
    void setIsGenerating(const std::atomic<bool>* flag)    { generating_ = flag; }

    /// Called once per row after the name label / rename field, before PopID.
    /// Use it to render a right-aligned badge or a BeginPopupContextItem menu.
    void setExtraRowFn(ExtraRowFn fn)                      { extraRowFn_ = std::move(fn); }

    // ---- Draw --------------------------------------------------------------
    void draw(const char* imguiTitle, bool& visible)
    {
        if (!visible) return;

        if (!stack_) {
            if (ImGui::Begin(imguiTitle, &visible)) {
                ImGui::TextDisabled("No layer stack connected.");
            }
            ImGui::End();
            return;
        }

        if (!ImGui::Begin(imguiTitle, &visible)) { ImGui::End(); return; }

        const bool isGenerating = generating_ && generating_->load();
        auto& layers = stack_->layers;
        int&  cur    = stack_->currentLayer;
        const int n  = (int)layers.size();

        // ---- Toolbar -------------------------------------------------------
        if (ImGui::Button(ICON_FA_PLUS "##add")) {
            cur = stack_->addLayer();
            if (onChanged_) onChanged_();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add layer");
        ImGui::SameLine();

        {
            bool canRemove = n > 1 && !isGenerating;
            if (!canRemove) ImGui::BeginDisabled();
            if (ImGui::Button(ICON_FA_TRASH_ALT "##remove")) {
                stack_->removeLayer(cur);
                if (onChanged_) onChanged_();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Remove layer");
            if (!canRemove) ImGui::EndDisabled();
        }
        ImGui::SameLine();

        {
            bool canUp = cur > 0 && !isGenerating;
            if (!canUp) ImGui::BeginDisabled();
            if (ImGui::Button(ICON_FA_ARROW_UP "##up")) {
                stack_->moveUp(cur);
                if (onChanged_) onChanged_();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Move up");
            if (!canUp) ImGui::EndDisabled();
        }
        ImGui::SameLine();

        {
            bool canDown = cur < n - 1 && !isGenerating;
            if (!canDown) ImGui::BeginDisabled();
            if (ImGui::Button(ICON_FA_ARROW_DOWN "##down")) {
                stack_->moveDown(cur);
                if (onChanged_) onChanged_();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Move down");
            if (!canDown) ImGui::EndDisabled();
        }

        ImGui::Separator();

        // ---- Layer list ----------------------------------------------------
        // Snapshot totalW before the loop; reflects scrollbar-adjusted content width.
        const float totalW = ImGui::GetContentRegionAvail().x;

        for (int i = 0; i < (int)layers.size(); ++i) {
            T& layer = layers[i];
            ImGui::PushID(i);

            // Selection highlight — drawn before widgets so it sits behind them.
            // Use GetFrameHeight() (no spacing) so it doesn't bleed into the gap
            // between rows.
            bool selected = (i == cur);
            if (selected) {
                ImVec2 rMin = ImGui::GetCursorScreenPos();
                ImVec2 rMax(rMin.x + totalW,
                            rMin.y + ImGui::GetFrameHeight());
                ImGui::GetWindowDrawList()->AddRectFilled(
                    rMin, rMax, ImGui::GetColorU32(ImGuiCol_Header));
            }

            // Eye / Lock icon buttons — push tighter FramePadding so the icons
            // stay compact in the row. eyeW / lockW are measured inside the push
            // so they agree with the actual rendered button size.
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.f, 1.f));
            const float eyeW  = ImGui::GetFrameHeight();
            const float lockW = ImGui::GetFrameHeight();

            // Eye toggle
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
                const char* eyeIcon = layer.visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
                if (ImGui::Button(eyeIcon, ImVec2(eyeW, 0))) {
                    layer.visible = !layer.visible;
                    if (onChanged_) onChanged_();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(layer.visible ? "Hide layer" : "Show layer");
                ImGui::PopStyleColor(2);
            }
            ImGui::SameLine();

            // Lock toggle
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
                const char* lockIcon = layer.locked ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
                if (ImGui::Button(lockIcon, ImVec2(lockW, 0)))
                    layer.locked = !layer.locked;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(layer.locked ? "Unlock layer" : "Lock layer");
                ImGui::PopStyleColor(2);
            }
            ImGui::SameLine();

            ImGui::PopStyleVar(); // restore FramePadding before swatch + selectable

            // Colour swatch
            {
                char swId[16];
                std::snprintf(swId, sizeof(swId), "##sw%d", i);
                if (colourSwatch(swId, layer.color))
                    ImGui::OpenPopup("##colpop");
                if (ImGui::BeginPopup("##colpop")) {
                    float c[3] = { layer.color.r / 255.f,
                                   layer.color.g / 255.f,
                                   layer.color.b / 255.f };
                    if (ImGui::ColorPicker3("##cp", c)) {
                        layer.color.r = static_cast<unsigned char>(c[0] * 255.f);
                        layer.color.g = static_cast<unsigned char>(c[1] * 255.f);
                        layer.color.b = static_cast<unsigned char>(c[2] * 255.f);
                    }
                    ImGui::EndPopup();
                }
            }
            ImGui::SameLine();

            // Selectable (click = select).  Width accounts for three SameLine()
            // spacings (after eye, lock, swatch) plus both icon button widths and
            // the 16px swatch button.
            const float selectW = totalW - eyeW - lockW
                                  - 16.f
                                  - 3.f * ImGui::GetStyle().ItemSpacing.x;
            if (ImGui::Selectable("##sel", selected,
                    ImGuiSelectableFlags_AllowOverlap,
                    ImVec2(selectW, 0))) {
                cur = i;
            }
            ImGui::SameLine(0, 0);

            // Inline rename (double-click on name label to activate)
            if (renamingIdx_ == i) {
                ImGui::SetNextItemWidth(selectW);
                if (ImGui::InputText("##rename", renameBuf_, sizeof(renameBuf_),
                        ImGuiInputTextFlags_EnterReturnsTrue
                        | ImGuiInputTextFlags_AutoSelectAll)) {
                    layer.name = renameBuf_;
                    renamingIdx_ = -1;
                }
                if (!ImGui::IsItemActive() && !ImGui::IsItemFocused())
                    renamingIdx_ = -1;
            } else {
                if (!layer.visible)
                    ImGui::PushStyleColor(ImGuiCol_Text,
                        ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::TextUnformatted(layer.name.c_str());
                if (!layer.visible) ImGui::PopStyleColor();

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Double-click to rename");
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        renamingIdx_ = i;
                        std::strncpy(renameBuf_, layer.name.c_str(),
                                     sizeof(renameBuf_) - 1);
                        renameBuf_[sizeof(renameBuf_) - 1] = '\0';
                        ImGui::SetKeyboardFocusHere(-1);
                    }
                }
            }

            // Domain-specific extras (badge, context menu, etc.)
            if (extraRowFn_)
                extraRowFn_(layer, i, selected);

            ImGui::PopID();
        }

        ImGui::End();
    }

private:
    LayerStack<T>*           stack_      = nullptr;
    OnChangedCb              onChanged_;
    const std::atomic<bool>* generating_ = nullptr;
    ExtraRowFn               extraRowFn_;
    int                      renamingIdx_ = -1;
    char                     renameBuf_[128] {};

    static bool colourSwatch(const char* id, ofColor& col) {
        ImVec4 c(col.r / 255.f, col.g / 255.f, col.b / 255.f, 1.f);
        ImGui::PushStyleColor(ImGuiCol_Button, c);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(c.x * 1.15f, c.y * 1.15f, c.z * 1.15f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, c);
        bool clicked = ImGui::Button(id, ImVec2(16, 16));
        ImGui::PopStyleColor(3);
        return clicked;
    }
};

} // namespace ofkitty
