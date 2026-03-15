#include "gitmanager.h"

#include <QDir>
#include <QFileInfo>

#include <git2.h>

GitManager::GitManager(QObject *parent)
    : QObject(parent)
{
    git_libgit2_init();
}

GitManager::~GitManager()
{
    closeRepository();
    git_libgit2_shutdown();
}

bool GitManager::openRepository(const QString &path)
{
    closeRepository();

    git_repository *repo = nullptr;
    int err = git_repository_open(&repo, path.toUtf8().constData());
    if (err < 0) {
        emitGitError(QStringLiteral("Failed to open repository"));
        return false;
    }

    m_repo.reset(repo);
    m_repoPath = path;
    Q_EMIT repositoryChanged();
    return true;
}

void GitManager::closeRepository()
{
    m_repo.reset();
    m_repoPath.clear();
}

bool GitManager::isOpen() const
{
    return m_repo != nullptr;
}

QString GitManager::repositoryPath() const
{
    return m_repoPath;
}

QString GitManager::repositoryName() const
{
    if (m_repoPath.isEmpty())
        return {};
    return QFileInfo(m_repoPath).fileName();
}

QStringList GitManager::listBranches() const
{
    QStringList branches;
    if (!m_repo)
        return branches;

    git_branch_iterator *iter = nullptr;
    if (git_branch_iterator_new(&iter, m_repo.get(), GIT_BRANCH_LOCAL) < 0)
        return branches;

    git_reference *ref = nullptr;
    git_branch_t type;
    while (git_branch_next(&ref, &type, iter) == 0) {
        const char *name = nullptr;
        if (git_branch_name(&name, ref) == 0) {
            branches.append(QString::fromUtf8(name));
        }
        git_reference_free(ref);
    }
    git_branch_iterator_free(iter);

    return branches;
}

QString GitManager::currentBranch() const
{
    if (!m_repo)
        return {};

    git_reference *head = nullptr;
    if (git_repository_head(&head, m_repo.get()) < 0)
        return {};

    const char *name = nullptr;
    git_branch_name(&name, head);
    QString branch = QString::fromUtf8(name);
    git_reference_free(head);
    return branch;
}

bool GitManager::createBranch(const QString &name, const QString &fromRef)
{
    if (!m_repo)
        return false;

    git_reference *ref = nullptr;
    git_object *target = nullptr;

    if (git_revparse_single(&target, m_repo.get(), fromRef.toUtf8().constData()) < 0) {
        emitGitError(QStringLiteral("Failed to resolve reference: %1").arg(fromRef));
        return false;
    }

    git_commit *commit = nullptr;
    if (git_commit_lookup(&commit, m_repo.get(), git_object_id(target)) < 0) {
        git_object_free(target);
        emitGitError(QStringLiteral("Failed to lookup commit"));
        return false;
    }

    int err = git_branch_create(&ref, m_repo.get(), name.toUtf8().constData(), commit, 0);
    git_commit_free(commit);
    git_object_free(target);

    if (err < 0) {
        emitGitError(QStringLiteral("Failed to create branch: %1").arg(name));
        return false;
    }

    git_reference_free(ref);
    return true;
}

bool GitManager::createWorktree(const QString &name, const QString &path, const QString &branchName)
{
    if (!m_repo)
        return false;

    QString sourceBranch = currentBranch();
    if (sourceBranch.isEmpty())
        sourceBranch = QStringLiteral("HEAD");

    if (!createBranch(branchName, sourceBranch))
        return false;

    git_reference *branchRef = nullptr;
    if (git_branch_lookup(&branchRef, m_repo.get(), branchName.toUtf8().constData(), GIT_BRANCH_LOCAL) < 0) {
        emitGitError(QStringLiteral("Failed to lookup created branch"));
        return false;
    }

    git_worktree_add_options opts = GIT_WORKTREE_ADD_OPTIONS_INIT;
    opts.ref = branchRef;

    git_worktree *wt = nullptr;
    int err = git_worktree_add(&wt, m_repo.get(), name.toUtf8().constData(),
                                path.toUtf8().constData(), &opts);
    git_reference_free(branchRef);

    if (err < 0) {
        emitGitError(QStringLiteral("Failed to create worktree"));
        return false;
    }

    git_worktree_free(wt);
    return true;
}

QStringList GitManager::listWorktrees() const
{
    QStringList result;
    if (!m_repo)
        return result;

    git_strarray worktrees = {nullptr, 0};
    if (git_worktree_list(&worktrees, m_repo.get()) < 0)
        return result;

    for (size_t i = 0; i < worktrees.count; ++i) {
        result.append(QString::fromUtf8(worktrees.strings[i]));
    }
    git_strarray_dispose(&worktrees);
    return result;
}

bool GitManager::removeWorktree(const QString &name)
{
    if (!m_repo)
        return false;

    git_worktree *wt = nullptr;
    if (git_worktree_lookup(&wt, m_repo.get(), name.toUtf8().constData()) < 0) {
        emitGitError(QStringLiteral("Failed to lookup worktree: %1").arg(name));
        return false;
    }

    git_worktree_prune_options opts = GIT_WORKTREE_PRUNE_OPTIONS_INIT;
    opts.flags = GIT_WORKTREE_PRUNE_VALID | GIT_WORKTREE_PRUNE_WORKING_TREE;

    int err = git_worktree_prune(wt, &opts);
    git_worktree_free(wt);

    if (err < 0) {
        emitGitError(QStringLiteral("Failed to prune worktree: %1").arg(name));
        return false;
    }

    return true;
}

QVariantList GitManager::getStatus() const
{
    QVariantList result;
    if (!m_repo)
        return result;

    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;

    git_status_list *statuses = nullptr;
    if (git_status_list_new(&statuses, m_repo.get(), &opts) < 0)
        return result;

    size_t count = git_status_list_entrycount(statuses);
    for (size_t i = 0; i < count; ++i) {
        const git_status_entry *entry = git_status_byindex(statuses, i);
        QVariantMap item;

        const char *path = entry->index_to_workdir
            ? entry->index_to_workdir->new_file.path
            : entry->head_to_index ? entry->head_to_index->new_file.path : nullptr;

        if (path) {
            item[QStringLiteral("path")] = QString::fromUtf8(path);
            item[QStringLiteral("status")] = static_cast<int>(entry->status);
            result.append(item);
        }
    }
    git_status_list_free(statuses);
    return result;
}

QVariantList GitManager::getDiff() const
{
    QVariantList result;
    if (!m_repo)
        return result;

    git_diff *diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;

    if (git_diff_index_to_workdir(&diff, m_repo.get(), nullptr, &opts) < 0)
        return result;

    size_t numDeltas = git_diff_num_deltas(diff);
    for (size_t i = 0; i < numDeltas; ++i) {
        const git_diff_delta *delta = git_diff_get_delta(diff, i);
        QVariantMap item;
        item[QStringLiteral("oldPath")] = QString::fromUtf8(delta->old_file.path);
        item[QStringLiteral("newPath")] = QString::fromUtf8(delta->new_file.path);
        item[QStringLiteral("status")] = static_cast<int>(delta->status);
        result.append(item);
    }

    git_diff_free(diff);
    return result;
}

void GitManager::emitGitError(const QString &context)
{
    const git_error *err = git_error_last();
    QString msg = context;
    if (err && err->message) {
        msg += QStringLiteral(": ") + QString::fromUtf8(err->message);
    }
    Q_EMIT errorOccurred(msg);
}
