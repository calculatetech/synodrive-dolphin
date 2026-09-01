#include "status_provider.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>

namespace {
#ifdef SYNODRIVE_PROVIDER_TESTING
constexpr qint64 kRefreshMilliseconds = 100;
constexpr qint64 kRecentMilliseconds = 750;
#else
constexpr qint64 kRefreshMilliseconds = 1000;
constexpr qint64 kRecentMilliseconds = 30000;
#endif
constexpr qsizetype kMaxResponse = 64;
}

StatusProvider::StatusProvider(QString program, QObject* parent)
    : QObject(parent),
      program_(program.isEmpty() ? QStandardPaths::findExecutable("synodrive-status")
                                 : std::move(program)),
      process_(new QProcess(this)) {
    connect(process_, &QProcess::started, this, &StatusProvider::sendActive);
    connect(process_, &QProcess::readyReadStandardOutput, this, &StatusProvider::readResponse);
    connect(process_, &QProcess::errorOccurred, this, [this] {
        if (!resetting_) failAll();
    });
    connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this] { processFinished(); });
    connect(&timer_, &QTimer::timeout, this, &StatusProvider::refresh);
}

StatusProvider::~StatusProvider() {
    shuttingDown_ = true;
    timer_.stop();
    disconnect(process_, nullptr, this, nullptr);
    if (process_->state() != QProcess::NotRunning) {
        QProcess* process = process_;
        process->setParent(QCoreApplication::instance());
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                process, &QObject::deleteLater);
        process->kill();
        process_ = nullptr;
    }
}

std::optional<SyncStatus> StatusProvider::status(const QString& path) {
    accesses_.insert(path, QDateTime::currentMSecsSinceEpoch());
    auto found = cache_.find(path);
    if (found == cache_.end()) {
        return std::nullopt;
    }
    return found->status;
}

void StatusProvider::request(const QString& path) {
    if (path.isEmpty() || path == active_ || queued_.contains(path)) {
        return;
    }
    queue_.enqueue(path);
    queued_.insert(path);
    pump();
}

void StatusProvider::pump() {
    if (shuttingDown_ || resetting_ || !active_.isEmpty() || queue_.isEmpty()) {
        return;
    }
    active_ = queue_.dequeue();
    queued_.remove(active_);
    requestWritten_ = false;
    response_.clear();

    if (program_.isEmpty()) {
        failAll();
    } else if (process_->state() == QProcess::NotRunning) {
        process_->start(program_, {"--stdio"}, QIODevice::ReadWrite);
    } else if (process_->state() == QProcess::Running) {
        sendActive();
    }
}

void StatusProvider::sendActive() {
    if (requestWritten_ || active_.isEmpty() || process_->state() != QProcess::Running) {
        return;
    }
    QByteArray frame = active_.toUtf8();
    frame.append('\0');
    if (process_->write(frame) != frame.size()) {
        failAll();
        return;
    }
    requestWritten_ = true;
}

void StatusProvider::readResponse() {
    response_.append(process_->readAllStandardOutput());
    if (response_.size() > kMaxResponse) {
        failAll();
        return;
    }

    const qsizetype delimiter = response_.indexOf('\0');
    if (delimiter < 0) {
        return;
    }
    if (delimiter != response_.size() - 1) {
        failAll();
        return;
    }

    const auto parsed = parseStatus(response_.first(delimiter));
    if (!parsed) {
        failAll();
        return;
    }

    const QString completed = active_;
    const bool stillRequested = accesses_.contains(completed);
    SyncStatus previous = SyncStatus::Unknown;
    const auto cached = cache_.constFind(completed);
    if (cached != cache_.cend()) {
        previous = cached->status;
    }
    active_.clear();
    requestWritten_ = false;
    response_.clear();
    if (stillRequested) {
        cache_.insert(completed, {*parsed});
        if (previous != *parsed) {
            emit statusChanged(completed, previous, *parsed);
        }
    }
    scheduleRefresh();
    pump();
}

void StatusProvider::processFinished() {
    if (shuttingDown_) {
        return;
    }
    resetting_ = false;
    if (!active_.isEmpty()) {
        failAll();
    }
    pump();
}

void StatusProvider::failAll() {
    if (shuttingDown_) {
        return;
    }
    QStringList failed;
    if (!active_.isEmpty()) {
        failed.append(active_);
    }
    while (!queue_.isEmpty()) {
        failed.append(queue_.dequeue());
    }
    queued_.clear();
    active_.clear();
    requestWritten_ = false;
    response_.clear();

    const bool stopProcess = process_->state() != QProcess::NotRunning;
    if (stopProcess) {
        resetting_ = true;
    }

    for (const QString& path : failed) {
        SyncStatus previous = SyncStatus::Unknown;
        const auto cached = cache_.constFind(path);
        if (cached != cache_.cend()) {
            previous = cached->status;
        }
        cache_.remove(path);
        accesses_.remove(path);
        if (previous != SyncStatus::Unknown) {
            emit statusChanged(path, previous, SyncStatus::Unknown);
        }
    }
    scheduleRefresh();

    if (stopProcess) {
        process_->kill();
    }
}

void StatusProvider::refresh() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList syncing;
    for (auto entry = cache_.begin(); entry != cache_.end();) {
        const QString path = entry.key();
        if (now - accesses_.value(path, 0) >= kRecentMilliseconds) {
            entry = cache_.erase(entry);
            accesses_.remove(path);
        } else {
            if (entry->status == SyncStatus::Syncing) {
                syncing.append(path);
            }
            ++entry;
        }
    }
    for (const QString& path : syncing) {
        request(path);
    }
    scheduleRefresh();
}

void StatusProvider::scheduleRefresh() {
    if (cache_.isEmpty()) {
        timer_.stop();
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 delay = kRecentMilliseconds;
    for (auto entry = cache_.cbegin(); entry != cache_.cend(); ++entry) {
        const qint64 remaining = kRecentMilliseconds - (now - accesses_.value(entry.key(), 0));
        delay = std::min(delay, std::max<qint64>(1, remaining));
        if (entry->status == SyncStatus::Syncing) {
            delay = std::min(delay, kRefreshMilliseconds);
        }
    }
    timer_.start(static_cast<int>(delay));
}

std::optional<SyncStatus> StatusProvider::parseStatus(const QByteArray& value) {
    if (value == "unknown") return SyncStatus::Unknown;
    if (value == "synced") return SyncStatus::Synced;
    if (value == "syncing") return SyncStatus::Syncing;
    if (value == "unsupported") return SyncStatus::Unsupported;
    if (value == "read-only") return SyncStatus::ReadOnly;
    if (value == "no-permission") return SyncStatus::NoPermission;
    return std::nullopt;
}
