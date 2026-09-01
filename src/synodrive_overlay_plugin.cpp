#include "synodrive_overlay_plugin.h"

#include <QUrl>

SynodriveOverlayPlugin::SynodriveOverlayPlugin(QObject* parent, QString helper)
    : KOverlayIconPlugin(parent), provider_(std::move(helper), this) {
    connect(&provider_, &StatusProvider::statusChanged, this,
            [this](const QString& path, SyncStatus status) {
                emit overlaysChanged(QUrl::fromLocalFile(path), overlays(status));
            });
}
QStringList SynodriveOverlayPlugin::getOverlays(const QUrl& url) {
    if (!url.isLocalFile()) {
        return {};
    }
    const QString path = url.toLocalFile();
    const auto status = provider_.status(path);
    if (!status) {
        provider_.request(path);
        return {};
    }
    return overlays(*status);
}

QStringList SynodriveOverlayPlugin::overlays(SyncStatus status) {
    switch (status) {
    case SyncStatus::Synced: return {"emblem-default"};
    case SyncStatus::Syncing: return {"emblem-synchronizing"};
    case SyncStatus::ReadOnly: return {"emblem-readonly"};
    case SyncStatus::NoPermission: return {"emblem-unreadable"};
    case SyncStatus::Unknown:
    case SyncStatus::Unsupported: return {};
    }
    return {};
}
