#include "Runtime_private.h"

#include "ofMain.h"

#include <algorithm>
#include <cctype>

namespace ofkitty::detail {

std::string makeImGuiIdFromLabel(const std::string& label)
{
    std::string id = label;
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) {
        if (std::isalnum(c))
            return static_cast<char>(std::tolower(c));
        return '_';
    });
    while (id.find("__") != std::string::npos)
        id.replace(id.find("__"), 2, "_");
    if (!id.empty() && id.front() == '_')
        id.erase(id.begin());
    if (!id.empty() && id.back() == '_')
        id.pop_back();
    return id.empty() ? "window" : id;
}

void createParentDirectoryIfNeeded(const std::string& path)
{
    const auto parent = of::filesystem::path(path).parent_path();
    if (!parent.empty())
        of::filesystem::create_directories(parent);
}

}
