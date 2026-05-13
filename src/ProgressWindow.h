#pragma once

#include <functional>
#include <mutex>
#include <string>

// Forward-declare ImGui types so callers don't need to include imgui.h.
struct ImVec2;

namespace ofkitty {

// ============================================================================
// ProgressWindow — tickable progress bar overlay
// ============================================================================
// A non-modal, non-intrusive progress panel drawn via the ofxKit Runtime
// overlay.  Optionally anchored to the bottom of the screen so it doesn't
// occlude the canvas.
//
// Supports two usage patterns:
//
//   // Step-based (total known up front):
//   ofkitty::progress().begin("Exporting GCode", 500);
//   for (auto& layer : layers) {
//       ofkitty::progress().tick("Layer " + ofToString(layer.index));
//       // ...
//   }
//   ofkitty::progress().finish();
//
//   // Absolute progress (0.0–1.0):
//   ofkitty::progress().begin("Rendering");
//   ofkitty::progress().tick("Frame 42", 0.84f);
//   ofkitty::progress().finish("Complete");
//
// Thread safety: begin / tick / finish / hide may be called from any thread.
// ============================================================================

class ProgressWindow {
public:
    // -------------------------------------------------------------------------
    // Singleton access
    // -------------------------------------------------------------------------
    static ProgressWindow& instance();

    ProgressWindow(const ProgressWindow&)            = delete;
    ProgressWindow& operator=(const ProgressWindow&) = delete;

    // -------------------------------------------------------------------------
    // Configuration — may be set before or after begin()
    // -------------------------------------------------------------------------

    /// Anchor the panel to the bottom edge of the viewport (default: true).
    /// When true the panel is full-width, no title bar, immovable.
    /// When false it floats as a regular ImGui window.
    void setBottomAnchored(bool anchored);
    bool isBottomAnchored() const;

    /// Seconds to keep the panel visible after finish() before auto-hiding.
    /// 0 = hide immediately on finish(). Default: 2.0.
    void setAutoHideDelay(float seconds);
    float autoHideDelay() const;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// Start a new operation with a known number of steps.
    /// Subsequent tick(label) calls advance by 1/totalSteps each time.
    void begin(std::string title, int totalSteps);

    /// Start a new operation with open-ended progress.
    /// Use tick(label, progress) to supply absolute 0.0–1.0 values.
    void begin(std::string title);

    /// Advance by one step (only valid after begin(title, totalSteps)).
    /// The panel is shown automatically.
    void tick(std::string label);

    /// Set absolute progress (0.0–1.0) and status label.
    /// The panel is shown automatically.
    void tick(std::string label, float progress);

    /// Show an animated marquee bar with no numeric progress.
    /// Equivalent to tick(label, -1.0f). Use when the total duration is
    /// unknown (e.g. waiting for a network response).
    void tickIndeterminate(std::string label);

    /// Mark the operation as complete.
    /// The panel shows doneLabel for autoHideDelay seconds then hides itself.
    void finish(std::string doneLabel = "Done");

    /// Immediately hide the panel (e.g. on cancel or error).
    void hide();

    // -------------------------------------------------------------------------
    // State queries
    // -------------------------------------------------------------------------

    /// True while between begin() and finish() / hide().
    bool isActive() const;

    float       progress() const;
    std::string label()    const;
    std::string title()    const;

    // -------------------------------------------------------------------------
    // Runtime integration
    // -------------------------------------------------------------------------

    /// Register as a RuntimeWindow with the active Runtime singleton.
    /// Called automatically on the first begin(); safe to call multiple times.
    void registerWithRuntime();

private:
    ProgressWindow() = default;

    /// Called from the Runtime draw loop on the GL / main thread.
    void draw(bool& visible);

    // ------------------------------------------------------------------
    // Mutable state — protected by m_mutex for cross-thread access
    // ------------------------------------------------------------------
    mutable std::mutex m_mutex;

    std::string  m_title;
    std::string  m_label;
    float        m_progress     {0.f};
    int          m_totalSteps   {0};
    int          m_currentStep  {0};
    bool         m_active       {false};
    bool         m_finished     {false};
    double       m_finishTime   {-1.0}; ///< ofGetElapsedTimef() when finish() was called

    // ------------------------------------------------------------------
    // Configuration — read only from GL thread after setup, so no lock needed
    // ------------------------------------------------------------------
    bool  m_bottomAnchored  {true};
    float m_autoHideDelay   {2.0f};

    // ------------------------------------------------------------------
    // Runtime bookkeeping — main thread only
    // ------------------------------------------------------------------
    bool m_registered {false};
};

// Convenience free function — mirrors ofkitty::runtime().
inline ProgressWindow& progress() { return ProgressWindow::instance(); }

} // namespace ofkitty
