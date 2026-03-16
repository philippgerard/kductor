#include <QTest>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonObject>

#include "core/agentoutputmodel.h"

class AgentOutputModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInitialState()
    {
        AgentOutputModel model;
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.count(), 0);
    }

    void testAppendText()
    {
        AgentOutputModel model;
        QSignalSpy countSpy(&model, &AgentOutputModel::countChanged);
        QSignalSpy lineSpy(&model, &AgentOutputModel::lineAppended);

        model.appendText(QStringLiteral("hello world"));

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(countSpy.count(), 1);
        QCOMPARE(lineSpy.count(), 1);

        auto idx = model.index(0);
        QCOMPARE(model.data(idx, AgentOutputModel::ContentRole).toString(), QStringLiteral("hello world"));
        QCOMPARE(model.data(idx, AgentOutputModel::LineTypeRole).toInt(), static_cast<int>(AgentOutputModel::TextLine));
        QVERIFY(model.data(idx, AgentOutputModel::ToolNameRole).toString().isEmpty());
        QVERIFY(model.data(idx, AgentOutputModel::TimestampRole).toLongLong() > 0);
    }

    void testAppendAllTypes()
    {
        AgentOutputModel model;

        model.appendText(QStringLiteral("text"));
        model.appendThinking(QStringLiteral("thought"));
        model.appendToolUse(QStringLiteral("Bash"), QStringLiteral("running ls"));
        model.appendToolResult(QStringLiteral("Bash"), QStringLiteral("file1\nfile2"));
        model.appendSystem(QStringLiteral("system msg"));
        model.appendError(QStringLiteral("error msg"));

        QCOMPARE(model.rowCount(), 6);

        QCOMPARE(model.data(model.index(0), AgentOutputModel::LineTypeRole).toInt(),
                 static_cast<int>(AgentOutputModel::TextLine));
        QCOMPARE(model.data(model.index(1), AgentOutputModel::LineTypeRole).toInt(),
                 static_cast<int>(AgentOutputModel::ThinkingLine));
        QCOMPARE(model.data(model.index(2), AgentOutputModel::LineTypeRole).toInt(),
                 static_cast<int>(AgentOutputModel::ToolUseLine));
        QCOMPARE(model.data(model.index(3), AgentOutputModel::LineTypeRole).toInt(),
                 static_cast<int>(AgentOutputModel::ToolResultLine));
        QCOMPARE(model.data(model.index(4), AgentOutputModel::LineTypeRole).toInt(),
                 static_cast<int>(AgentOutputModel::SystemLine));
        QCOMPARE(model.data(model.index(5), AgentOutputModel::LineTypeRole).toInt(),
                 static_cast<int>(AgentOutputModel::ErrorLine));
    }

    void testToolUsePreservesToolName()
    {
        AgentOutputModel model;

        model.appendToolUse(QStringLiteral("Read"), QStringLiteral("reading file.cpp"));
        model.appendToolResult(QStringLiteral("Write"), QStringLiteral("wrote output"));

        QCOMPARE(model.data(model.index(0), AgentOutputModel::ToolNameRole).toString(),
                 QStringLiteral("Read"));
        QCOMPARE(model.data(model.index(0), AgentOutputModel::ContentRole).toString(),
                 QStringLiteral("reading file.cpp"));

        QCOMPARE(model.data(model.index(1), AgentOutputModel::ToolNameRole).toString(),
                 QStringLiteral("Write"));
    }

    void testClear()
    {
        AgentOutputModel model;
        model.appendText(QStringLiteral("a"));
        model.appendText(QStringLiteral("b"));
        QCOMPARE(model.rowCount(), 2);

        QSignalSpy countSpy(&model, &AgentOutputModel::countChanged);
        model.clear();

        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.count(), 0);
        QCOMPARE(countSpy.count(), 1);
    }

    void testJsonRoundTrip()
    {
        AgentOutputModel model;
        model.appendText(QStringLiteral("line one"));
        model.appendThinking(QStringLiteral("deep thought"));
        model.appendToolUse(QStringLiteral("Bash"), QStringLiteral("ls -la"));
        model.appendError(QStringLiteral("something broke"));

        QJsonArray json = model.toJson();
        QCOMPARE(json.size(), 4);

        // Verify JSON structure
        QJsonObject first = json[0].toObject();
        QCOMPARE(first[QStringLiteral("c")].toString(), QStringLiteral("line one"));
        QCOMPARE(first[QStringLiteral("t")].toInt(), static_cast<int>(AgentOutputModel::TextLine));
        QVERIFY(!first.contains(QStringLiteral("n"))); // no tool name for text

        QJsonObject third = json[2].toObject();
        QCOMPARE(third[QStringLiteral("n")].toString(), QStringLiteral("Bash"));

        // Load into fresh model
        AgentOutputModel model2;
        model2.loadFromJson(json);

        QCOMPARE(model2.rowCount(), 4);
        QCOMPARE(model2.data(model2.index(0), AgentOutputModel::ContentRole).toString(),
                 QStringLiteral("line one"));
        QCOMPARE(model2.data(model2.index(1), AgentOutputModel::LineTypeRole).toInt(),
                 static_cast<int>(AgentOutputModel::ThinkingLine));
        QCOMPARE(model2.data(model2.index(2), AgentOutputModel::ToolNameRole).toString(),
                 QStringLiteral("Bash"));
        QCOMPARE(model2.data(model2.index(3), AgentOutputModel::ContentRole).toString(),
                 QStringLiteral("something broke"));
    }

    void testLoadFromJsonClearsPrevious()
    {
        AgentOutputModel model;
        model.appendText(QStringLiteral("old"));
        model.appendText(QStringLiteral("data"));

        QJsonArray json;
        QJsonObject obj;
        obj[QStringLiteral("c")] = QStringLiteral("new");
        obj[QStringLiteral("t")] = static_cast<int>(AgentOutputModel::TextLine);
        json.append(obj);

        model.loadFromJson(json);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), AgentOutputModel::ContentRole).toString(),
                 QStringLiteral("new"));
    }

    void testTrimAtMaxLines()
    {
        AgentOutputModel model;
        // Insert MAX_LINES + 50 entries
        for (int i = 0; i < 10050; ++i) {
            model.appendText(QStringLiteral("line %1").arg(i));
        }

        QVERIFY(model.rowCount() <= 10000);
        // The oldest lines should have been trimmed — the first line should not be "line 0"
        QVERIFY(model.data(model.index(0), AgentOutputModel::ContentRole).toString() != QStringLiteral("line 0"));
    }

    void testInvalidIndex()
    {
        AgentOutputModel model;
        model.appendText(QStringLiteral("only one"));

        QVERIFY(!model.data(model.index(-1), AgentOutputModel::ContentRole).isValid());
        QVERIFY(!model.data(model.index(1), AgentOutputModel::ContentRole).isValid());
        QVERIFY(!model.data(model.index(999), AgentOutputModel::ContentRole).isValid());
    }

    void testInvalidRole()
    {
        AgentOutputModel model;
        model.appendText(QStringLiteral("test"));

        QVERIFY(!model.data(model.index(0), Qt::DisplayRole).isValid());
        QVERIFY(!model.data(model.index(0), 9999).isValid());
    }

    void testRoleNames()
    {
        AgentOutputModel model;
        auto roles = model.roleNames();

        QVERIFY(roles.contains(AgentOutputModel::ContentRole));
        QVERIFY(roles.contains(AgentOutputModel::LineTypeRole));
        QVERIFY(roles.contains(AgentOutputModel::ToolNameRole));
        QVERIFY(roles.contains(AgentOutputModel::TimestampRole));

        QCOMPARE(roles[AgentOutputModel::ContentRole], QByteArray("content"));
        QCOMPARE(roles[AgentOutputModel::LineTypeRole], QByteArray("lineType"));
    }
};

QTEST_GUILESS_MAIN(AgentOutputModelTest)
#include "agentoutputmodeltest.moc"
