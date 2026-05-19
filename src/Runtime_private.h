#pragma once

#include <string>

namespace ofkitty::detail {

std::string makeImGuiIdFromLabel(const std::string& label);
void createParentDirectoryIfNeeded(const std::string& path);

}
