#pragma once

#include <filesystem>

namespace synodrive {

bool supportedInstalledVersion(const std::filesystem::path& infoPath);
std::filesystem::path overlayDirectory(const std::filesystem::path& currentPath);

}
