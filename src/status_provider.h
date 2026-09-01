#pragma once

#include <QObject>
#include <QHash>
#include <QProcess>
#include <QQueue>
#include <QSet>
#include <QTimer>

#include <optional>

enum class SyncStatus {
    Unknown,
    Synced,
    Syncing,
    Unsupported,
    ReadOnly,
    NoPermission,
};
Q_DECLARE_METATYPE(SyncStatus)

class StatusProvider : public QObject {
    Q_OBJECT

public:
    explicit StatusProvider(QString program = {}, QObject* parent = nullptr);
    ~StatusProvider() override;

    std::optional<SyncStatus> status(const QString& path);
    void request(const QString& path);

signals:
    void statusChanged(const QString& path, SyncStatus previous, SyncStatus current);

private:
    struct Entry {
        SyncStatus status;
    };

    void pump();
    void sendActive();
    void readResponse();
    void processFinished();
    void failAll();
    void refresh();
    void scheduleRefresh();
    static std::optional<SyncStatus> parseStatus(const QByteArray& value);

    QString program_;
    QProcess* process_;
    QTimer timer_;
    QQueue<QString> queue_;
    QSet<QString> queued_;
    QHash<QString, Entry> cache_;
    QHash<QString, qint64> accesses_;
    QString active_;
    QByteArray response_;
    bool requestWritten_ = false;
    bool resetting_ = false;
    bool shuttingDown_ = false;
};
