#include "synodrive_compatibility.h"

#include <dlfcn.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

fs::path clientInfoPath() {
    fs::path path(kClientInfo);
#ifdef SYNODRIVE_STATUS_TESTING
    if (std::string value = environment("SYNODRIVE_STATUS_TEST_INFO"); !value.empty()) {
        path = value;
    }
#endif
    return path;
}

fs::path overlayDirectory() {
    const std::string home = environment("HOME");
    if (home.empty()) {
        throw std::runtime_error("HOME is not set");
    }

    return synodrive::overlayDirectory(
        fs::path(home) / ".SynologyDrive/SynologyDrive.app/icon-overlay/current");
}

std::string query(const std::string& path) {
    if (!fs::path(path).is_absolute()) {
        throw std::runtime_error("path must be absolute");
    }
    std::string queryPath = path;
    while (queryPath.compare(0, 4, "/../") == 0) queryPath.erase(0, 3);
    if (queryPath == "/..") queryPath = "/";
    if (!synodrive::supportedInstalledVersion(clientInfoPath())) {
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
        if (getInfo(queryPath.c_str(), info) != 0) {
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
