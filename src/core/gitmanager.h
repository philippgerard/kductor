#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <git2.h>

#include <memory>

struct GitRepoDeleter {
    void operator()(git_repository *repo) const { git_repository_free(repo); }
};
using GitRepoPtr = std::unique_ptr<git_repository, GitRepoDeleter>;

class GitManager : public QObject
{
    Q_OBJECT

public:
    explicit GitManager(QObject *parent = nullptr);
    ~GitManager() override;

    Q_INVOKABLE bool openRepository(const QString &path);
    Q_INVOKABLE void closeRepository();
    Q_INVOKABLE bool isOpen() const;
    Q_INVOKABLE QString repositoryPath() const;
    Q_INVOKABLE QString repositoryName() const;

    Q_INVOKABLE QStringList listBranches() const;
    Q_INVOKABLE QString currentBranch() const;

    Q_INVOKABLE bool createWorktree(const QString &name, const QString &path,
                                     const QString &branchName, const QString &sourceBranch);
    Q_INVOKABLE QStringList listWorktrees() const;
    Q_INVOKABLE bool removeWorktree(const QString &name);

    Q_INVOKABLE QVariantList getStatus() const;
    Q_INVOKABLE QVariantList getDiff() const;
    // mode: 0=all (source vs workdir), 1=committed (source vs HEAD), 2=pending (HEAD vs workdir)
    Q_INVOKABLE QVariantList getDetailedDiff(const QString &worktreePath, const QString &sourceBranch, int mode = 0) const;
    // Same computation off the GUI thread; result arrives via detailedDiffReady.
    Q_INVOKABLE void requestDetailedDiff(const QString &worktreePath, const QString &sourceBranch, int mode = 0);

    Q_INVOKABLE bool hasUncommittedChanges(const QString &worktreePath) const;

    git_repository *repo() const { return m_repo.get(); }
    QString lastError() const { return m_lastError; }

Q_SIGNALS:
    void repositoryChanged();
    void errorOccurred(const QString &message);
    // files = diff data, mode echoed back, ok = false when the worktree could
    // not be opened (so the UI can tell "error" from "no changes").
    void detailedDiffReady(const QVariantList &files, int mode, bool ok);

private:
    GitRepoPtr m_repo;
    QString m_repoPath;
    QString m_lastError;

    bool createBranch(const QString &name, const QString &fromRef);
    void setError(const QString &context);
};
