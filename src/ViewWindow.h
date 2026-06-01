#pragma once

#include "Runtime.h"
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace ofkitty {

/// Optional overrides for registerViewWindow — defaults match a normal View panel.
struct ViewWindowOpts {
    std::string menuGroup{"View"};
    bool        visible{true};
    bool        editModeOnly{true};
    std::string id;  ///< empty → Runtime assigns "ofxkit.window." + slug(name)
};

/// Build a RuntimeWindow from named options (avoids positional true/true).
Runtime::RuntimeWindow makeViewWindow(
    std::string name,
    std::function<void(bool& visible)> draw,
    ViewWindowOpts opts = {});

/// Optional virtual base for apps that prefer override clarity (not for panel addons).
class ViewWindow {
public:
    virtual ~ViewWindow() = default;
    virtual std::string name() const = 0;
    virtual void draw(bool& visible) = 0;
    virtual ViewWindowOpts options() const { return {}; }
};

namespace detail {

template<typename W, typename = void>
struct ViewWindowHasOptions : std::false_type {};

template<typename W>
struct ViewWindowHasOptions<
    W,
    std::void_t<decltype(std::declval<const W&>().options())>> : std::true_type {};

template<typename W>
ViewWindowOpts viewWindowOptsFrom(const W& w)
{
    if constexpr (ViewWindowHasOptions<W>::value)
        return w.options();
    else
        return {};
}

}  // namespace detail

/// Register a window object owned by the app (member struct). Lifetime must outlive Runtime.
template<typename W>
Runtime::RuntimeWindow* registerViewWindow(W& window)
{
    return Runtime::instance().registerWindow(makeViewWindow(
        window.name(),
        [&window](bool& visible) { window.draw(visible); },
        detail::viewWindowOptsFrom(window)));
}

}  // namespace ofkitty
