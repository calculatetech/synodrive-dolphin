#include "synodrive_compatibility.h"

#include <charconv>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::string_view trim(std::string_view value) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
}

}

namespace synodrive {

bool supportedInstalledVersion(const std::filesystem::path& infoPath) {
    std::ifstream input(infoPath);
    if (!input) {
        return false;
    }

    bool haveSection = false;
    bool inVersion = false;
    bool haveVersion = false;
    std::optional<unsigned> major;
    std::string storage;
    while (std::getline(input, storage)) {
        const std::string_view line = trim(storage);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }
        if (line.front() == '[') {
            if (line.back() != ']') {
                return false;
            }
            const std::string_view section = trim(line.substr(1, line.size() - 2));
            if (section.empty() || section.find_first_of("[]") != std::string_view::npos) {
                return false;
            }
            haveSection = true;
            inVersion = section == "Version";
            if (inVersion && haveVersion) {
                return false;
            }
            haveVersion = haveVersion || inVersion;
            continue;
        }

        const std::size_t equals = line.find('=');
        if (!haveSection || equals == std::string_view::npos || trim(line.substr(0, equals)).empty()) {
            return false;
        }
        if (!inVersion || trim(line.substr(0, equals)) != "major_version") {
            continue;
        }
        if (major) {
            return false;
        }

        const std::string_view value = trim(line.substr(equals + 1));
        if (value.empty()) {
            return false;
        }
        unsigned parsed = 0;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} || end != value.data() + value.size()) {
            return false;
        }
        major = parsed;
    }
    return !input.bad() && haveVersion && major == 4;
}

std::filesystem::path overlayDirectory(const std::filesystem::path& currentPath) {
    std::error_code error;
    const std::filesystem::path resolved = std::filesystem::canonical(currentPath, error);
    if (error || resolved.filename() != "15") {
        throw std::runtime_error("unsupported Synology icon-overlay ABI (expected 15)");
    }
    return resolved;
}

}
