#pragma once

#include <KAbstractFileItemActionPlugin>

#include <QStringList>

class QProcess;
class QByteArray;

namespace synodrive {
QStringList parseActionList(const QByteArray &output);
}

class SynodriveFileItemActionPlugin final : public KAbstractFileItemActionPlugin
{
    Q_OBJECT

public:
    explicit SynodriveFileItemActionPlugin(QObject *parent);
    QList<QAction *> actions(const KFileItemListProperties &fileItemInfos,
                             QWidget *parentWidget) override;

private:
    void activate(const QString &action, const QString &path);

};
