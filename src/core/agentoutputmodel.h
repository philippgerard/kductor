#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QString>
#include <QList>

class AgentOutputModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum LineType {
        TextLine = 0,
        ThinkingLine,
        ToolUseLine,
        ToolResultLine,
        SystemLine,
        ErrorLine
    };
    Q_ENUM(LineType)

    enum Roles {
        ContentRole = Qt::UserRole + 1,
        LineTypeRole,
        ToolNameRole,
        TimestampRole,
    };
    Q_ENUM(Roles)

    explicit AgentOutputModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_lines.size(); }

    Q_INVOKABLE void appendText(const QString &text);
    Q_INVOKABLE void appendThinking(const QString &text);
    Q_INVOKABLE void appendToolUse(const QString &toolName, const QString &summary);
    Q_INVOKABLE void appendToolResult(const QString &toolName, const QString &output);
    Q_INVOKABLE void appendSystem(const QString &text);
    Q_INVOKABLE void appendError(const QString &text);
    Q_INVOKABLE void clear();

    QJsonArray toJson() const;
    void loadFromJson(const QJsonArray &arr);

Q_SIGNALS:
    void countChanged();
    void lineAppended();

private:
    struct OutputLine {
        QString content;
        int lineType;
        QString toolName;
        qint64 timestamp;
    };

    QList<OutputLine> m_lines;
    static constexpr int MAX_LINES = 10000;

    void trimIfNeeded();
};
