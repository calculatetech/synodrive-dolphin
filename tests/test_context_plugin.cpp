#include "synodrive_fileitemaction_plugin.h"

#include <KFileItem>
#include <KFileItemActions>
#include <KFileItemListProperties>

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QMenu>
#include <QMessageBox>
#include <QPluginLoader>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <cerrno>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

QAction *findAction(QMenu &menu, const QString &text)
{
    for (QAction *action : menu.actions()) {
        if (action->text() == text) {
            return action;
        }
        if (action->menu()) {
            if (QAction *found = findAction(*action->menu(), text)) {
                return found;
            }
        }
    }
    return nullptr;
}

struct MenuBundle {
    std::unique_ptr<QMenu> menu;
    std::unique_ptr<KFileItemActions> owner;
};

MenuBundle buildMenu(const QList<QUrl> &urls)
{
    KFileItemList items;
    for (const QUrl &url : urls) {
        items.append(KFileItem(url, QStringLiteral("text/plain")));
    }
    auto actions = std::make_unique<KFileItemActions>();
    actions->setItemListProperties(KFileItemListProperties(items));
    auto menu = std::make_unique<QMenu>();
    actions->setParentWidget(menu.get());
    actions->addActionsTo(menu.get(), KFileItemActions::MenuActionSource::Plugins);
    return {std::move(menu), std::move(actions)};
}

MenuBundle buildMenu(const QString &path)
{
    return buildMenu({QUrl::fromLocalFile(path)});
}

std::unique_ptr<QMenu> buildPluginMenu(SynodriveFileItemActionPlugin &plugin,
                                       const QList<QUrl> &urls)
{
    KFileItemList items;
    for (const QUrl &url : urls) {
        items.append(KFileItem(url, QStringLiteral("text/plain")));
    }
    auto menu = std::make_unique<QMenu>();
    for (QAction *action : plugin.actions(KFileItemListProperties(items), menu.get())) {
        menu->addAction(action);
    }
    return menu;
}

QList<qint64> pids(const fs::path &path)
{
    QList<qint64> result;
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }
    for (const QByteArray &line : file.readAll().split('\n')) {
        if (!line.isEmpty()) {
            result.append(line.toLongLong());
        }
    }
    return result;
}

QList<QMessageBox *> errorDialogs()
{
    QList<QMessageBox *> result;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *dialog = qobject_cast<QMessageBox *>(widget)) {
            result.append(dialog);
        }
    }
    return result;
}

bool require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    if (argc != 3) {
        return 2;
    }

    const QJsonArray serviceTypes = QPluginLoader(
        QString::fromLocal8Bit(qgetenv("SYNODRIVE_ACTION_TEST_MODULE")))
        .metaData().value(QStringLiteral("MetaData")).toObject()
        .value(QStringLiteral("KPlugin")).toObject()
        .value(QStringLiteral("ServiceTypes")).toArray();
    if (!require(serviceTypes.contains(QStringLiteral("KFileItemAction/Plugin")),
                 "module metadata lacks the KFileItemAction service type")) {
        return 1;
    }

    const QList<QByteArray> invalid = {
        "get-link", "get-link\r\n", "\n", "get-link\n\n",
        "get-link\nget-link\n", "unknown\n", "get-link\ntrailing"
    };
    for (const QByteArray &value : invalid) {
        if (!require(synodrive::parseActionList(value).isEmpty(), "parser accepted invalid output")) {
            return 1;
        }
    }
    if (!require(synodrive::parseActionList("get-link\nbrowse-versions\n").size() == 2,
                 "parser rejected valid output")) {
        return 1;
    }

    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        return 1;
    }
    const fs::path root = temporary.path().toStdString();
    const fs::path overlay = root / "overlay";
    fs::create_directories(overlay / "15/lib");
    fs::copy_file(argv[1], overlay / "15/lib/plugin-cb-4.so");
    fs::create_directory_symlink("15", overlay / "current");
    const fs::path info = root / "INFO";
    QFile infoFile(QString::fromStdString(info.string()));
    if (!infoFile.open(QIODevice::WriteOnly)) {
        return 1;
    }
    infoFile.write("[Version]\nmajor_version = 4\n");
    infoFile.close();
    const fs::path selected = root / "selected.txt";
    QFile selectedFile(QString::fromStdString(selected.string()));
    if (!selectedFile.open(QIODevice::WriteOnly)) {
        return 1;
    }
    selectedFile.write("test");
    selectedFile.close();
    const fs::path log = root / "activation.log";
    const fs::path pidLog = root / "pids";
    SynodriveFileItemActionPlugin directPlugin(nullptr);

    qputenv("SYNODRIVE_ACTION_TEST_INFO", info.string().c_str());
    qputenv("SYNODRIVE_ACTION_TEST_OVERLAY", (overlay / "current").string().c_str());
    qputenv("SYNODRIVE_ACTION_TEST_NAUTILUS", "libnautilus-extension.so.4");
    qputenv("FAKE_CONTEXT_LOG", log.string().c_str());
    qputenv("SYNODRIVE_ACTION_TEST_PID_LOG", pidLog.string().c_str());

    const QString selectedPath = QString::fromStdString(selected.string());
    const qsizetype beforeUnsupported = pids(pidLog).size();
    auto noSelection = buildPluginMenu(directPlugin, {});
    auto multiple = buildPluginMenu(directPlugin, {QUrl::fromLocalFile(selectedPath), QUrl::fromLocalFile(selectedPath)});
    auto remote = buildPluginMenu(directPlugin, {QUrl(QStringLiteral("https://example.invalid/file"))});
    auto missing = buildPluginMenu(directPlugin, {QUrl::fromLocalFile(QString::fromStdString((root / "missing").string()))});
    const fs::path fifo = root / "fifo";
    if (mkfifo(fifo.c_str(), 0600) != 0) {
        return 1;
    }
    auto other = buildPluginMenu(directPlugin, {QUrl::fromLocalFile(QString::fromStdString(fifo.string()))});
    if (!require(!findAction(*noSelection, QStringLiteral("Synology Drive")) &&
                 !findAction(*multiple, QStringLiteral("Synology Drive")) &&
                 !findAction(*remote, QStringLiteral("Synology Drive")) &&
                 !findAction(*missing, QStringLiteral("Synology Drive")) &&
                 !findAction(*other, QStringLiteral("Synology Drive")),
                 "unsupported selection created a menu") ||
        !require(pids(pidLog).size() == beforeUnsupported,
                 "unsupported selection started a helper")) {
        return 1;
    }

    const qsizetype beforeFile = pids(pidLog).size();
    auto menu = buildMenu(selectedPath);
    QAction *rootAction = findAction(*menu.menu, QStringLiteral("Synology Drive"));
    if (!require(rootAction && rootAction->menu(), "KF6 loader did not discover the plugin") ||
        !require(findAction(*rootAction->menu(), QStringLiteral("Get link")), "missing Get link") ||
        !require(findAction(*rootAction->menu(), QStringLiteral("Browse previous versions")),
                 "missing history action") ||
        !require(pids(pidLog).size() == beforeFile + 1,
                 "eligible file did not start exactly one helper")) {
        return 1;
    }
    QTest::qWait(100);
    if (!require(pids(pidLog).size() == beforeFile + 1,
                 "eligible file retried its list helper")) {
        return 1;
    }

    auto actionMenu = buildPluginMenu(directPlugin, {QUrl::fromLocalFile(selectedPath)});
    QAction *actionRoot = findAction(*actionMenu, QStringLiteral("Synology Drive"));
    int successErrors = 0;
    QObject::connect(&directPlugin, &SynodriveFileItemActionPlugin::error,
                     [&successErrors](const QString &) { ++successErrors; });
    if (!require(!fs::exists(log), "listing activated a Synology action")) {
        return 1;
    }
    const QList<QString> actionLabels = {
        QStringLiteral("Get link"), QStringLiteral("Browse previous versions")
    };
    for (const QString &actionLabel : actionLabels) {
        const qsizetype beforeActivation = pids(pidLog).size();
        findAction(*actionRoot->menu(), actionLabel)->trigger();
        for (int i = 0; i < 100; ++i) {
            if (pids(pidLog).size() == beforeActivation + 1 &&
                application.findChildren<QProcess *>().isEmpty()) {
                break;
            }
            QTest::qWait(10);
        }
        const QList<qint64> afterActivation = pids(pidLog);
        if (!require(afterActivation.size() == beforeActivation + 1,
                     "activation did not start exactly one helper") ||
            !require(kill(static_cast<pid_t>(afterActivation.back()), 0) == -1 && errno == ESRCH,
                     "activation helper remains alive") ||
            !require(application.findChildren<QProcess *>().isEmpty(),
                     "activation process was not released")) {
            return 1;
        }
        QTest::qWait(100);
        if (!require(pids(pidLog).size() == afterActivation.size(),
                     "activation retried the helper")) {
            return 1;
        }
    }
    for (int i = 0; i < 100; ++i) {
        QFile logFile(QString::fromStdString(log.string()));
        if (logFile.open(QIODevice::ReadOnly) && logFile.readAll().count('\n') == 2) {
            break;
        }
        QTest::qWait(10);
    }
    QFile activationLog(QString::fromStdString(log.string()));
    if (!activationLog.open(QIODevice::ReadOnly)) {
        return 1;
    }
    const QByteArray activationOutput = activationLog.readAll();
    const QByteArray uri = QUrl::fromLocalFile(selectedPath).toEncoded();
    const QByteArray expectedActivation =
        "NautilusCloudStation::ShareLink " + uri + '\n' +
        "NautilusCloudStation::VersionBrowse " + uri + '\n';
    if (!require(activationOutput == expectedActivation,
                 "actions did not reach the exact private-provider callbacks") ||
        !require(successErrors == 0, "successful activation emitted an error")) {
        return 1;
    }

    qputenv("FAKE_CONTEXT_ACTIVATION_DELAY", "100");
    const qsizetype beforeTeardownActivation = pids(pidLog).size();
    {
        SynodriveFileItemActionPlugin transientPlugin(nullptr);
        auto transientMenu = buildPluginMenu(
            transientPlugin, {QUrl::fromLocalFile(selectedPath)});
        QAction *transientRoot = findAction(*transientMenu, QStringLiteral("Synology Drive"));
        findAction(*transientRoot->menu(), QStringLiteral("Get link"))->trigger();
    }
    for (int i = 0; i < 100; ++i) {
        QFile logFile(QString::fromStdString(log.string()));
        if (logFile.open(QIODevice::ReadOnly) && logFile.readAll().count('\n') == 3 &&
            application.findChildren<QProcess *>().isEmpty()) {
            break;
        }
        QTest::qWait(10);
    }
    qunsetenv("FAKE_CONTEXT_ACTIVATION_DELAY");
    const QList<qint64> afterTeardownActivation = pids(pidLog);
    QFile teardownLog(QString::fromStdString(log.string()));
    if (!require(teardownLog.open(QIODevice::ReadOnly), "cannot read teardown activation log") ||
        !require(teardownLog.readAll().endsWith(
                     "NautilusCloudStation::ShareLink " + uri + '\n'),
                 "activation stopped when the popup owner was destroyed") ||
        !require(afterTeardownActivation.size() == beforeTeardownActivation + 2,
                 "teardown case did not run one list and one activation helper") ||
        !require(kill(static_cast<pid_t>(afterTeardownActivation.back()), 0) == -1 && errno == ESRCH,
                 "teardown activation helper remains alive") ||
        !require(application.findChildren<QProcess *>().isEmpty(),
                 "teardown activation process was not released")) {
        return 1;
    }

    qputenv("FAKE_CONTEXT_ACTIVATION_DELAY", "100");
    qputenv("FAKE_CONTEXT_ACTIVATION_EXIT", "7");
    const qsizetype beforeTeardownFailure = pids(pidLog).size();
    {
        SynodriveFileItemActionPlugin transientPlugin(nullptr);
        auto transientMenu = buildPluginMenu(
            transientPlugin, {QUrl::fromLocalFile(selectedPath)});
        QAction *transientRoot = findAction(*transientMenu, QStringLiteral("Synology Drive"));
        findAction(*transientRoot->menu(), QStringLiteral("Get link"))->trigger();
    }
    for (int i = 0; i < 100 &&
         (!application.findChildren<QProcess *>().isEmpty() || errorDialogs().size() != 1); ++i) {
        QTest::qWait(10);
    }
    qunsetenv("FAKE_CONTEXT_ACTIVATION_DELAY");
    qunsetenv("FAKE_CONTEXT_ACTIVATION_EXIT");
    const QList<qint64> afterTeardownFailure = pids(pidLog);
    const QList<QMessageBox *> dialogs = errorDialogs();
    if (!require(afterTeardownFailure.size() == beforeTeardownFailure + 2,
                 "teardown failure did not run one list and one activation helper") ||
        !require(kill(static_cast<pid_t>(afterTeardownFailure.back()), 0) == -1 && errno == ESRCH,
                 "failed teardown activation helper remains alive") ||
        !require(application.findChildren<QProcess *>().isEmpty(),
                 "failed teardown activation process was not released") ||
        !require(dialogs.size() == 1 &&
                 dialogs.front()->text() == QStringLiteral("Synology Drive action failed"),
                 "failed teardown activation did not show one error")) {
        return 1;
    }
    dialogs.front()->close();
    QTest::qWait(100);
    if (!require(pids(pidLog).size() == afterTeardownFailure.size(),
                 "failed teardown activation retried the helper") ||
        !require(errorDialogs().isEmpty(), "teardown error dialog was not released")) {
        return 1;
    }

    const fs::path selectedDirectory = root / "selected-directory";
    fs::create_directory(selectedDirectory);
    const qsizetype beforeDirectory = pids(pidLog).size();
    auto directoryMenu = buildPluginMenu(directPlugin,
        {QUrl::fromLocalFile(QString::fromStdString(selectedDirectory.string()))});
    if (!require(findAction(*directoryMenu, QStringLiteral("Synology Drive")),
                 "eligible directory did not create a menu") ||
        !require(pids(pidLog).size() == beforeDirectory + 1,
                 "eligible directory did not start exactly one helper")) {
        return 1;
    }
    QTest::qWait(100);
    if (!require(pids(pidLog).size() == beforeDirectory + 1,
                 "eligible directory retried its list helper")) {
        return 1;
    }

    qputenv("FAKE_CONTEXT_MODE", "empty");
    auto emptyMenu = buildMenu(QString::fromStdString(selected.string()));
    if (!require(!findAction(*emptyMenu.menu, QStringLiteral("Synology Drive")),
                 "empty provider created a menu")) {
        return 1;
    }

    qputenv("FAKE_CONTEXT_MODE", "duplicate");
    auto duplicateMenu = buildPluginMenu(directPlugin, {QUrl::fromLocalFile(selectedPath)});
    if (!require(!findAction(*duplicateMenu, QStringLiteral("Synology Drive")),
                 "duplicate action set created a menu")) {
        return 1;
    }

    qputenv("FAKE_CONTEXT_MODE", "reverse");
    auto reverseMenu = buildPluginMenu(directPlugin, {QUrl::fromLocalFile(selectedPath)});
    QAction *reverseRoot = findAction(*reverseMenu, QStringLiteral("Synology Drive"));
    if (!require(reverseRoot && reverseRoot->menu() &&
                 reverseRoot->menu()->actions().size() == 2 &&
                 reverseRoot->menu()->actions()[0]->text() == QStringLiteral("Browse previous versions") &&
                 reverseRoot->menu()->actions()[1]->text() == QStringLiteral("Get link"),
                 "plugin did not preserve helper action order")) {
        return 1;
    }

    qputenv("FAKE_CONTEXT_MODE", "sleep");
    const qsizetype beforeTimeout = pids(pidLog).size();
    QElapsedTimer timer;
    timer.start();
    auto slowMenu = buildMenu(QString::fromStdString(selected.string()));
    if (!require(timer.elapsed() < 250, "lookup exceeded the external time budget") ||
        !require(!findAction(*slowMenu.menu, QStringLiteral("Synology Drive")),
                 "timed-out provider created a menu")) {
        return 1;
    }
    const QList<qint64> afterTimeout = pids(pidLog);
    if (!require(afterTimeout.size() == beforeTimeout + 1, "timeout started an unexpected helper set") ||
        !require(kill(static_cast<pid_t>(afterTimeout.back()), 0) == -1 && errno == ESRCH,
                 "timed-out helper remains alive")) {
        return 1;
    }
    QTest::qWait(100);
    if (!require(pids(pidLog).size() == afterTimeout.size(), "timeout retried the helper")) {
        return 1;
    }

    for (const char *mode : {"nonzero", "partial", "crash"}) {
        qputenv("FAKE_CONTEXT_MODE", mode);
        const qsizetype beforeFailure = pids(pidLog).size();
        auto failedList = buildMenu(selectedPath);
        const QList<qint64> afterFailure = pids(pidLog);
        if (!require(!findAction(*failedList.menu, QStringLiteral("Synology Drive")),
                     "failed list helper created a menu") ||
            !require(afterFailure.size() == beforeFailure + 1,
                     "failed list did not start exactly one helper") ||
            !require(kill(static_cast<pid_t>(afterFailure.back()), 0) == -1 && errno == ESRCH,
                     "failed list helper remains alive")) {
            return 1;
        }
    }

    qunsetenv("FAKE_CONTEXT_MODE");
    int nonzeroErrors = 0;
    QObject::connect(&directPlugin, &SynodriveFileItemActionPlugin::error,
                     [&nonzeroErrors](const QString &) { ++nonzeroErrors; });
    qputenv("FAKE_CONTEXT_ACTIVATION_EXIT", "7");
    for (int actionIndex = 0; actionIndex < actionLabels.size(); ++actionIndex) {
        const qsizetype beforeFailure = pids(pidLog).size();
        findAction(*actionRoot->menu(), actionLabels[actionIndex])->trigger();
        for (int i = 0; i < 100 &&
             (nonzeroErrors != actionIndex + 1 ||
              !application.findChildren<QProcess *>().isEmpty()); ++i) {
            QTest::qWait(10);
        }
        const QList<qint64> afterFailure = pids(pidLog);
        if (!require(nonzeroErrors == actionIndex + 1,
                     "nonzero activation did not emit one error") ||
            !require(afterFailure.size() == beforeFailure + 1,
                     "nonzero activation did not start exactly one helper") ||
            !require(kill(static_cast<pid_t>(afterFailure.back()), 0) == -1 && errno == ESRCH,
                     "nonzero activation helper remains alive") ||
            !require(application.findChildren<QProcess *>().isEmpty(),
                     "nonzero activation process was not released")) {
            return 1;
        }
        QTest::qWait(100);
        if (!require(pids(pidLog).size() == afterFailure.size(),
                     "nonzero activation retried the helper")) {
            return 1;
        }
    }
    qunsetenv("FAKE_CONTEXT_ACTIVATION_EXIT");

    const fs::perms helperPermissions = fs::status(argv[2]).permissions();
    int startErrors = 0;
    QObject::connect(&directPlugin, &SynodriveFileItemActionPlugin::error,
                     [&startErrors](const QString &) { ++startErrors; });
    fs::permissions(argv[2], fs::perms::none, fs::perm_options::replace);
    auto failedList = buildPluginMenu(directPlugin, {QUrl::fromLocalFile(selectedPath)});
    const qsizetype beforeStartFailures = pids(pidLog).size();
    for (int actionIndex = 0; actionIndex < actionLabels.size(); ++actionIndex) {
        findAction(*actionRoot->menu(), actionLabels[actionIndex])->trigger();
        for (int i = 0; i < 100 &&
             (startErrors != actionIndex + 1 ||
              !application.findChildren<QProcess *>().isEmpty()); ++i) {
            QTest::qWait(10);
        }
        if (!require(startErrors == actionIndex + 1,
                     "activation start failure did not emit one error") ||
            !require(application.findChildren<QProcess *>().isEmpty(),
                     "failed-start activation process was not released")) {
            fs::permissions(argv[2], helperPermissions, fs::perm_options::replace);
            return 1;
        }
        QTest::qWait(100);
        if (!require(startErrors == actionIndex + 1,
                     "failed-start activation retried or repeated its error") ||
            !require(pids(pidLog).size() == beforeStartFailures,
                     "failed-start activation later launched a helper") ||
            !require(application.findChildren<QProcess *>().isEmpty(),
                     "failed-start activation later retained a process")) {
            fs::permissions(argv[2], helperPermissions, fs::perm_options::replace);
            return 1;
        }
    }
    fs::permissions(argv[2], helperPermissions, fs::perm_options::replace);
    if (!require(!findAction(*failedList, QStringLiteral("Synology Drive")),
                 "list start failure created a menu") ||
        !require(pids(pidLog).size() == beforeStartFailures,
                 "start failure unexpectedly launched a helper")) {
        return 1;
    }

    QFile maps(QStringLiteral("/proc/self/maps"));
    if (!maps.open(QIODevice::ReadOnly)) {
        return 1;
    }
    if (!require(!maps.readAll().contains("plugin-cb-4.so"),
                 "Dolphin-equivalent host mapped the private provider")) {
        return 1;
    }

    return 0;
}
