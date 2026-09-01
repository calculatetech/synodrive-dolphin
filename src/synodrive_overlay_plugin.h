#pragma once

#include "status_provider.h"

#include <KOverlayIconPlugin>

class SynodriveOverlayPlugin : public KOverlayIconPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.overlayicon.synodrive")

public:
    explicit SynodriveOverlayPlugin(QObject* parent = nullptr, QString helper = {});
    QStringList getOverlays(const QUrl& url) override;

private:
    static QStringList overlays(SyncStatus status);
    StatusProvider provider_;
};
