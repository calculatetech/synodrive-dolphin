#include <dlfcn.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

constexpr std::string_view kClientInfo = "/opt/Synology/SynologyDrive/INFO";
constexpr std::size_t kMaxFrame = 1024 * 1024;
constexpr std::array<std::string_view, 6> kStatusNames = {
    "unknown", "synced", "syncing", "unsupported", "read-only", "no-permission"};

struct IconOverlayInfo {
    int enable;
    int file_status;
};

class SharedObject {
public:
    explicit SharedObject(const char* path) : handle_(dlopen(path, RTLD_LAZY | RTLD_LOCAL)) {
        if (!handle_) {
            throw std::runtime_error(dlerror());
        }
    }

    ~SharedObject() {
        if (handle_) {
            dlclose(handle_);
        }
    }

    SharedObject(const SharedObject&) = delete;
    SharedObject& operator=(const SharedObject&) = delete;

    template <typename T>
    T symbol(const char* name) const {
        dlerror();
        void* address = dlsym(handle_, name);
        if (const char* error = dlerror()) {
            throw std::runtime_error(error);
        }
        return reinterpret_cast<T>(address);
    }

private:
    void* handle_;
};

std::string environment(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}

std::string_view trim(std::string_view value) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
}

bool supportedInstalledVersion() {
    fs::path path(kClientInfo);
#ifdef SYNODRIVE_STATUS_TESTING
    if (std::string value = environment("SYNODRIVE_STATUS_TEST_INFO"); !value.empty()) {
        path = value;
    }
#endif

    std::ifstream input(path);
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

fs::path overlayDirectory() {
    const std::string home = environment("HOME");
    if (home.empty()) {
        throw std::runtime_error("HOME is not set");
    }

    const fs::path current = fs::path(home) / ".SynologyDrive/SynologyDrive.app/icon-overlay/current";
    std::error_code error;
    const fs::path resolved = fs::canonical(current, error);
    if (error || resolved.filename() != "15") {
        throw std::runtime_error("unsupported Synology icon-overlay ABI (expected 15)");
    }
    return resolved;
}

std::string query(const std::string& path) {
    if (!fs::path(path).is_absolute()) {
        throw std::runtime_error("path must be absolute");
    }
    if (!supportedInstalledVersion()) {
        throw std::runtime_error("unsupported or malformed Synology Drive metadata (expected internal major 4)");
    }

#ifdef SYNODRIVE_STATUS_TESTING
    const std::string testNautilus = environment("SYNODRIVE_STATUS_TEST_NAUTILUS");
    SharedObject nautilusRuntime(
        testNautilus.empty() ? "libnautilus-extension.so.4" : testNautilus.c_str());
#else
    SharedObject nautilusRuntime("libnautilus-extension.so.4");
#endif
    SharedObject helper((overlayDirectory() / "lib/plugin-cb-4.so").c_str());

    using Prepare = void (*)();
    using GetInfo = int (*)(const char*, IconOverlayInfo&);
    const auto prepare = helper.symbol<Prepare>("_Z17PrepareCacheTablev");
    const auto getInfo = helper.symbol<GetInfo>(
        "_Z24GetIconOverlayInfoByPathPKcR21_tag_ICONOVERLAY_INFO");

    IconOverlayInfo info{0, 0};
    try {
        prepare();
        if (getInfo(path.c_str(), info) != 0) {
            throw std::runtime_error("Synology status query failed");
        }
    } catch (...) {
        throw std::runtime_error("Synology helper threw an exception");
    }

    if (info.file_status < 0 || static_cast<std::size_t>(info.file_status) >= kStatusNames.size()) {
        throw std::runtime_error("Synology helper returned an unknown status");
    }
    return std::string(kStatusNames[info.file_status]);
}

void writeFrame(std::string_view value) {
    std::cout.write(value.data(), static_cast<std::streamsize>(value.size()));
    std::cout.put('\0');
    std::cout.flush();
}

std::string isolatedQuery(const std::string& path) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return "error";
    }

    const pid_t parent = getpid();
    const pid_t child = fork();
    if (child == 0) {
        if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || getppid() != parent) {
            _exit(1);
        }
        close(pipefd[0]);
        std::string result = "error";
        try {
            result = query(path);
        } catch (...) {
        }
        std::size_t written = 0;
        while (written < result.size()) {
            const ssize_t count = write(pipefd[1], result.data() + written, result.size() - written);
            if (count > 0) {
                written += static_cast<std::size_t>(count);
            } else if (errno != EINTR) {
                break;
            }
        }
        close(pipefd[1]);
        _exit(result == "error" ? 1 : 0);
    }
    close(pipefd[1]);
    if (child < 0) {
        close(pipefd[0]);
        return "error";
    }

    pollfd descriptor{pipefd[0], POLLIN, 0};
    std::string result;
    if (poll(&descriptor, 1, 10000) > 0) {
        std::array<char, 32> buffer{};
        const ssize_t count = read(pipefd[0], buffer.data(), buffer.size());
        if (count > 0) {
            result.assign(buffer.data(), static_cast<std::size_t>(count));
        }
    } else {
        kill(child, SIGKILL);
    }
    close(pipefd[0]);

    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    for (const std::string_view name : kStatusNames) {
        if (result == name) {
            return result;
        }
    }
    return "error";
}

int streamMode() {
#ifdef SYNODRIVE_STATUS_TESTING
    if (const std::string log = environment("SYNODRIVE_STATUS_TEST_WRAPPER_PID_LOG"); !log.empty()) {
        std::ofstream(log, std::ios::app) << getpid() << '\n';
    }
#endif
    std::string frame;
    bool oversized = false;
    char byte = 0;
    while (std::cin.get(byte)) {
        if (byte != '\0') {
            if (!oversized && frame.size() < kMaxFrame) {
                frame.push_back(byte);
            } else {
                oversized = true;
            }
            continue;
        }

        if (oversized || frame.empty()) {
            writeFrame("error");
        } else {
            writeFrame(isolatedQuery(frame));
        }
        frame.clear();
        oversized = false;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--stdio") {
        return streamMode();
    }
    if (argc != 2 || !fs::path(argv[1]).is_absolute()) {
        std::cerr << "usage: synodrive-status <absolute-path>\n";
        return 2;
    }

    try {
        std::cout << query(argv[1]) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "synodrive-status: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "synodrive-status: unexpected helper failure\n";
        return 1;
    }
}
