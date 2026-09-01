#include "synodrive_overlay_plugin.h"

#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QPluginLoader>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <cerrno>
#include <csignal>
#include <limits>

namespace {
bool hasOverlay(const QSignalSpy& spy, const QStringList& overlay) {
    for (const QList<QVariant>& signal : spy) {
        if (signal.at(1).toStringList() == overlay) return true;
    }
    return false;
}
bool hasUrl(const QSignalSpy& spy, const QUrl& url) {
    for (const QList<QVariant>& signal : spy) {
        if (signal.at(0).toUrl() == url) return true;
    }
    return false;
}
bool hasUrlOverlay(const QSignalSpy& spy, const QUrl& url, const QStringList& overlay) {
    for (const QList<QVariant>& signal : spy) {
        if (signal.at(0).toUrl() == url && signal.at(1).toStringList() == overlay) return true;
    }
    return false;
}
}

class PluginTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void nonLocalDoesNothing();
    void loadsModule();
    void composesReviewedCli();
    void mappings_data();
    void mappings();
    void returnsBeforeHeldResponse();
    void preservesDistinctQueuedPaths();
    void failsEveryQueuedPath();
    void refreshesAndRecovers();
    void helperUnavailable();
    void prunesInactiveCache();
    void destructionDoesNotWait();

private:
    void control(const QByteArray& value);
    QStringList logLines() const;

    QTemporaryDir temporary_;
    QString helper_;
    QString cli_;
    QString synologyPlugin_;
    QString control_;
    QString log_;
    QString release_;
    QString pid_;
    QString wrapperPid_;
    QString queryPids_;
};

void PluginTest::init() {
    helper_ = QString::fromLocal8Bit(qgetenv("SYNODRIVE_TEST_HELPER"));
    cli_ = QString::fromLocal8Bit(qgetenv("SYNODRIVE_TEST_CLI"));
    synologyPlugin_ = QString::fromLocal8Bit(qgetenv("SYNODRIVE_TEST_SYNOLOGY_PLUGIN"));
    QVERIFY(!helper_.isEmpty());
    QVERIFY(!cli_.isEmpty());
    QVERIFY(!synologyPlugin_.isEmpty());
    control_ = temporary_.filePath("control");
    log_ = temporary_.filePath("log");
    release_ = temporary_.filePath("release");
    pid_ = temporary_.filePath("pid");
    wrapperPid_ = temporary_.filePath("wrapper-pid");
    queryPids_ = temporary_.filePath("query-pids");
    qputenv("FAKE_STATUS_CONTROL", control_.toLocal8Bit());
    qputenv("FAKE_STATUS_LOG", log_.toLocal8Bit());
    qputenv("FAKE_STATUS_RELEASE", release_.toLocal8Bit());
    qputenv("FAKE_STATUS_PID", pid_.toLocal8Bit());

    const QString overlay = temporary_.filePath(
        ".SynologyDrive/SynologyDrive.app/icon-overlay/15/lib");
    QVERIFY(QDir().mkpath(overlay));
    const QString installedFixture = overlay + "/plugin-cb-4.so";
    if (!QFile::exists(installedFixture)) QVERIFY(QFile::copy(synologyPlugin_, installedFixture));
    const QString current = temporary_.filePath(
        ".SynologyDrive/SynologyDrive.app/icon-overlay/current");
    if (!QFile::exists(current)) {
        QVERIFY(QFile::link(temporary_.filePath(
            ".SynologyDrive/SynologyDrive.app/icon-overlay/15"), current));
    }
    qputenv("HOME", temporary_.path().toLocal8Bit());
    const QString info = temporary_.filePath("INFO");
    QFile infoFile(info);
    QVERIFY(infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray metadata =
        "[Package]\ninstaller = deb\n\n[Version]\nmajor_version = 4\n"
        "minor_version = 0\nmini_version = 2\nbuild_version = 17889\n";
    QCOMPARE(infoFile.write(metadata), metadata.size());
    infoFile.close();
    qputenv("SYNODRIVE_STATUS_TEST_INFO", info.toLocal8Bit());
    qputenv("SYNODRIVE_STATUS_TEST_NAUTILUS", synologyPlugin_.toLocal8Bit());
    qputenv("SYNODRIVE_STATUS_TEST_WRAPPER_PID_LOG", wrapperPid_.toLocal8Bit());
    qputenv("FAKE_SYNODRIVE_CONTROL", control_.toLocal8Bit());
    qputenv("FAKE_SYNODRIVE_PID_LOG", queryPids_.toLocal8Bit());
    qputenv("FAKE_SYNODRIVE_RELEASE", release_.toLocal8Bit());
}

void PluginTest::cleanup() {
    QFile::remove(log_);
    QFile::remove(release_);
    QFile::remove(pid_);
    QFile::remove(wrapperPid_);
    QFile::remove(queryPids_);
    qunsetenv("FAKE_SYNODRIVE_HOLD");
}

void PluginTest::control(const QByteArray& value) {
    QFile file(control_);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(value), value.size());
}

QStringList PluginTest::logLines() const {
    QFile file(log_);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
}

void PluginTest::nonLocalDoesNothing() {
    SynodriveOverlayPlugin plugin(nullptr, helper_);
    QCOMPARE(plugin.getOverlays(QUrl("https://example.invalid/file")), QStringList{});
    QTest::qWait(50);
    QVERIFY(!QFile::exists(log_));
    QVERIFY(!QFile::exists(pid_));
}

void PluginTest::loadsModule() {
    QPluginLoader loader(QString::fromLocal8Bit(qgetenv("SYNODRIVE_TEST_PLUGIN")));
    QObject* instance = loader.instance();
    QVERIFY2(instance, qPrintable(loader.errorString()));
    QVERIFY(qobject_cast<KOverlayIconPlugin*>(instance));
    QVERIFY(loader.unload());
}

void PluginTest::composesReviewedCli() {
    control("1");
    qputenv("FAKE_SYNODRIVE_HOLD", "1");
    SynodriveOverlayPlugin plugin(nullptr, cli_);
    QSignalSpy changed(&plugin, &KOverlayIconPlugin::overlaysChanged);
    const QUrl first = QUrl::fromLocalFile("/tmp/composed-one");
    QCOMPARE(plugin.getOverlays(first), QStringList{});
    QTRY_VERIFY(QFile::exists(wrapperPid_));
    QCOMPARE(changed.count(), 0);

    QFile release(release_);
    QVERIFY(release.open(QIODevice::WriteOnly));
    QTRY_VERIFY(hasOverlay(changed, QStringList{"emblem-default"}));

    const QUrl second = QUrl::fromLocalFile("/tmp/composed-two");
    plugin.getOverlays(second);
    QTRY_VERIFY(hasUrl(changed, second));
    QFile wrapper(wrapperPid_);
    QVERIFY(wrapper.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(wrapper.readAll()).split('\n', Qt::SkipEmptyParts).size(), 1);
    QFile queries(queryPids_);
    QVERIFY(queries.open(QIODevice::ReadOnly));
    const QStringList pids = QString::fromUtf8(queries.readAll()).split('\n', Qt::SkipEmptyParts);
    QVERIFY(pids.size() >= 2);
    QVERIFY(pids.at(0) != pids.at(1));
}

void PluginTest::mappings_data() {
    QTest::addColumn<QByteArray>("status");
    QTest::addColumn<QStringList>("overlay");
    QTest::newRow("unknown") << QByteArray("unknown") << QStringList{};
    QTest::newRow("synced") << QByteArray("synced") << QStringList{"emblem-default"};
    QTest::newRow("syncing") << QByteArray("syncing") << QStringList{"emblem-synchronizing"};
    QTest::newRow("unsupported") << QByteArray("unsupported") << QStringList{};
    QTest::newRow("read-only") << QByteArray("read-only") << QStringList{"emblem-readonly"};
    QTest::newRow("no-permission") << QByteArray("no-permission") << QStringList{"emblem-unreadable"};
}

void PluginTest::mappings() {
    QFETCH(QByteArray, status);
    QFETCH(QStringList, overlay);
    control(status);
    SynodriveOverlayPlugin plugin(nullptr, helper_);
    QSignalSpy changed(&plugin, &KOverlayIconPlugin::overlaysChanged);
    plugin.getOverlays(QUrl::fromLocalFile("/tmp/mapping"));
    QTRY_VERIFY(hasOverlay(changed, overlay));
}

void PluginTest::returnsBeforeHeldResponse() {
    control("hold:synced");
    SynodriveOverlayPlugin plugin(nullptr, helper_);
    QSignalSpy changed(&plugin, &KOverlayIconPlugin::overlaysChanged);
    const QUrl url = QUrl::fromLocalFile("/tmp/held");
    QCOMPARE(plugin.getOverlays(url), QStringList{});
    QTRY_COMPARE(logLines(), QStringList{"/tmp/held"});
    QCOMPARE(changed.count(), 0);
    QFile release(release_);
    QVERIFY(release.open(QIODevice::WriteOnly));
    QTRY_VERIFY(hasOverlay(changed, QStringList{"emblem-default"}));
}

void PluginTest::preservesDistinctQueuedPaths() {
    control("hold:synced");
    SynodriveOverlayPlugin plugin(nullptr, helper_);
    QSignalSpy changed(&plugin, &KOverlayIconPlugin::overlaysChanged);
    const QUrl a = QUrl::fromLocalFile("/tmp/A");
    const QUrl b = QUrl::fromLocalFile(QString::fromUtf8("/tmp/B-é"));
    plugin.getOverlays(a);
    plugin.getOverlays(a);
    plugin.getOverlays(b);
    plugin.getOverlays(b);
    QTRY_COMPARE(logLines(), QStringList{"/tmp/A"});
    QFile release(release_);
    QVERIFY(release.open(QIODevice::WriteOnly));
    QTRY_VERIFY(logLines().size() >= 2);
    QCOMPARE(logLines().sliced(0, 2), (QStringList{"/tmp/A", QString::fromUtf8("/tmp/B-é")}));
    QTRY_VERIFY(hasUrlOverlay(changed, a, QStringList{"emblem-default"}));
    QTRY_VERIFY(hasUrlOverlay(changed, b, QStringList{"emblem-default"}));
}

void PluginTest::failsEveryQueuedPath() {
    control("hold:error");
    SynodriveOverlayPlugin plugin(nullptr, helper_);
    QSignalSpy changed(&plugin, &KOverlayIconPlugin::overlaysChanged);
    const QUrl a = QUrl::fromLocalFile("/tmp/fail-A");
    const QUrl b = QUrl::fromLocalFile("/tmp/fail-B");
    plugin.getOverlays(a);
    plugin.getOverlays(b);
    QTRY_COMPARE(logLines(), QStringList{"/tmp/fail-A"});
    QFile release(release_);
    QVERIFY(release.open(QIODevice::WriteOnly));
    QTRY_VERIFY(hasUrlOverlay(changed, a, QStringList{}));
    QTRY_VERIFY(hasUrlOverlay(changed, b, QStringList{}));
    QCOMPARE(logLines(), QStringList{"/tmp/fail-A"});

    control("synced");
    plugin.getOverlays(a);
    plugin.getOverlays(b);
    QTRY_VERIFY(logLines().size() >= 3);
    QTRY_VERIFY(hasUrlOverlay(changed, a, QStringList{"emblem-default"}));
    QTRY_VERIFY(hasUrlOverlay(changed, b, QStringList{"emblem-default"}));
}

void PluginTest::refreshesAndRecovers() {
    control("synced");
    SynodriveOverlayPlugin plugin(nullptr, helper_);
    QSignalSpy changed(&plugin, &KOverlayIconPlugin::overlaysChanged);
    const QUrl url = QUrl::fromLocalFile("/tmp/dynamic");
    QCOMPARE(plugin.getOverlays(url), QStringList{});
    QTRY_VERIFY(hasOverlay(changed, QStringList{"emblem-default"}));
    QCOMPARE(plugin.getOverlays(url), QStringList{"emblem-default"});

    changed.clear();
    control("oversized");
    QTRY_VERIFY(hasOverlay(changed, QStringList{}));

    changed.clear();
    control("synced");
    QCOMPARE(plugin.getOverlays(url), QStringList{});
    QTRY_VERIFY(hasOverlay(changed, QStringList{"emblem-default"}));

    changed.clear();
    control("syncing");
    QElapsedTimer refreshDeadline;
    refreshDeadline.start();
    QTRY_VERIFY(hasOverlay(changed, QStringList{"emblem-synchronizing"}));
    QVERIFY(refreshDeadline.elapsed() < 250);

    changed.clear();
    bool queuedDuringReset = false;
    const QMetaObject::Connection reentrant = connect(
        &plugin, &KOverlayIconPlugin::overlaysChanged, &plugin,
        [&](const QUrl& changedUrl, const QStringList& overlay) {
            if (!queuedDuringReset && changedUrl == url && overlay.isEmpty()) {
                queuedDuringReset = true;
                control("synced");
                QCOMPARE(plugin.getOverlays(url), QStringList{});
            }
        }, Qt::DirectConnection);
    control("malformed");
    QTRY_VERIFY(queuedDuringReset);
    QTRY_VERIFY(hasUrlOverlay(changed, url, QStringList{"emblem-default"}));
    disconnect(reentrant);

    changed.clear();
    control("exit");
    QTRY_VERIFY(hasOverlay(changed, QStringList{}));

    changed.clear();
    control("synced");
    QCOMPARE(plugin.getOverlays(url), QStringList{});
    QTRY_VERIFY(hasOverlay(changed, QStringList{"emblem-default"}));
}

void PluginTest::helperUnavailable() {
    SynodriveOverlayPlugin plugin(nullptr, "/definitely/missing/synodrive-status");
    QSignalSpy changed(&plugin, &KOverlayIconPlugin::overlaysChanged);
    QCOMPARE(plugin.getOverlays(QUrl::fromLocalFile("/tmp/unavailable")), QStringList{});
    QTRY_VERIFY(hasOverlay(changed, QStringList{}));
}

void PluginTest::prunesInactiveCache() {
    control("synced");
    SynodriveOverlayPlugin plugin(nullptr, helper_);
    QSignalSpy changed(&plugin, &KOverlayIconPlugin::overlaysChanged);
    const QUrl url = QUrl::fromLocalFile("/tmp/pruned");
    plugin.getOverlays(url);
    QTRY_VERIFY(!changed.isEmpty());
    changed.clear();
    QElapsedTimer deadline;
    deadline.start();
    bool pruned = false;
    while (deadline.elapsed() < 2000 && !pruned) {
        for (const QList<QVariant>& signal : changed) {
            pruned = signal.at(1).toStringList().isEmpty();
        }
        changed.clear();
        QTest::qWait(25);
    }
    QVERIFY(pruned);
    QCOMPARE(plugin.getOverlays(url), QStringList{});
}

void PluginTest::destructionDoesNotWait() {
    control("1");
    qputenv("FAKE_SYNODRIVE_HOLD", "1");
    auto* plugin = new SynodriveOverlayPlugin(nullptr, cli_);
    plugin->getOverlays(QUrl::fromLocalFile("/tmp/slow"));
    QTRY_VERIFY(QFile::exists(wrapperPid_));
    QFile wrapperFile(wrapperPid_);
    QVERIFY(wrapperFile.open(QIODevice::ReadOnly));
    bool wrapperOk = false;
    const qlonglong wrapperValue = wrapperFile.readAll().trimmed().toLongLong(&wrapperOk);
    QVERIFY(wrapperOk && wrapperValue > 0 && wrapperValue <= std::numeric_limits<pid_t>::max());
    const pid_t wrapper = static_cast<pid_t>(wrapperValue);

    QByteArray children;
    QTRY_VERIFY([&] {
        QFile file(QString("/proc/%1/task/%1/children").arg(wrapper));
        if (!file.open(QIODevice::ReadOnly)) return false;
        children = file.readAll().trimmed();
        return !children.isEmpty();
    }());
    bool childOk = false;
    const qlonglong childValue = children.split(' ').first().toLongLong(&childOk);
    QVERIFY(childOk && childValue > 0 && childValue <= std::numeric_limits<pid_t>::max());
    const pid_t child = static_cast<pid_t>(childValue);

    QElapsedTimer elapsed;
    elapsed.start();
    delete plugin;
    QVERIFY(elapsed.elapsed() < 100);
    QTRY_VERIFY(kill(wrapper, 0) == -1 && errno == ESRCH);
    QTRY_VERIFY(kill(child, 0) == -1 && errno == ESRCH);
}

QTEST_GUILESS_MAIN(PluginTest)
#include "test_plugin.moc"
