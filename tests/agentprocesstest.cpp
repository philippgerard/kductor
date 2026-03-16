#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "core/agentprocess.h"

class AgentProcessTest : public QObject
{
    Q_OBJECT

private:
    // Helper: feed a JSON line through parseLine
    void feed(AgentProcess &ap, const QJsonObject &obj)
    {
        ap.parseLine(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }

    QJsonObject makeSystemInit(const QString &sessionId = QStringLiteral("sess-123"))
    {
        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("system");
        obj[QStringLiteral("subtype")] = QStringLiteral("init");
        obj[QStringLiteral("session_id")] = sessionId;
        return obj;
    }

    QJsonObject makeAssistant(const QJsonArray &content)
    {
        QJsonObject message;
        message[QStringLiteral("content")] = content;

        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("assistant");
        obj[QStringLiteral("message")] = message;
        return obj;
    }

    QJsonObject makeTextBlock(const QString &text)
    {
        QJsonObject b;
        b[QStringLiteral("type")] = QStringLiteral("text");
        b[QStringLiteral("text")] = text;
        return b;
    }

    QJsonObject makeThinkingBlock(const QString &thought)
    {
        QJsonObject b;
        b[QStringLiteral("type")] = QStringLiteral("thinking");
        b[QStringLiteral("thinking")] = thought;
        return b;
    }

    QJsonObject makeToolUseBlock(const QString &name, const QJsonObject &input)
    {
        QJsonObject b;
        b[QStringLiteral("type")] = QStringLiteral("tool_use");
        b[QStringLiteral("name")] = name;
        b[QStringLiteral("input")] = input;
        return b;
    }

    QJsonObject makeToolResultBlock(const QString &name, const QString &content)
    {
        QJsonObject b;
        b[QStringLiteral("type")] = QStringLiteral("tool_result");
        b[QStringLiteral("name")] = name;
        b[QStringLiteral("content")] = content;
        return b;
    }

    QJsonObject makeResult(double cost, int inputTokens = 100, int outputTokens = 50,
                           int cacheRead = 10, int cacheCreate = 5, int contextWindow = 200000)
    {
        QJsonObject usage;
        usage[QStringLiteral("inputTokens")] = inputTokens;
        usage[QStringLiteral("outputTokens")] = outputTokens;
        usage[QStringLiteral("cacheReadInputTokens")] = cacheRead;
        usage[QStringLiteral("cacheCreationInputTokens")] = cacheCreate;
        usage[QStringLiteral("contextWindow")] = contextWindow;

        QJsonObject modelUsage;
        modelUsage[QStringLiteral("claude-sonnet-4-20250514")] = usage;

        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("result");
        obj[QStringLiteral("total_cost_usd")] = cost;
        obj[QStringLiteral("modelUsage")] = modelUsage;
        return obj;
    }

private Q_SLOTS:
    void testParseSystemInit()
    {
        AgentProcess ap;
        QSignalSpy initSpy(&ap, &AgentProcess::initialized);
        QSignalSpy sessionSpy(&ap, &AgentProcess::sessionIdChanged);
        QSignalSpy statusSpy(&ap, &AgentProcess::statusChanged);

        feed(ap, makeSystemInit(QStringLiteral("my-session")));

        QCOMPARE(initSpy.count(), 1);
        QCOMPARE(sessionSpy.count(), 1);
        QCOMPARE(ap.sessionId(), QStringLiteral("my-session"));
        QCOMPARE(ap.status(), static_cast<int>(AgentProcess::Running));
        QCOMPARE(ap.currentActivity(), QStringLiteral("Initialized"));
    }

    void testParseAssistantText()
    {
        AgentProcess ap;
        QSignalSpy textSpy(&ap, &AgentProcess::assistantText);
        QSignalSpy actSpy(&ap, &AgentProcess::activityChanged);

        QJsonArray content;
        content.append(makeTextBlock(QStringLiteral("Hello, world!")));
        feed(ap, makeAssistant(content));

        QCOMPARE(textSpy.count(), 1);
        QCOMPARE(textSpy[0][0].toString(), QStringLiteral("Hello, world!"));
        QVERIFY(actSpy.count() > 0);
    }

    void testParseAssistantThinking()
    {
        AgentProcess ap;
        QSignalSpy thinkSpy(&ap, &AgentProcess::thinkingText);

        QJsonArray content;
        content.append(makeThinkingBlock(QStringLiteral("Let me consider...")));
        feed(ap, makeAssistant(content));

        QCOMPARE(thinkSpy.count(), 1);
        QCOMPARE(thinkSpy[0][0].toString(), QStringLiteral("Let me consider..."));
    }

    void testParseAssistantToolUse()
    {
        AgentProcess ap;
        QSignalSpy toolSpy(&ap, &AgentProcess::toolUse);
        QSignalSpy actSpy(&ap, &AgentProcess::activityChanged);

        QJsonObject input;
        input[QStringLiteral("command")] = QStringLiteral("ls -la");

        QJsonArray content;
        content.append(makeToolUseBlock(QStringLiteral("Bash"), input));
        feed(ap, makeAssistant(content));

        QCOMPARE(toolSpy.count(), 1);
        QCOMPARE(toolSpy[0][0].toString(), QStringLiteral("Bash"));
        QCOMPARE(toolSpy[0][1].toJsonObject()[QStringLiteral("command")].toString(),
                 QStringLiteral("ls -la"));
        // Activity should say "Using Bash"
        QCOMPARE(ap.currentActivity(), QStringLiteral("Using Bash"));
    }

    void testParseAssistantToolResult()
    {
        AgentProcess ap;
        QSignalSpy resultSpy(&ap, &AgentProcess::toolResult);

        QJsonArray content;
        content.append(makeToolResultBlock(QStringLiteral("Read"), QStringLiteral("file contents here")));
        feed(ap, makeAssistant(content));

        QCOMPARE(resultSpy.count(), 1);
        QCOMPARE(resultSpy[0][0].toString(), QStringLiteral("Read"));
        QCOMPARE(resultSpy[0][1].toString(), QStringLiteral("file contents here"));
    }

    void testParseContextCompaction()
    {
        AgentProcess ap;
        QSignalSpy textSpy(&ap, &AgentProcess::assistantText);

        QJsonObject ctxMgmt;
        ctxMgmt[QStringLiteral("truncated")] = true;

        QJsonObject message;
        message[QStringLiteral("context_management")] = ctxMgmt;
        message[QStringLiteral("content")] = QJsonArray();

        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("assistant");
        obj[QStringLiteral("message")] = message;

        feed(ap, obj);

        QCOMPARE(textSpy.count(), 1);
        QVERIFY(textSpy[0][0].toString().contains(QStringLiteral("compacted")));
        QCOMPARE(ap.currentActivity(), QStringLiteral("Context compacted"));
    }

    void testParseMultipleContentBlocks()
    {
        AgentProcess ap;
        QSignalSpy textSpy(&ap, &AgentProcess::assistantText);
        QSignalSpy thinkSpy(&ap, &AgentProcess::thinkingText);
        QSignalSpy toolSpy(&ap, &AgentProcess::toolUse);

        QJsonObject input;
        input[QStringLiteral("file_path")] = QStringLiteral("/tmp/test.cpp");

        QJsonArray content;
        content.append(makeThinkingBlock(QStringLiteral("thinking first")));
        content.append(makeTextBlock(QStringLiteral("then text")));
        content.append(makeToolUseBlock(QStringLiteral("Read"), input));
        feed(ap, makeAssistant(content));

        QCOMPARE(thinkSpy.count(), 1);
        QCOMPARE(textSpy.count(), 1);
        QCOMPARE(toolSpy.count(), 1);
    }

    void testParseResult()
    {
        AgentProcess ap;
        QSignalSpy costSpy(&ap, &AgentProcess::costUpdated);
        QSignalSpy ctxSpy(&ap, &AgentProcess::contextUpdated);
        QSignalSpy resultSpy(&ap, &AgentProcess::resultReady);
        QSignalSpy statusSpy(&ap, &AgentProcess::statusChanged);

        feed(ap, makeResult(0.0342, 100, 50, 10, 5, 200000));

        QCOMPARE(costSpy.count(), 1);
        QCOMPARE(ap.totalCost(), 0.0342);

        QCOMPARE(ctxSpy.count(), 1);
        QCOMPARE(ap.contextWindow(), 200000);
        QCOMPARE(ap.contextUsed(), 165); // 100 + 50 + 10 + 5

        QCOMPARE(resultSpy.count(), 1);
        QCOMPARE(ap.status(), static_cast<int>(AgentProcess::Completed));
        QCOMPARE(ap.currentActivity(), QStringLiteral("Completed"));
    }

    void testParseResultNoModelUsage()
    {
        AgentProcess ap;
        QSignalSpy ctxSpy(&ap, &AgentProcess::contextUpdated);

        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("result");
        obj[QStringLiteral("total_cost_usd")] = 0.01;
        feed(ap, obj);

        QCOMPARE(ctxSpy.count(), 0);
        QCOMPARE(ap.status(), static_cast<int>(AgentProcess::Completed));
    }

    void testParseInvalidJson()
    {
        AgentProcess ap;
        QSignalSpy textSpy(&ap, &AgentProcess::assistantText);
        QSignalSpy initSpy(&ap, &AgentProcess::initialized);

        // Should not crash, should not emit anything
        ap.parseLine("this is not json at all");
        ap.parseLine("{broken json");
        ap.parseLine("");

        QCOMPARE(textSpy.count(), 0);
        QCOMPARE(initSpy.count(), 0);
    }

    void testParseUnknownType()
    {
        AgentProcess ap;
        QSignalSpy textSpy(&ap, &AgentProcess::assistantText);

        QJsonObject obj;
        obj[QStringLiteral("type")] = QStringLiteral("unknown_future_type");
        feed(ap, obj);

        // Should be silently ignored
        QCOMPARE(textSpy.count(), 0);
    }

    void testActivityTruncation()
    {
        AgentProcess ap;

        // Text longer than 80 chars — activity should be truncated
        QString longText = QString(200, QLatin1Char('x'));
        QJsonArray content;
        content.append(makeTextBlock(longText));
        feed(ap, makeAssistant(content));

        QCOMPARE(ap.currentActivity().size(), 80);
    }

    void testBuildArgsStart()
    {
        AgentProcess ap;
        QStringList args = ap.buildArgs(QStringLiteral("do something"), QStringLiteral("opus"), false);

        QVERIFY(!args.contains(QStringLiteral("--continue")));
        QVERIFY(args.contains(QStringLiteral("-p")));
        QVERIFY(args.contains(QStringLiteral("do something")));
        QVERIFY(args.contains(QStringLiteral("--output-format")));
        QVERIFY(args.contains(QStringLiteral("stream-json")));
        QVERIFY(args.contains(QStringLiteral("--verbose")));
        QVERIFY(args.contains(QStringLiteral("--model")));
        QVERIFY(args.contains(QStringLiteral("opus")));
        QVERIFY(args.contains(QStringLiteral("--dangerously-skip-permissions")));
    }

    void testBuildArgsResume()
    {
        AgentProcess ap;
        QStringList args = ap.buildArgs(QStringLiteral("continue"), QStringLiteral("sonnet"), true);

        QVERIFY(args.contains(QStringLiteral("--continue")));
        QVERIFY(args.contains(QStringLiteral("-p")));
        QVERIFY(args.contains(QStringLiteral("--model")));
        QVERIFY(args.contains(QStringLiteral("sonnet")));
    }

    void testBuildArgsWithImages()
    {
        AgentProcess ap;
        QStringList images = {QStringLiteral("/tmp/a.png"), QStringLiteral("/tmp/b.jpg")};
        QStringList args = ap.buildArgs(QStringLiteral("check these"), QStringLiteral("opus"), false, images);

        // The prompt argument should contain the image instructions
        int pIdx = args.indexOf(QStringLiteral("-p"));
        QVERIFY(pIdx >= 0);
        QString prompt = args[pIdx + 1];
        QVERIFY(prompt.contains(QStringLiteral("/tmp/a.png")));
        QVERIFY(prompt.contains(QStringLiteral("/tmp/b.jpg")));
        QVERIFY(prompt.contains(QStringLiteral("check these")));
        QVERIFY(prompt.contains(QStringLiteral("screenshots")));
    }

    void testBuildPromptWithImagesEmpty()
    {
        QString result = AgentProcess::buildPromptWithImages(QStringLiteral("hello"), {});
        QCOMPARE(result, QStringLiteral("hello"));
    }

    void testBuildPromptWithImagesAddsPrefix()
    {
        QStringList images = {QStringLiteral("/screenshot.png")};
        QString result = AgentProcess::buildPromptWithImages(QStringLiteral("describe"), images);

        QVERIFY(result.startsWith(QStringLiteral("[")));
        QVERIFY(result.contains(QStringLiteral("/screenshot.png")));
        QVERIFY(result.contains(QStringLiteral("Read tool")));
        QVERIFY(result.endsWith(QStringLiteral("describe")));
    }

    void testBuildPromptWithImagesEmptyPrompt()
    {
        QStringList images = {QStringLiteral("/img.png")};
        QString result = AgentProcess::buildPromptWithImages(QString(), images);

        QVERIFY(result.contains(QStringLiteral("/img.png")));
        // Should not append anything after the image block when prompt is empty
        QVERIFY(result.endsWith(QStringLiteral("\n")));
    }

    void testStatusTransitions()
    {
        AgentProcess ap;
        QCOMPARE(ap.status(), static_cast<int>(AgentProcess::Idle));

        // System init → Running
        feed(ap, makeSystemInit());
        QCOMPARE(ap.status(), static_cast<int>(AgentProcess::Running));

        // Result → Completed
        feed(ap, makeResult(0.01));
        QCOMPARE(ap.status(), static_cast<int>(AgentProcess::Completed));
    }

    void testSetStatusNoOpOnSameValue()
    {
        AgentProcess ap;
        QSignalSpy statusSpy(&ap, &AgentProcess::statusChanged);

        ap.setStatus(AgentProcess::Idle); // already Idle
        QCOMPARE(statusSpy.count(), 0);

        ap.setStatus(AgentProcess::Running);
        QCOMPARE(statusSpy.count(), 1);

        ap.setStatus(AgentProcess::Running); // same again
        QCOMPARE(statusSpy.count(), 1);
    }

    void testSetActivityNoOpOnSameValue()
    {
        AgentProcess ap;
        QSignalSpy actSpy(&ap, &AgentProcess::activityChanged);

        ap.setActivity(QStringLiteral("working"));
        QCOMPARE(actSpy.count(), 1);

        ap.setActivity(QStringLiteral("working")); // same
        QCOMPARE(actSpy.count(), 1);

        ap.setActivity(QStringLiteral("done"));
        QCOMPARE(actSpy.count(), 2);
    }
};

QTEST_GUILESS_MAIN(AgentProcessTest)
#include "agentprocesstest.moc"
