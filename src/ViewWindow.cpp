#include "ViewWindow.h"
#include "Runtime_private.h"

namespace ofkitty {

Runtime::RuntimeWindow makeViewWindow(
    std::string name,
    std::function<void(bool& visible)> draw,
    ViewWindowOpts opts)
{
    Runtime::RuntimeWindow w;
    w.name         = std::move(name);
    w.menuGroup    = std::move(opts.menuGroup);
    w.visible      = opts.visible;
    w.editModeOnly = opts.editModeOnly;
    w.draw         = std::move(draw);
    w.id           = std::move(opts.id);
    return w;
}

}  // namespace ofkitty
