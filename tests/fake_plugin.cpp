#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

struct _tag_ICONOVERLAY_INFO {
    int enable;
    int file_status;
};

void PrepareCacheTable() {
    if (const char* fail = std::getenv("FAKE_SYNODRIVE_PREPARE_FAIL"); fail && *fail) {
        throw std::runtime_error("fixture daemon unavailable");
    }
}

int GetIconOverlayInfoByPath(const char*, _tag_ICONOVERLAY_INFO& info) {
    if (const char* log = std::getenv("FAKE_SYNODRIVE_PID_LOG"); log && *log) {
        std::ofstream(log, std::ios::app) << getpid() << '\n';
    }
    const char* control = std::getenv("FAKE_SYNODRIVE_CONTROL");
    if (!control) {
        return 1;
    }
    std::ifstream input(control);
    input >> info.file_status;
    info.enable = 1;
    return input ? 0 : 1;
}
