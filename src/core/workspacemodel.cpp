#include "workspacemodel.h"
#include "worktreemanager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

WorkspaceModel::WorkspaceModel(WorktreeManager *worktreeManager, QObject *parent)
    : QAbstractListModel(parent)
    , m_worktreeManager(worktreeManager)
{
    loadRepos();
    refresh();
}

int WorkspaceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_workspaces.size();
}

QVariant WorkspaceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_workspaces.size())
        return {};

    const auto &ws = m_workspaces.at(index.row());

    switch (role) {
    case IdRole: return ws.id;
    case NameRole: return ws.name;
    case RepoPathRole: return ws.repoPath;
    case WorktreePathRole: return ws.worktreePath;
    case BranchNameRole: return ws.branchName;
    case SourceBranchRole: return ws.sourceBranch;
    case StatusRole: return ws.status;
    case CreatedAtRole: return ws.createdAt;
    case AgentCountRole: return ws.agentIds.size();
    default: return {};
    }
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const
{
    return {
        {IdRole, "workspaceId"},
        {NameRole, "name"},
        {RepoPathRole, "repoPath"},
        {WorktreePathRole, "worktreePath"},
        {BranchNameRole, "branchName"},
        {SourceBranchRole, "sourceBranch"},
        {StatusRole, "status"},
        {CreatedAtRole, "createdAt"},
        {AgentCountRole, "agentCount"},
    };
}

void WorkspaceModel::refresh()
{
    beginResetModel();
    m_workspaces = m_store.loadAll();
    endResetModel();
    Q_EMIT countChanged();
}

QVariantMap WorkspaceModel::get(int index) const
{
    if (index < 0 || index >= m_workspaces.size())
        return {};

    const auto &ws = m_workspaces.at(index);
    return {
        {QStringLiteral("id"), ws.id},
        {QStringLiteral("name"), ws.name},
        {QStringLiteral("repoPath"), ws.repoPath},
        {QStringLiteral("worktreePath"), ws.worktreePath},
        {QStringLiteral("branchName"), ws.branchName},
        {QStringLiteral("sourceBranch"), ws.sourceBranch},
        {QStringLiteral("status"), ws.status},
        {QStringLiteral("createdAt"), ws.createdAt},
        {QStringLiteral("agentCount"), ws.agentIds.size()},
    };
}

QVariantMap WorkspaceModel::getById(const QString &id) const
{
    for (int i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces[i].id == id)
            return get(i);
    }
    return {};
}

void WorkspaceModel::remove(const QString &id)
{
    for (int i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces[i].id == id) {
            QString repo = m_workspaces[i].repoPath;
            beginRemoveRows(QModelIndex(), i, i);
            m_workspaces.removeAt(i);
            m_store.removeWorkspace(id);
            endRemoveRows();
            // Keep the repo in sidebar if this was the last workspace
            addRepo(repo);
            Q_EMIT countChanged();
            return;
        }
    }
}

void WorkspaceModel::updateStatus(const QString &id, int status)
{
    for (int i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces[i].id == id) {
            m_workspaces[i].status = status;
            m_workspaces[i].updatedAt = QDateTime::currentDateTime();
            m_store.updateWorkspace(m_workspaces[i]);
            Q_EMIT dataChanged(index(i), index(i), {StatusRole});
            return;
        }
    }
}

QStringList WorkspaceModel::uniqueRepoPaths() const
{
    QStringList result;
    // Include standalone repos first
    for (const auto &repo : m_standaloneRepos) {
        if (!result.contains(repo))
            result.append(repo);
    }
    // Then repos from workspaces
    for (const auto &ws : m_workspaces) {
        if (!result.contains(ws.repoPath))
            result.append(ws.repoPath);
    }
    return result;
}

static QString reposFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/repos.json");
}

void WorkspaceModel::loadRepos()
{
    QFile file(reposFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    for (const auto &v : arr)
        m_standaloneRepos.append(v.toString());
}

void WorkspaceModel::saveRepos()
{
    QJsonArray arr;
    for (const auto &r : m_standaloneRepos)
        arr.append(r);
    QFile file(reposFilePath());
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void WorkspaceModel::addRepo(const QString &repoPath)
{
    if (!m_standaloneRepos.contains(repoPath)) {
        m_standaloneRepos.append(repoPath);
        saveRepos();
        Q_EMIT countChanged(); // triggers sidebar rebuild
    }
}

void WorkspaceModel::removeRepo(const QString &repoPath)
{
    m_standaloneRepos.removeAll(repoPath);
    saveRepos();
    Q_EMIT countChanged();
}

QVariantList WorkspaceModel::workspacesForRepo(const QString &repoPath) const
{
    QVariantList result;
    for (int i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces[i].repoPath == repoPath) {
            result.append(get(i));
        }
    }
    return result;
}

void WorkspaceModel::addWorkspace(const Workspace &ws)
{
    beginInsertRows(QModelIndex(), m_workspaces.size(), m_workspaces.size());
    m_workspaces.append(ws);
    m_store.addWorkspace(ws);
    m_lastAddedId = ws.id;
    endInsertRows();
    Q_EMIT countChanged();
}
