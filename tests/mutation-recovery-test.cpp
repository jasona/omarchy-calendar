#include "database.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 2;

    const QString path = temporary.filePath(QStringLiteral("calendar.db"));
    const QString accountId = QStringLiteral("google:recovery-test");
    QString sourceId;
    QString targetId;
    QString createMutationId;
    QString createdEventId;
    {
        Database database(path);
        if (!database.open()
            || !database.upsertAccount(accountId, QStringLiteral("google"),
                                       QStringLiteral("recovery-test"), QStringLiteral("Recovery"),
                                       QStringLiteral("recovery@example.com"), QStringLiteral("connected"))
            || !database.replaceGoogleCalendars(accountId, QJsonArray {
                QJsonObject { { "id", "source@example.com" }, { "summary", "Source" },
                              { "timeZone", "America/Phoenix" }, { "accessRole", "owner" },
                              { "selected", true } },
                QJsonObject { { "id", "target@example.com" }, { "summary", "Target" },
                              { "timeZone", "America/Phoenix" }, { "accessRole", "writer" },
                              { "selected", true } }
            })) return 3;
        for (const QJsonValue &value : database.calendarsForAccount(accountId).array()) {
            const QJsonObject calendar = value.toObject();
            if (calendar.value("providerCalendarId") == QStringLiteral("source@example.com"))
                sourceId = calendar.value("id").toString();
            else if (calendar.value("providerCalendarId") == QStringLiteral("target@example.com"))
                targetId = calendar.value("id").toString();
        }
        if (sourceId.isEmpty() || targetId.isEmpty()) return 4;

        auto remote = [](const QString &id, const QString &title) {
            return QJsonObject {
                { "id", id }, { "summary", title }, { "status", "confirmed" },
                { "start", QJsonObject { { "dateTime", "2026-09-21T09:00:00-07:00" },
                                           { "timeZone", "America/Phoenix" } } },
                { "end", QJsonObject { { "dateTime", "2026-09-21T10:00:00-07:00" },
                                         { "timeZone", "America/Phoenix" } } },
                { "etag", id + QStringLiteral("-etag") }, { "updated", "2026-09-21T20:00:00Z" }
            };
        };
        QJsonObject invitation = remote(QStringLiteral("invite-1"), QStringLiteral("Invitation"));
        invitation.insert("organizer", QJsonObject { { "email", "owner@example.com" } });
        invitation.insert("attendees", QJsonArray {
            QJsonObject { { "email", "recovery@example.com" }, { "self", true },
                          { "responseStatus", "needsAction" } }
        });
        if (!database.applyGoogleEvents(accountId, sourceId, QJsonArray {
                remote(QStringLiteral("update-1"), QStringLiteral("Update")),
                invitation,
                remote(QStringLiteral("move-1"), QStringLiteral("Move")),
                remote(QStringLiteral("delete-1"), QStringLiteral("Delete"))
            }, QStringLiteral("recovery-cursor"), true)) return 5;

        const QJsonArray events = database.eventsForRange("2026-09-21", "2026-09-21").array();
        QJsonObject updateEvent;
        QJsonObject moveEvent;
        for (const QJsonValue &value : events) {
            const QJsonObject event = value.toObject();
            if (event.value("id") == QStringLiteral("update-1")) updateEvent = event;
            if (event.value("id") == QStringLiteral("move-1")) moveEvent = event;
        }
        createdEventId = database.createPendingEvent(QJsonObject {
            { "calendarId", sourceId }, { "title", "Offline create" },
            { "startMs", updateEvent.value("startMs") }, { "endMs", updateEvent.value("endMs") },
            { "timeZone", "America/Phoenix" }
        });
        if (createdEventId.isEmpty()
            || !database.updatePendingEvent(QJsonObject {
                { "id", "update-1" }, { "calendarId", sourceId }, { "title", "Offline update" },
                { "startMs", updateEvent.value("startMs") }, { "endMs", updateEvent.value("endMs") },
                { "timeZone", "America/Phoenix" }
            })
            || !database.respondPendingEvent(sourceId, QStringLiteral("invite-1"), QStringLiteral("accepted"))
            || !database.updatePendingEvent(QJsonObject {
                { "id", "move-1" }, { "calendarId", sourceId }, { "targetCalendarId", targetId },
                { "title", "Move" }, { "startMs", moveEvent.value("startMs") },
                { "endMs", moveEvent.value("endMs") }, { "timeZone", "America/Phoenix" }
            })) return 6;
        const QString deletion = database.deletePendingEvent(sourceId, QStringLiteral("delete-1"));
        if (deletion.isEmpty() || !database.finalizePendingDelete(deletion)) return 7;
    }

    {
        Database recovered(path);
        if (!recovered.open()) return 8;
        const QJsonArray mutations = recovered.pendingMutations().array();
        QSet<QString> operations;
        for (const QJsonValue &value : mutations) {
            const QJsonObject mutation = value.toObject();
            operations.insert(mutation.value("operation").toString());
            if (mutation.value("operation") == QStringLiteral("create"))
                createMutationId = mutation.value("id").toString();
        }
        if (mutations.size() != 5
            || operations != QSet<QString> { "create", "update", "rsvp", "move", "delete" }) return 9;
        const QJsonArray events = recovered.eventsForRange("2026-09-21", "2026-09-21").array();
        bool foundCreate = false;
        bool foundUpdate = false;
        bool foundRsvp = false;
        bool foundMove = false;
        for (const QJsonValue &value : events) {
            const QJsonObject event = value.toObject();
            foundCreate = foundCreate || event.value("title") == QStringLiteral("Offline create");
            foundUpdate = foundUpdate || event.value("title") == QStringLiteral("Offline update");
            foundRsvp = foundRsvp || (event.value("id") == QStringLiteral("invite-1")
                && event.value("selfResponseStatus") == QStringLiteral("accepted"));
            foundMove = foundMove || (event.value("id") == QStringLiteral("move-1")
                && event.value("calendarId") == targetId);
            if (event.value("id") == QStringLiteral("delete-1")) return 10;
        }
        if (!foundCreate || !foundUpdate || !foundRsvp || !foundMove || createMutationId.isEmpty()
            || !recovered.setMutationState(createMutationId, QStringLiteral("uploading"), {}, true)) {
            std::fprintf(stderr, "recovery state create=%d update=%d rsvp=%d move=%d mutation=%s events=%s error=%s\n",
                         foundCreate, foundUpdate, foundRsvp, foundMove,
                         qPrintable(createMutationId),
                         QJsonDocument(events).toJson(QJsonDocument::Compact).constData(),
                         qPrintable(recovered.lastError()));
            return 11;
        }
    }

    {
        Database recovered(path);
        if (!recovered.open()) return 12;
        const QJsonObject next = recovered.nextPendingMutation(accountId).object();
        if (next.value("id") != createMutationId || next.value("state") != QStringLiteral("uploading")
            || next.value("attemptCount").toInt() != 1
            || !recovered.setMutationState(createMutationId, QStringLiteral("retrying"),
                                           QStringLiteral("connection interrupted"))
            || !recovered.retryMutation(createMutationId)
            || recovered.nextPendingMutation(accountId).object().value("state") != QStringLiteral("queued")) return 13;
    }

    QFile corrupt(temporary.filePath(QStringLiteral("corrupt.db")));
    if (!corrupt.open(QIODevice::WriteOnly) || corrupt.write("not a sqlite database") < 1) return 14;
    corrupt.close();
    Database corrupted(corrupt.fileName());
    if (corrupted.open() || corrupted.lastError().isEmpty()) return 15;

    Database unwritable(QStringLiteral("/proc/omarchy-calendar-test/calendar.db"));
    if (unwritable.open() || unwritable.lastError().isEmpty()) return 16;
    return 0;
}
