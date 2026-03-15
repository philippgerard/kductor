#include "workspacemodel.h"
#include "worktreemanager.h"

WorkspaceModel::WorkspaceModel(WorktreeManager *worktreeManager, QObject *parent)
    : QAbstractListModel(parent)
    , m_worktreeManager(worktreeManager)
{
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
            beginRemoveRows(QModelIndex(), i, i);
            m_workspaces.removeAt(i);
            m_store.removeWorkspace(id);
            endRemoveRows();
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
    for (const auto &ws : m_workspaces) {
        if (!result.contains(ws.repoPath))
            result.append(ws.repoPath);
    }
    return result;
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
    endInsertRows();
    Q_EMIT countChanged();
}
