#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

std::string readFile(const char* name) {
    const char* path = std::getenv(name);
    if (!path) return {};
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--stdio") return 2;
    const char* log = std::getenv("FAKE_STATUS_LOG");
    const char* pidFile = std::getenv("FAKE_STATUS_PID");
    if (pidFile) std::ofstream(pidFile) << getpid();

    std::string path;
    while (std::getline(std::cin, path, '\0')) {
        if (log) std::ofstream(log, std::ios::app) << path << '\n';
        std::string mode = readFile("FAKE_STATUS_CONTROL");
        if (mode.rfind("hold:", 0) == 0) {
            const char* release = std::getenv("FAKE_STATUS_RELEASE");
            while (release && !fs::exists(release)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            mode.erase(0, 5);
        }
        if (mode == "exit") return 1;
        if (mode == "malformed") mode = "not-a-status";
        if (mode == "oversized") mode.assign(65, 'x');
        std::cout << mode << '\0' << std::flush;
    }
}
