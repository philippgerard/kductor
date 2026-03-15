#include "workspace.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

Workspace Workspace::create(const QString &name, const QString &repoPath,
                            const QString &worktreePath, const QString &branchName,
                            const QString &sourceBranch)
{
    Workspace ws;
    ws.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ws.name = name;
    ws.repoPath = repoPath;
    ws.worktreePath = worktreePath;
    ws.branchName = branchName;
    ws.sourceBranch = sourceBranch;
    ws.status = Active;
    ws.createdAt = QDateTime::currentDateTime();
    ws.updatedAt = ws.createdAt;
    return ws;
}

QJsonObject Workspace::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("repoPath")] = repoPath;
    obj[QStringLiteral("worktreePath")] = worktreePath;
    obj[QStringLiteral("branchName")] = branchName;
    obj[QStringLiteral("sourceBranch")] = sourceBranch;
    obj[QStringLiteral("status")] = status;
    obj[QStringLiteral("createdAt")] = createdAt.toString(Qt::ISODate);
    obj[QStringLiteral("updatedAt")] = updatedAt.toString(Qt::ISODate);

    QJsonArray agents;
    for (const auto &aid : agentIds)
        agents.append(aid);
    obj[QStringLiteral("agentIds")] = agents;

    return obj;
}

Workspace Workspace::fromJson(const QJsonObject &obj)
{
    Workspace ws;
    ws.id = obj[QStringLiteral("id")].toString();
    ws.name = obj[QStringLiteral("name")].toString();
    ws.repoPath = obj[QStringLiteral("repoPath")].toString();
    ws.worktreePath = obj[QStringLiteral("worktreePath")].toString();
    ws.branchName = obj[QStringLiteral("branchName")].toString();
    ws.sourceBranch = obj[QStringLiteral("sourceBranch")].toString();
    ws.status = obj[QStringLiteral("status")].toInt();
    ws.createdAt = QDateTime::fromString(obj[QStringLiteral("createdAt")].toString(), Qt::ISODate);
    ws.updatedAt = QDateTime::fromString(obj[QStringLiteral("updatedAt")].toString(), Qt::ISODate);

    const auto agents = obj[QStringLiteral("agentIds")].toArray();
    for (const auto &a : agents)
        ws.agentIds.append(a.toString());

    return ws;
}

// WorkspaceStore

WorkspaceStore::WorkspaceStore(QObject *parent)
    : QObject(parent)
{
}

QString WorkspaceStore::storagePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/workspaces.json");
}

QList<Workspace> WorkspaceStore::loadAll() const
{
    QList<Workspace> result;
    QFile file(storagePath());
    if (!file.open(QIODevice::ReadOnly))
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const auto arr = doc.array();
    for (const auto &val : arr) {
        result.append(Workspace::fromJson(val.toObject()));
    }
    return result;
}

void WorkspaceStore::saveAll(const QList<Workspace> &workspaces)
{
    QJsonArray arr;
    for (const auto &ws : workspaces)
        arr.append(ws.toJson());

    QFile file(storagePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
    }
}

void WorkspaceStore::addWorkspace(const Workspace &ws)
{
    auto all = loadAll();
    all.append(ws);
    saveAll(all);
}

void WorkspaceStore::updateWorkspace(const Workspace &ws)
{
    auto all = loadAll();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].id == ws.id) {
            all[i] = ws;
            break;
        }
    }
    saveAll(all);
}

void WorkspaceStore::removeWorkspace(const QString &id)
{
    auto all = loadAll();
    all.removeIf([&id](const Workspace &ws) { return ws.id == id; });
    saveAll(all);
}
