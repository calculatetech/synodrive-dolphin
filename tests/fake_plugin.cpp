#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

struct _tag_ICONOVERLAY_INFO {
    int enable;
    int file_status;
};

void PrepareCacheTable() {
    if (const char* fail = std::getenv("FAKE_SYNODRIVE_PREPARE_FAIL"); fail && *fail) {
        throw std::runtime_error("fixture daemon unavailable");
    }
    if (const char* hold = std::getenv("FAKE_SYNODRIVE_HOLD"); hold && *hold) {
        const char* release = std::getenv("FAKE_SYNODRIVE_RELEASE");
        while (release && !std::filesystem::exists(release)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

int GetIconOverlayInfoByPath(const char* path, _tag_ICONOVERLAY_INFO& info) {
    if (const char* log = std::getenv("FAKE_SYNODRIVE_PID_LOG"); log && *log) {
        std::ofstream(log, std::ios::app) << getpid() << '\n';
    }
    const char* control = std::getenv("FAKE_SYNODRIVE_CONTROL");
    if (!control) {
        return 1;
    }
    if (const char* expected = std::getenv("FAKE_SYNODRIVE_EXPECT_PATH");
        expected && path != std::string(expected)) {
        return 1;
    }
    std::ifstream input(control);
    input >> info.file_status;
    info.enable = 1;
    return input ? 0 : 1;
}
