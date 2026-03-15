#pragma once

#include <QAbstractListModel>
#include "workspace.h"

class WorktreeManager;

class WorkspaceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        RepoPathRole,
        WorktreePathRole,
        BranchNameRole,
        SourceBranchRole,
        StatusRole,
        CreatedAtRole,
        AgentCountRole,
    };
    Q_ENUM(Roles)

    explicit WorkspaceModel(WorktreeManager *worktreeManager, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_workspaces.size(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE QVariantMap getById(const QString &id) const;
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE void updateStatus(const QString &id, int status);
    Q_INVOKABLE QStringList uniqueRepoPaths() const;
    Q_INVOKABLE QVariantList workspacesForRepo(const QString &repoPath) const;
    Q_INVOKABLE void addRepo(const QString &repoPath);
    Q_INVOKABLE void removeRepo(const QString &repoPath);

    void addWorkspace(const Workspace &ws);

Q_SIGNALS:
    void countChanged();

private:
    QList<Workspace> m_workspaces;
    QStringList m_standaloneRepos;
    WorkspaceStore m_store;
    WorktreeManager *m_worktreeManager;
    void loadRepos();
    void saveRepos();
};
