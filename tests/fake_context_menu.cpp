#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

struct GList {
    void *data;
    GList *next;
    GList *prev;
};

extern "C" {
void *nautilus_menu_item_new(const char *, const char *, const char *, const char *);
void *nautilus_menu_new();
void nautilus_menu_append_item(void *, void *);
void nautilus_menu_item_set_submenu(void *, void *);
char *nautilus_file_info_get_uri(void *);
void g_object_set(void *, const char *, ...);
void g_object_unref(void *);
GList *g_list_append(GList *, void *);
unsigned long g_signal_connect_data(void *, const char *, void (*)(), void *, void (*)(void *, void *), int);
void g_free(void *);
}

namespace {

std::string selectionUri;

void activated(void *, void *data)
{
    if (const char *delay = std::getenv("FAKE_CONTEXT_ACTIVATION_DELAY"); delay && *delay) {
        std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(delay)));
    }
    if (const char *status = std::getenv("FAKE_CONTEXT_ACTIVATION_EXIT"); status && *status) {
        _exit(std::atoi(status));
    }
    if (const char *path = std::getenv("FAKE_CONTEXT_LOG"); path && *path) {
        std::ofstream(path, std::ios::app) << static_cast<const char *>(data)
                                            << ' ' << selectionUri << '\n';
    }
}

void append(void *menu, const char *name, const char *label, bool sensitive = true)
{
    void *item = nautilus_menu_item_new(name, label, nullptr, nullptr);
    if (!sensitive) {
        g_object_set(item, "sensitive", 0, nullptr);
    }
    g_signal_connect_data(item, "activate", reinterpret_cast<void (*)()>(activated),
                          const_cast<char *>(name), nullptr, 0);
    nautilus_menu_append_item(menu, item);
    g_object_unref(item);
}

} // namespace

extern "C" GList *cstn_private_get_file_item(void *, GList *selected)
{
    const std::string mode = std::getenv("FAKE_CONTEXT_MODE")
        ? std::getenv("FAKE_CONTEXT_MODE") : "both";
    if (mode == "sleep") {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (mode == "nonzero") {
        _exit(9);
    }
    if (mode == "partial") {
        std::cout << "get-link\n" << std::flush;
        _exit(9);
    }
    if (mode == "crash") {
        std::raise(SIGSEGV);
    }
    if (selected && selected->data) {
        char *uri = nautilus_file_info_get_uri(selected->data);
        selectionUri = uri ? uri : "";
        g_free(uri);
    }
    if (mode == "empty") {
        return nullptr;
    }

    void *root = nautilus_menu_item_new("NautilusCloudStation::Root", "Synology Drive",
                                        nullptr, nullptr);
    void *menu = nautilus_menu_new();
    if (mode == "unrelated") {
        append(menu, "Other::Action", "Other action");
    } else if (mode == "prefix") {
        append(menu, "NautilusCloudStation::ShareLinkExtra", "Get link extra");
    } else if (mode == "prefix-exact") {
        append(menu, "NautilusCloudStation::ShareLinkExtra", "Get link extra");
        append(menu, "NautilusCloudStation::ShareLink", "Get link");
    } else if (mode == "disabled") {
        append(menu, "NautilusCloudStation::ShareLink", "Get link", false);
    } else if (mode == "reverse") {
        append(menu, "NautilusCloudStation::VersionBrowse", "Browse previous versions");
        append(menu, "NautilusCloudStation::ShareLink", "Get link");
    } else {
        append(menu, "NautilusCloudStation::ShareLink", "Get link");
        if (mode != "single") {
            append(menu, "NautilusCloudStation::VersionBrowse", "Browse previous versions");
        }
        if (mode == "duplicate") {
            append(menu, "NautilusCloudStation::ShareLink", "Get link again");
        }
    }
    nautilus_menu_item_set_submenu(root, menu);
    g_object_unref(menu);
    return g_list_append(nullptr, root);
}
