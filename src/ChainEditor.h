#pragma once

#include "ReorderDragDrop.h"
#include "imgui.h"
#include <functional>
#include <string>
#include <vector>

namespace ofkitty {

/// Generic re-orderable chain of collapsing-header rows (see docs/chain-editor.md).
class ChainEditor {
public:
    using DrawStepFn   = std::function<void(int index)>;
    using LabelFn      = std::function<std::string(int index)>;
    using MoveFn       = std::function<void(int from, int toInsert)>;
    using RemoveFn     = std::function<void(int index)>;
    using EnabledFn    = std::function<bool(int index)>;
    using SetEnabledFn = std::function<void(int index, bool on)>;

    void setStepCount(int n) { m_count = n; }
    int  stepCount() const { return m_count; }

    /// Unique drag-drop scope per list (required when multiple editors share a panel).
    void setPayloadTag(const char* tag) { m_payloadTag = tag; }

    void setDrawStepBody(DrawStepFn fn) { m_drawStep = std::move(fn); }
    void setStepLabel(LabelFn fn) { m_label = std::move(fn); }
    void setOnMove(MoveFn fn) { m_onMove = std::move(fn); }
    void setOnRemove(RemoveFn fn) { m_onRemove = std::move(fn); }
    void setIsEnabled(EnabledFn fn) { m_isEnabled = std::move(fn); }
    void setSetEnabled(SetEnabledFn fn) { m_setEnabled = std::move(fn); }

    /// Optional outer section (e.g. "Image filters"). Empty = flat list only.
    void setSectionTitle(std::string title) { m_sectionTitle = std::move(title); }

    /// Optional hint below the list (e.g. "Order matters — top runs first.").
    void setFooterHint(std::string hint) { m_footerHint = std::move(hint); }

    /// Add combo: labels[0] is placeholder; `onAdd` receives index >= 1 when Add is clicked.
    void setAddTypes(std::vector<std::string> types) { m_addTypes = std::move(types); }
    void setOnAdd(std::function<void(int typeIndex)> fn) { m_onAdd = std::move(fn); }

    void setAddButtonLabel(const char* label) { m_addButtonLabel = label; }
    void setShowRemoveButton(bool show) { m_showRemove = show; }

    void draw();

private:
    int                      m_count = 0;
    const char*              m_payloadTag = "OFKIT_CHAIN";
    std::string              m_sectionTitle;
    std::string              m_footerHint;
    std::vector<std::string> m_addTypes;
    int                      m_addIndex = 0;
    const char*              m_addButtonLabel = "Add";
    bool                     m_showRemove = true;

    DrawStepFn    m_drawStep;
    LabelFn       m_label;
    MoveFn        m_onMove;
    RemoveFn      m_onRemove;
    EnabledFn     m_isEnabled;
    SetEnabledFn  m_setEnabled;
    std::function<void(int)> m_onAdd;
};

} // namespace ofkitty
