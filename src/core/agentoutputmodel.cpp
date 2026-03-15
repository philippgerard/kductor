#include "agentoutputmodel.h"

#include <QDateTime>

AgentOutputModel::AgentOutputModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AgentOutputModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_lines.size();
}

QVariant AgentOutputModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size())
        return {};

    const auto &line = m_lines.at(index.row());

    switch (role) {
    case ContentRole: return line.content;
    case LineTypeRole: return line.lineType;
    case ToolNameRole: return line.toolName;
    case TimestampRole: return line.timestamp;
    default: return {};
    }
}

QHash<int, QByteArray> AgentOutputModel::roleNames() const
{
    return {
        {ContentRole, "content"},
        {LineTypeRole, "lineType"},
        {ToolNameRole, "toolName"},
        {TimestampRole, "timestamp"},
    };
}

void AgentOutputModel::trimIfNeeded()
{
    if (m_lines.size() > MAX_LINES) {
        int toRemove = m_lines.size() - MAX_LINES;
        beginRemoveRows(QModelIndex(), 0, toRemove - 1);
        m_lines.remove(0, toRemove);
        endRemoveRows();
    }
}

void AgentOutputModel::appendText(const QString &text)
{
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size());
    m_lines.append({text, TextLine, {}, QDateTime::currentMSecsSinceEpoch()});
    endInsertRows();
    trimIfNeeded();
    Q_EMIT countChanged();
    Q_EMIT lineAppended();
}

void AgentOutputModel::appendThinking(const QString &text)
{
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size());
    m_lines.append({text, ThinkingLine, {}, QDateTime::currentMSecsSinceEpoch()});
    endInsertRows();
    trimIfNeeded();
    Q_EMIT countChanged();
    Q_EMIT lineAppended();
}

void AgentOutputModel::appendToolUse(const QString &toolName, const QString &summary)
{
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size());
    m_lines.append({summary, ToolUseLine, toolName, QDateTime::currentMSecsSinceEpoch()});
    endInsertRows();
    trimIfNeeded();
    Q_EMIT countChanged();
    Q_EMIT lineAppended();
}

void AgentOutputModel::appendToolResult(const QString &toolName, const QString &output)
{
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size());
    m_lines.append({output, ToolResultLine, toolName, QDateTime::currentMSecsSinceEpoch()});
    endInsertRows();
    trimIfNeeded();
    Q_EMIT countChanged();
    Q_EMIT lineAppended();
}

void AgentOutputModel::appendSystem(const QString &text)
{
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size());
    m_lines.append({text, SystemLine, {}, QDateTime::currentMSecsSinceEpoch()});
    endInsertRows();
    trimIfNeeded();
    Q_EMIT countChanged();
    Q_EMIT lineAppended();
}

void AgentOutputModel::appendError(const QString &text)
{
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size());
    m_lines.append({text, ErrorLine, {}, QDateTime::currentMSecsSinceEpoch()});
    endInsertRows();
    trimIfNeeded();
    Q_EMIT countChanged();
    Q_EMIT lineAppended();
}

void AgentOutputModel::clear()
{
    beginResetModel();
    m_lines.clear();
    endResetModel();
    Q_EMIT countChanged();
}
