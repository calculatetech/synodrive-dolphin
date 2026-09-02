#include "synodrive_fileitemaction_plugin.h"

#include <KFileItemListProperties>
#include <KPluginFactory>

#include <QAction>
#include <QCoreApplication>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QUrl>

namespace {

#ifndef SYNODRIVE_ACTION_PROGRAM
#error SYNODRIVE_ACTION_PROGRAM must name the installed helper
#endif

constexpr int kLookupTimeoutMs = 200;
constexpr int kKillTimeoutMs = 25;

void reportFailure(const QPointer<SynodriveFileItemActionPlugin> &plugin,
                   QProcess *process)
{
    if (process->property("synodrive-error-reported").toBool()) {
        return;
    }
    process->setProperty("synodrive-error-reported", true);
    if (plugin) {
        Q_EMIT plugin->error(QStringLiteral("Synology Drive action failed"));
        return;
    }

    auto *dialog = new QMessageBox(
        QMessageBox::Critical,
        QStringLiteral("Synology Drive"),
        QStringLiteral("Synology Drive action failed"),
        QMessageBox::Ok);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

} // namespace

QStringList synodrive::parseActionList(const QByteArray &output)
{
    if (output.isEmpty()) {
        return {};
    }
    if (output.contains('\r') || !output.endsWith('\n')) {
        return {};
    }

    QStringList result;
    const QList<QByteArray> lines = output.chopped(1).split('\n');
    for (const QByteArray &line : lines) {
        if ((line != "get-link" && line != "browse-versions") ||
            result.contains(QString::fromLatin1(line))) {
            return {};
        }
        result.append(QString::fromLatin1(line));
    }
    return result;
}

namespace {

QString label(const QString &action)
{
    return action == QStringLiteral("get-link")
        ? QStringLiteral("Get link")
        : QStringLiteral("Browse previous versions");
}

} // namespace

SynodriveFileItemActionPlugin::SynodriveFileItemActionPlugin(QObject *parent)
    : KAbstractFileItemActionPlugin(parent)
{
}

QList<QAction *> SynodriveFileItemActionPlugin::actions(
    const KFileItemListProperties &fileItemInfos,
    QWidget *parentWidget)
{
    const QList<QUrl> urls = fileItemInfos.urlList();
    if (urls.size() != 1 || !urls.front().isLocalFile()) {
        return {};
    }

    const QFileInfo info(urls.front().toLocalFile());
    const QString path = info.canonicalFilePath();
    if (path.isEmpty() || (!info.isFile() && !info.isDir())) {
        return {};
    }

    QProcess helper;
    helper.setProgram(QStringLiteral(SYNODRIVE_ACTION_PROGRAM));
    helper.setArguments({QStringLiteral("--list"), path});
    helper.start();
    if (!helper.waitForFinished(kLookupTimeoutMs)) {
        helper.kill();
        helper.waitForFinished(kKillTimeoutMs);
        return {};
    }
    if (helper.exitStatus() != QProcess::NormalExit || helper.exitCode() != 0) {
        return {};
    }

    const QStringList available = synodrive::parseActionList(helper.readAllStandardOutput());
    if (available.isEmpty()) {
        return {};
    }

    auto *menu = new QMenu(QStringLiteral("Synology Drive"), parentWidget);
    for (const QString &actionName : available) {
        QAction *action = menu->addAction(label(actionName));
        connect(action, &QAction::triggered, this, [this, actionName, path] {
            activate(actionName, path);
        });
    }
    return {menu->menuAction()};
}

void SynodriveFileItemActionPlugin::activate(const QString &action, const QString &path)
{
    auto *process = new QProcess(QCoreApplication::instance());
    const QPointer<SynodriveFileItemActionPlugin> plugin(this);
    process->setProgram(QStringLiteral(SYNODRIVE_ACTION_PROGRAM));
    process->setArguments({QStringLiteral("--activate"), action, path});
    connect(process, &QProcess::errorOccurred, process,
            [plugin, process](QProcess::ProcessError processError) {
                if (processError == QProcess::FailedToStart) {
                    reportFailure(plugin, process);
                    process->deleteLater();
                }
            });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
            [plugin, process](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    reportFailure(plugin, process);
                }
                process->deleteLater();
            });
    process->start();
}

K_PLUGIN_CLASS_WITH_JSON(SynodriveFileItemActionPlugin, "synodrive-fileitemaction.json")

#include "synodrive_fileitemaction_plugin.moc"
