#include "database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    Database database(temporary.filePath(QStringLiteral("calendar.db")));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString accountId = QStringLiteral("google:reminder-test");
    if (!temporary.isValid() || !database.open()
        || !database.upsertAccount(accountId, QStringLiteral("google"), QStringLiteral("reminder-test"),
                                   QStringLiteral("Reminder Test"), QStringLiteral("test@example.com"),
                                   QStringLiteral("connected"))
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { QStringLiteral("id"), QStringLiteral("primary@example.com") },
            { QStringLiteral("summary"), QStringLiteral("Primary") },
            { QStringLiteral("accessRole"), QStringLiteral("owner") },
            { QStringLiteral("selected"), true },
            { QStringLiteral("defaultReminders"), QJsonArray { QJsonObject {
                { QStringLiteral("method"), QStringLiteral("popup") },
                { QStringLiteral("minutes"), 10 }
            } } }
        } })) return 2;

    const QString calendarId = database.calendarsForAccount(accountId).array().first()
                                   .toObject().value(QStringLiteral("id")).toString();
    const QString eventId = database.createPendingEvent(QJsonObject {
        { QStringLiteral("calendarId"), calendarId },
        { QStringLiteral("title"), QStringLiteral("Reminder contract") },
        { QStringLiteral("startMs"), double(now + 100) },
        { QStringLiteral("endMs"), double(now + 3600000) },
        { QStringLiteral("reminders"), QJsonObject {
            { QStringLiteral("useDefault"), false },
            { QStringLiteral("overrides"), QJsonArray { QJsonObject {
                { QStringLiteral("method"), QStringLiteral("popup") },
                { QStringLiteral("minutes"), 0 }
            } } }
        } }
    });
    if (eventId.isEmpty()) return 3;
    if (!database.dueReminders(now).array().isEmpty()) return 4;
    QThread::msleep(150);
    const qint64 deliveryNow = QDateTime::currentMSecsSinceEpoch();
    const QJsonArray due = database.dueReminders(deliveryNow).array();
    if (due.size() != 1 || due.first().toObject().value(QStringLiteral("title"))
            != QStringLiteral("Reminder contract")) return 5;
    const QString reminderId = due.first().toObject().value(QStringLiteral("id")).toString();
    if (!database.markReminderDelivered(reminderId, 42)
        || database.reminderIdForNotification(42) != reminderId
        || !database.dueReminders(deliveryNow + 1000).array().isEmpty()) return 6;
    if (!database.snoozeReminder(reminderId, deliveryNow + 10 * 60 * 1000)
        || !database.dueReminders(deliveryNow + 9 * 60 * 1000).array().isEmpty()
        || database.dueReminders(deliveryNow + 11 * 60 * 1000).array().size() != 1) return 7;
    if (!database.dismissReminder(reminderId)
        || !database.dueReminders(deliveryNow + 12 * 60 * 1000).array().isEmpty()) return 8;
    return 0;
}
