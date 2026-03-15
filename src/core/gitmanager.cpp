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
        setError(QStringLiteral("Failed to open repository"));
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
        setError(QStringLiteral("Failed to resolve branch '%1'").arg(fromRef));
        return false;
    }

    git_commit *commit = nullptr;
    if (git_commit_lookup(&commit, m_repo.get(), git_object_id(target)) < 0) {
        git_object_free(target);
        setError(QStringLiteral("Failed to lookup commit for '%1'").arg(fromRef));
        return false;
    }

    int err = git_branch_create(&ref, m_repo.get(), name.toUtf8().constData(), commit, 0);
    git_commit_free(commit);
    git_object_free(target);

    if (err < 0) {
        setError(QStringLiteral("Failed to create branch '%1'").arg(name));
        return false;
    }

    git_reference_free(ref);
    return true;
}

bool GitManager::createWorktree(const QString &name, const QString &path,
                                const QString &branchName, const QString &sourceBranch)
{
    if (!m_repo)
        return false;

    // Use provided source branch, fall back to current branch or HEAD
    QString base = sourceBranch;
    if (base.isEmpty()) {
        base = currentBranch();
        if (base.isEmpty())
            base = QStringLiteral("HEAD");
    }

    if (!createBranch(branchName, base))
        return false;

    git_reference *branchRef = nullptr;
    if (git_branch_lookup(&branchRef, m_repo.get(), branchName.toUtf8().constData(), GIT_BRANCH_LOCAL) < 0) {
        setError(QStringLiteral("Failed to lookup created branch '%1'").arg(branchName));
        return false;
    }

    git_worktree_add_options opts = GIT_WORKTREE_ADD_OPTIONS_INIT;
    opts.ref = branchRef;

    git_worktree *wt = nullptr;
    int err = git_worktree_add(&wt, m_repo.get(), name.toUtf8().constData(),
                                path.toUtf8().constData(), &opts);
    git_reference_free(branchRef);

    if (err < 0) {
        setError(QStringLiteral("Failed to add worktree at '%1'").arg(path));
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
        setError(QStringLiteral("Failed to lookup worktree: %1").arg(name));
        return false;
    }

    git_worktree_prune_options opts = GIT_WORKTREE_PRUNE_OPTIONS_INIT;
    opts.flags = GIT_WORKTREE_PRUNE_VALID | GIT_WORKTREE_PRUNE_WORKING_TREE;

    int err = git_worktree_prune(wt, &opts);
    git_worktree_free(wt);

    if (err < 0) {
        setError(QStringLiteral("Failed to prune worktree: %1").arg(name));
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

// Callback payload for git_diff_foreach
struct DiffCollector {
    QVariantList files;
    QVariantList currentHunks;
    QVariantList currentLines;
};

static int diffFileCb(const git_diff_delta *delta, float, void *payload)
{
    auto *dc = static_cast<DiffCollector *>(payload);

    // Flush previous file's last hunk
    if (!dc->currentLines.isEmpty()) {
        if (!dc->currentHunks.isEmpty()) {
            QVariantMap lastHunk = dc->currentHunks.last().toMap();
            lastHunk[QStringLiteral("lines")] = dc->currentLines;
            dc->currentHunks.last() = lastHunk;
        }
        dc->currentLines.clear();
    }

    // Flush previous file
    if (!dc->currentHunks.isEmpty() && !dc->files.isEmpty()) {
        QVariantMap lastFile = dc->files.last().toMap();
        lastFile[QStringLiteral("hunks")] = dc->currentHunks;
        dc->files.last() = lastFile;
    }
    dc->currentHunks.clear();

    QString statusLabel;
    switch (delta->status) {
    case GIT_DELTA_ADDED:      statusLabel = QStringLiteral("Added"); break;
    case GIT_DELTA_DELETED:    statusLabel = QStringLiteral("Deleted"); break;
    case GIT_DELTA_MODIFIED:   statusLabel = QStringLiteral("Modified"); break;
    case GIT_DELTA_RENAMED:    statusLabel = QStringLiteral("Renamed"); break;
    case GIT_DELTA_COPIED:     statusLabel = QStringLiteral("Copied"); break;
    default:                   statusLabel = QStringLiteral("Changed"); break;
    }

    QVariantMap file;
    file[QStringLiteral("oldPath")] = QString::fromUtf8(delta->old_file.path);
    file[QStringLiteral("newPath")] = QString::fromUtf8(delta->new_file.path);
    file[QStringLiteral("status")] = static_cast<int>(delta->status);
    file[QStringLiteral("statusLabel")] = statusLabel;
    file[QStringLiteral("hunks")] = QVariantList();
    dc->files.append(file);

    return 0;
}

static int diffHunkCb(const git_diff_delta *, const git_diff_hunk *hunk, void *payload)
{
    auto *dc = static_cast<DiffCollector *>(payload);

    // Flush previous hunk's lines
    if (!dc->currentLines.isEmpty() && !dc->currentHunks.isEmpty()) {
        QVariantMap lastHunk = dc->currentHunks.last().toMap();
        lastHunk[QStringLiteral("lines")] = dc->currentLines;
        dc->currentHunks.last() = lastHunk;
    }
    dc->currentLines.clear();

    QVariantMap h;
    h[QStringLiteral("header")] = QString::fromUtf8(hunk->header, static_cast<int>(hunk->header_len)).trimmed();
    h[QStringLiteral("oldStart")] = hunk->old_start;
    h[QStringLiteral("oldCount")] = static_cast<int>(hunk->old_lines);
    h[QStringLiteral("newStart")] = hunk->new_start;
    h[QStringLiteral("newCount")] = static_cast<int>(hunk->new_lines);
    h[QStringLiteral("lines")] = QVariantList();
    dc->currentHunks.append(h);

    return 0;
}

static int diffLineCb(const git_diff_delta *, const git_diff_hunk *, const git_diff_line *line, void *payload)
{
    auto *dc = static_cast<DiffCollector *>(payload);

    QString type;
    switch (line->origin) {
    case GIT_DIFF_LINE_ADDITION:  type = QStringLiteral("add"); break;
    case GIT_DIFF_LINE_DELETION:  type = QStringLiteral("del"); break;
    case GIT_DIFF_LINE_CONTEXT:   type = QStringLiteral("ctx"); break;
    default: return 0; // Skip file/hunk headers, binary notices
    }

    QVariantMap l;
    l[QStringLiteral("type")] = type;
    l[QStringLiteral("content")] = QString::fromUtf8(line->content, static_cast<int>(line->content_len)).chopped(
        (line->content_len > 0 && line->content[line->content_len - 1] == '\n') ? 1 : 0);
    l[QStringLiteral("oldLine")] = line->old_lineno;
    l[QStringLiteral("newLine")] = line->new_lineno;
    dc->currentLines.append(l);

    return 0;
}

QVariantList GitManager::getDetailedDiff(const QString &worktreePath, const QString &sourceBranch, int mode) const
{
    // mode: 0 = all (source vs workdir), 1 = committed (source vs HEAD), 2 = pending (HEAD vs workdir)

    git_repository *wtRepo = nullptr;
    if (git_repository_open(&wtRepo, worktreePath.toUtf8().constData()) < 0)
        return {};

    git_diff *diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
    opts.flags = GIT_DIFF_INCLUDE_UNTRACKED;
    opts.context_lines = 3;

    int err = -1;

    if (mode == 2) {
        // Pending: HEAD vs working directory (uncommitted changes only)
        err = git_diff_index_to_workdir(&diff, wtRepo, nullptr, &opts);
    } else {
        // Resolve source branch tree for modes 0 and 1
        git_object *sourceObj = nullptr;
        git_commit *sourceCommit = nullptr;
        git_tree *sourceTree = nullptr;
        bool haveSourceTree = false;

        QString ref = sourceBranch.isEmpty() ? QStringLiteral("HEAD") : sourceBranch;
        if (git_revparse_single(&sourceObj, wtRepo, ref.toUtf8().constData()) == 0) {
            if (git_commit_lookup(&sourceCommit, wtRepo, git_object_id(sourceObj)) == 0) {
                if (git_commit_tree(&sourceTree, sourceCommit) == 0) {
                    haveSourceTree = true;
                }
            }
        }

        if (haveSourceTree) {
            if (mode == 1) {
                // Committed: source tree vs HEAD tree
                git_reference *headRef = nullptr;
                git_commit *headCommit = nullptr;
                git_tree *headTree = nullptr;
                if (git_repository_head(&headRef, wtRepo) == 0) {
                    if (git_commit_lookup(&headCommit, wtRepo, git_reference_target(headRef)) == 0) {
                        if (git_commit_tree(&headTree, headCommit) == 0) {
                            err = git_diff_tree_to_tree(&diff, wtRepo, sourceTree, headTree, &opts);
                            git_tree_free(headTree);
                        }
                        git_commit_free(headCommit);
                    }
                    git_reference_free(headRef);
                }
            } else {
                // All: source tree vs working directory
                err = git_diff_tree_to_workdir_with_index(&diff, wtRepo, sourceTree, &opts);
            }
        } else if (mode == 0) {
            // Fallback for mode 0 when source branch not found
            err = git_diff_index_to_workdir(&diff, wtRepo, nullptr, &opts);
        }

        if (sourceTree) git_tree_free(sourceTree);
        if (sourceCommit) git_commit_free(sourceCommit);
        if (sourceObj) git_object_free(sourceObj);
    }

    if (err < 0) {
        git_repository_free(wtRepo);
        return {};
    }

    // Detect renames
    git_diff_find_options findOpts = GIT_DIFF_FIND_OPTIONS_INIT;
    findOpts.flags = GIT_DIFF_FIND_RENAMES;
    git_diff_find_similar(diff, &findOpts);

    // Collect all data via callbacks
    DiffCollector collector;
    git_diff_foreach(diff, diffFileCb, nullptr, diffHunkCb, diffLineCb, &collector);

    // Flush the last file's last hunk
    if (!collector.currentLines.isEmpty() && !collector.currentHunks.isEmpty()) {
        QVariantMap lastHunk = collector.currentHunks.last().toMap();
        lastHunk[QStringLiteral("lines")] = collector.currentLines;
        collector.currentHunks.last() = lastHunk;
    }
    if (!collector.currentHunks.isEmpty() && !collector.files.isEmpty()) {
        QVariantMap lastFile = collector.files.last().toMap();
        lastFile[QStringLiteral("hunks")] = collector.currentHunks;
        collector.files.last() = lastFile;
    }

    git_diff_free(diff);
    git_repository_free(wtRepo);
    return collector.files;
}

void GitManager::setError(const QString &context)
{
    const git_error *err = git_error_last();
    m_lastError = context;
    if (err && err->message) {
        m_lastError += QStringLiteral(": ") + QString::fromUtf8(err->message);
    }
    Q_EMIT errorOccurred(m_lastError);
}
