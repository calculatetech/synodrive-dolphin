#include "synodrive_compatibility.h"

#include <dlfcn.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

constexpr std::string_view kClientInfo = "/opt/Synology/SynologyDrive/INFO";
constexpr std::string_view kShareName = "NautilusCloudStation::ShareLink";
constexpr std::string_view kVersionName = "NautilusCloudStation::VersionBrowse";

struct GList {
    void* data;
    GList* next;
    GList* prev;
};

struct Selection {
    std::string uri;
};

struct MenuItem {
    std::string name;
    void* object;
};

class SharedObject {
public:
    explicit SharedObject(const char* path)
        : handle_(dlopen(path, RTLD_NOW | RTLD_GLOBAL)) {
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
        void* value = dlsym(handle_, name);
        if (const char* error = dlerror()) {
            throw std::runtime_error(error);
        }
        return reinterpret_cast<T>(value);
    }

private:
    void* handle_;
};

struct Api {
    using ObjectGet = void (*)(void*, const char*, ...);
    using ObjectUnref = void (*)(void*);
    using Free = void (*)(void*);
    using MenuGetItems = GList* (*)(void*);
    using MenuItemActivate = void (*)(void*);
    using MenuItemListFree = void (*)(GList*);

    ObjectGet objectGet{};
    ObjectUnref objectUnref{};
    Free free{};
    MenuGetItems menuGetItems{};
    MenuItemActivate menuItemActivate{};
    MenuItemListFree menuItemListFree{};
};

template <typename T>
T symbol(const char* name) {
    dlerror();
    void* value = dlsym(RTLD_DEFAULT, name);
    if (const char* error = dlerror()) {
        throw std::runtime_error(std::string(name) + ": " + error);
    }
    return reinterpret_cast<T>(value);
}

Api loadApi() {
    return {
        symbol<Api::ObjectGet>("g_object_get"),
        symbol<Api::ObjectUnref>("g_object_unref"),
        symbol<Api::Free>("g_free"),
        symbol<Api::MenuGetItems>("nautilus_menu_get_items"),
        symbol<Api::MenuItemActivate>("nautilus_menu_item_activate"),
        symbol<Api::MenuItemListFree>("nautilus_menu_item_list_free"),
    };
}

class MenuSnapshot {
public:
    MenuSnapshot(const Api& api, GList* roots)
        : api_(api), roots_(roots) {
        collect(roots_);
    }

    ~MenuSnapshot() {
        for (GList* list : childLists_) {
            api_.menuItemListFree(list);
        }
        for (void* submenu : submenus_) {
            api_.objectUnref(submenu);
        }
        if (roots_) {
            api_.menuItemListFree(roots_);
        }
    }

    MenuSnapshot(const MenuSnapshot&) = delete;
    MenuSnapshot& operator=(const MenuSnapshot&) = delete;

    const std::vector<MenuItem>& items() const { return items_; }

private:
    void collect(GList* list) {
        for (GList* node = list; node; node = node->next) {
            void* submenu = nullptr;
            api_.objectGet(node->data, "menu", &submenu, nullptr);
            if (submenu) {
                submenus_.push_back(submenu);
                GList* children = api_.menuGetItems(submenu);
                if (children) {
                    childLists_.push_back(children);
                    collect(children);
                }
                continue;
            }

            char* name = nullptr;
            int sensitive = 0;
            api_.objectGet(node->data, "name", &name, "sensitive", &sensitive, nullptr);
            if (sensitive && name) {
                items_.push_back({name, node->data});
            }
            api_.free(name);
        }
    }

    const Api& api_;
    GList* roots_{};
    std::vector<GList*> childLists_;
    std::vector<void*> submenus_;
    std::vector<MenuItem> items_;
};

std::string environment(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}

fs::path infoPath() {
#ifdef SYNODRIVE_ACTION_TESTING
    if (const std::string value = environment("SYNODRIVE_ACTION_TEST_INFO"); !value.empty()) {
        return value;
    }
#endif
    return kClientInfo;
}

fs::path currentOverlayPath() {
#ifdef SYNODRIVE_ACTION_TESTING
    if (const std::string value = environment("SYNODRIVE_ACTION_TEST_OVERLAY"); !value.empty()) {
        return value;
    }
#endif
    const std::string home = environment("HOME");
    if (home.empty()) {
        throw std::runtime_error("HOME is not set");
    }
    return fs::path(home) / ".SynologyDrive/SynologyDrive.app/icon-overlay/current";
}

const char* nautilusLibrary() {
#ifdef SYNODRIVE_ACTION_TESTING
    static const std::string value = environment("SYNODRIVE_ACTION_TEST_NAUTILUS");
    if (!value.empty()) {
        return value.c_str();
    }
#endif
    return "libnautilus-extension.so.4";
}

std::string fileUri(const fs::path& path) {
    static constexpr char hex[] = "0123456789ABCDEF";
    const std::string bytes = path.string();
    std::string uri = "file://";
    for (const unsigned char byte : bytes) {
        const bool unreserved = (byte >= 'a' && byte <= 'z') ||
                                (byte >= 'A' && byte <= 'Z') ||
                                (byte >= '0' && byte <= '9') ||
                                byte == '-' || byte == '.' || byte == '_' || byte == '~' || byte == '/';
        if (unreserved) {
            uri.push_back(static_cast<char>(byte));
        } else {
            uri.push_back('%');
            uri.push_back(hex[byte >> 4]);
            uri.push_back(hex[byte & 0x0f]);
        }
    }
    return uri;
}

std::string_view actionId(std::string_view name) {
    if (name == kShareName) {
        return "get-link";
    }
    if (name == kVersionName) {
        return "browse-versions";
    }
    return {};
}

std::string_view actionName(std::string_view id) {
    if (id == "get-link") {
        return kShareName;
    }
    if (id == "browse-versions") {
        return kVersionName;
    }
    return {};
}

int run(std::string_view action, const fs::path& input) {
    std::error_code error;
    const bool exists = fs::exists(input, error);
    const bool supportedType = fs::is_regular_file(input, error) || fs::is_directory(input, error);
    if (!input.is_absolute() || error || !exists || !supportedType) {
        return 2;
    }
    if (!synodrive::supportedInstalledVersion(infoPath())) {
        throw std::runtime_error(
            "unsupported or malformed Synology Drive metadata (expected internal major 4)");
    }

    const fs::path canonicalPath = fs::canonical(input);
    Selection selection{fileUri(canonicalPath)};
    GList selected{&selection, nullptr, nullptr};

    SharedObject nautilus(nautilusLibrary());
    const fs::path helperPath =
        synodrive::overlayDirectory(currentOverlayPath()) / "lib/plugin-cb-4.so";
    SharedObject helper(helperPath.c_str());
    const Api api = loadApi();

    using GetFileItems = GList* (*)(void*, GList*);
    const auto getFileItems =
        helper.symbol<GetFileItems>("cstn_private_get_file_item");

    int providerSentinel = 1;
    MenuSnapshot menu(api, getFileItems(&providerSentinel, &selected));
    std::set<std::string_view> seen;
    std::vector<std::pair<std::string_view, void*>> supported;
    for (const MenuItem& item : menu.items()) {
        const std::string_view id = actionId(item.name);
        if (id.empty()) {
            continue;
        }
        if (!seen.insert(id).second) {
            throw std::runtime_error("duplicate Synology action");
        }
        supported.emplace_back(id, item.object);
    }

    if (action.empty()) {
        for (const auto& [id, object] : supported) {
            static_cast<void>(object);
            std::cout << id << '\n';
        }
        return 0;
    }

    for (const auto& [id, object] : supported) {
        if (id == action) {
            api.menuItemActivate(object);
            return 0;
        }
    }
    throw std::runtime_error("action is not available");
}

void usage() {
    std::cerr << "usage: synodrive-action --list <absolute-path>\n"
                 "       synodrive-action --activate get-link|browse-versions <absolute-path>\n";
}

}

extern "C" char* nautilus_file_info_get_uri(void* fileInfo) {
    const auto* selection = static_cast<const Selection*>(fileInfo);
    return ::strdup(selection->uri.c_str());
}

int main(int argc, char** argv) {
#ifdef SYNODRIVE_ACTION_TESTING
    if (const std::string pidLog = environment("SYNODRIVE_ACTION_TEST_PID_LOG"); !pidLog.empty()) {
        std::ofstream(pidLog, std::ios::app) << getpid() << '\n';
    }
#endif
    std::string_view action;
    const char* path = nullptr;
    if (argc == 3 && std::string_view(argv[1]) == "--list") {
        path = argv[2];
    } else if (argc == 4 && std::string_view(argv[1]) == "--activate") {
        action = argv[2];
        path = argv[3];
        if (actionName(action).empty()) {
            usage();
            return 2;
        }
    } else {
        usage();
        return 2;
    }

    try {
        const int result = run(action, path);
        if (result == 2) {
            usage();
        }
        return result;
    } catch (const std::exception& error) {
        std::cerr << "synodrive-action: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "synodrive-action: unexpected helper failure\n";
        return 1;
    }
}
