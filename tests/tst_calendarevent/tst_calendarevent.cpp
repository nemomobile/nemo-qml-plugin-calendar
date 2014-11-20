#include <QObject>
#include <QtTest>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QSet>

#include <KDateTime>

#include "calendarapi.h"
#include "calendarevent.h"
#include "calendareventquery.h"
#include "calendaragendamodel.h"
#include "calendarmanager.h"

#include "plugin.cpp"

class tst_CalendarEvent : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void modSetters();
    void testSave();
    void testRecurrenceException();
    void cleanupTestCase();

private:
    bool saveEvent(NemoCalendarEventModification *eventMod, QString *uid);
    QQmlEngine *engine;
    QSet<QString> mSavedEvents;
};

void tst_CalendarEvent::initTestCase()
{
    // Create plugin, it shuts down the DB in proper order
    engine = new QQmlEngine(this);
    NemoCalendarPlugin* plugin = new NemoCalendarPlugin();
    plugin->initializeEngine(engine, "foobar");
}

void tst_CalendarEvent::modSetters()
{
    NemoCalendarApi calendarApi(this);
    NemoCalendarEventModification *eventMod = calendarApi.createNewEvent();
    QVERIFY(eventMod != 0);

    QSignalSpy allDaySpy(eventMod, SIGNAL(allDayChanged()));
    bool allDay = !eventMod->allDay();
    eventMod->setAllDay(allDay);
    QCOMPARE(allDaySpy.count(), 1);
    QCOMPARE(eventMod->allDay(), allDay);

    QSignalSpy descriptionSpy(eventMod, SIGNAL(descriptionChanged()));
    QLatin1String description("Test event");
    eventMod->setDescription(description);
    QCOMPARE(descriptionSpy.count(), 1);
    QCOMPARE(eventMod->description(), description);

    QSignalSpy displayLabelSpy(eventMod, SIGNAL(displayLabelChanged()));
    QLatin1String displayLabel("Test display label");
    eventMod->setDisplayLabel(displayLabel);
    QCOMPARE(displayLabelSpy.count(), 1);
    QCOMPARE(eventMod->displayLabel(), displayLabel);

    QSignalSpy locationSpy(eventMod, SIGNAL(locationChanged()));
    QLatin1String location("Test location");
    eventMod->setLocation(location);
    QCOMPARE(locationSpy.count(), 1);
    QCOMPARE(eventMod->location(), location);

    QSignalSpy endTimeSpy(eventMod, SIGNAL(endTimeChanged()));
    QDateTime endTime = QDateTime::currentDateTime();
    eventMod->setEndTime(endTime, NemoCalendarEvent::SpecLocalZone);
    QCOMPARE(endTimeSpy.count(), 1);
    QCOMPARE(eventMod->endTime(), endTime);

    QSignalSpy recurSpy(eventMod, SIGNAL(recurChanged()));
    NemoCalendarEvent::Recur recur = NemoCalendarEvent::RecurDaily; // Default value is RecurOnce
    eventMod->setRecur(recur);
    QCOMPARE(recurSpy.count(), 1);
    QCOMPARE(eventMod->recur(), recur);

    QSignalSpy recurEndSpy(eventMod, SIGNAL(recurEndDateChanged()));
    QDateTime recurEnd = QDateTime::currentDateTime().addDays(100);
    eventMod->setRecurEndDate(recurEnd);
    QCOMPARE(recurEndSpy.count(), 1);
    QCOMPARE(eventMod->recurEndDate(), QDateTime(recurEnd.date())); // day precision

    QSignalSpy reminderSpy(eventMod, SIGNAL(reminderChanged()));
    NemoCalendarEvent::Reminder reminder = NemoCalendarEvent::ReminderTime; // default is ReminderNone
    eventMod->setReminder(reminder);
    QCOMPARE(reminderSpy.count(), 1);
    QCOMPARE(eventMod->reminder(), reminder);

    QSignalSpy startTimeSpy(eventMod, SIGNAL(startTimeChanged()));
    QDateTime startTime = QDateTime::currentDateTime();
    eventMod->setStartTime(startTime, NemoCalendarEvent::SpecLocalZone);
    QCOMPARE(startTimeSpy.count(), 1);
    QCOMPARE(eventMod->startTime(), startTime);

    delete eventMod;
}

void tst_CalendarEvent::testSave()
{
    NemoCalendarApi calendarApi(this);
    NemoCalendarEventModification *eventMod = calendarApi.createNewEvent();
    QVERIFY(eventMod != 0);

    bool allDay = false;
    eventMod->setAllDay(allDay);
    QCOMPARE(eventMod->allDay(), allDay);

    QLatin1String description("Test event");
    eventMod->setDescription(description);
    QCOMPARE(eventMod->description(), description);

    QLatin1String displayLabel("Test display label");
    eventMod->setDisplayLabel(displayLabel);
    QCOMPARE(eventMod->displayLabel(), displayLabel);

    QLatin1String location("Test location");
    eventMod->setLocation(location);
    QCOMPARE(eventMod->location(), location);

    QDateTime endTime = QDateTime::currentDateTime();
    eventMod->setEndTime(endTime, NemoCalendarEvent::SpecLocalZone);
    QCOMPARE(eventMod->endTime(), endTime);

    NemoCalendarEvent::Recur recur = NemoCalendarEvent::RecurDaily;
    eventMod->setRecur(recur);
    QCOMPARE(eventMod->recur(), recur);

    QDateTime recurEnd = endTime.addDays(100);
    eventMod->setRecurEndDate(recurEnd);
    QCOMPARE(eventMod->recurEndDate(), QDateTime(recurEnd.date()));

    NemoCalendarEvent::Reminder reminder = NemoCalendarEvent::ReminderTime;
    eventMod->setReminder(reminder);
    QCOMPARE(eventMod->reminder(), reminder);

    QDateTime startTime = QDateTime::currentDateTime();
    eventMod->setStartTime(startTime, NemoCalendarEvent::SpecLocalZone);
    QCOMPARE(eventMod->startTime(), startTime);

    QString uid;
    bool ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);

    NemoCalendarEventQuery query;
    query.setUniqueId(uid);

    for (int i = 0; i < 30; i++) {
        if (query.event())
            break;

        QTest::qWait(100);
    }
    NemoCalendarEvent *eventB = (NemoCalendarEvent*) query.event();
    QVERIFY(eventB != 0);

    // mKCal DB stores times as seconds, loosing millisecond accuracy.
    // Compare dates with QDateTime::toTime_t() instead of QDateTime::toMSecsSinceEpoch()
    QCOMPARE(eventB->endTime().toTime_t(), endTime.toTime_t());
    QCOMPARE(eventB->startTime().toTime_t(), startTime.toTime_t());

    QCOMPARE(eventB->allDay(), allDay);
    QCOMPARE(eventB->description(), description);
    QCOMPARE(eventB->displayLabel(), displayLabel);
    QCOMPARE(eventB->location(), location);
    QCOMPARE(eventB->recur(), recur);
    QCOMPARE(eventB->recurEndDate(), QDateTime(recurEnd.date()));
    QCOMPARE(eventB->reminder(), reminder);

    calendarApi.remove(uid);
    mSavedEvents.remove(uid);

    delete eventMod;
}

void tst_CalendarEvent::testRecurrenceException()
{
    NemoCalendarApi calendarApi(this);
    NemoCalendarEventModification *event = calendarApi.createNewEvent();
    QVERIFY(event != 0);

    // main event
    event->setDisplayLabel("Recurring event");
    QDateTime startTime = QDateTime(QDate(2014, 6, 7), QTime(12, 00));
    QDateTime endTime = startTime.addSecs(60 * 60);
    event->setStartTime(startTime, NemoCalendarEvent::SpecLocalZone);
    event->setEndTime(endTime, NemoCalendarEvent::SpecLocalZone);
    NemoCalendarEvent::Recur recur = NemoCalendarEvent::RecurWeekly;
    event->setRecur(recur);
    QString uid;
    bool ok = saveEvent(event, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }
    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);

    // need event and occurrence to replace....
    NemoCalendarEventQuery query;
    query.setUniqueId(uid);
    QDateTime secondStart = startTime.addDays(7);
    query.setStartTime(secondStart);

    for (int i = 0; i < 30; i++) {
        if (query.event())
            break;

        QTest::qWait(100);
    }
    NemoCalendarEvent *savedEvent = (NemoCalendarEvent*) query.event();
    QVERIFY(savedEvent);
    QVERIFY(query.occurrence());

    NemoCalendarEventModification *recurrenceException = calendarApi.createModification(savedEvent);
    QVERIFY(recurrenceException != 0);
    QDateTime modifiedSecond = secondStart.addSecs(10*60);
    recurrenceException->setStartTime(modifiedSecond, NemoCalendarEvent::SpecLocalZone);
    recurrenceException->setEndTime(modifiedSecond.addSecs(10*60), NemoCalendarEvent::SpecLocalZone);
    recurrenceException->setDisplayLabel("Modified recurring event instance");
    NemoCalendarChangeInformation *info
            = recurrenceException->replaceOccurrence(static_cast<NemoCalendarEventOccurrence*>(query.occurrence()));
    QVERIFY(info);
    QSignalSpy doneSpy(info, SIGNAL(pendingChanged()));
    doneSpy.wait();
    QCOMPARE(doneSpy.count(), 1);
    QVERIFY(!info->recurrenceId().isEmpty());

    QTest::qWait(1000); // allow saved data to be reloaded

    // check the occurrences are correct
    NemoCalendarEventOccurrence *occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime(),
                                                                                                 startTime.addDays(-1));
    // first
    QCOMPARE(occurrence->startTime(), startTime);
    // third
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime(), startTime.addDays(1));
    QCOMPARE(occurrence->startTime(), startTime.addDays(14));
    // second is exception
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime::fromString(info->recurrenceId()),
                                                                    startTime.addDays(1));

    QCOMPARE(occurrence->startTime(), modifiedSecond);
    delete recurrenceException;

    // update the exception time
    QSignalSpy eventChangeSpy(&query, SIGNAL(eventChanged()));
    query.resetStartTime();
    query.setRecurrenceIdString(info->recurrenceId());
    eventChangeSpy.wait();
    QVERIFY(eventChangeSpy.count() > 0);
    QVERIFY(query.event());

    recurrenceException = calendarApi.createModification(static_cast<NemoCalendarEvent*>(query.event()));
    QVERIFY(recurrenceException != 0);

    modifiedSecond = modifiedSecond.addSecs(20*60);
    recurrenceException->setStartTime(modifiedSecond, NemoCalendarEvent::SpecLocalZone);
    recurrenceException->setEndTime(modifiedSecond.addSecs(10*60), NemoCalendarEvent::SpecLocalZone);
    recurrenceException->setDisplayLabel("Modified recurring event instance, ver 2");
    recurrenceException->save();
    QTest::qWait(1000); // allow saved data to be reloaded

    // check the occurrences are correct
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime(), startTime.addDays(-1));
    // first
    QCOMPARE(occurrence->startTime(), startTime);
    // third
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime(), startTime.addDays(1));
    QCOMPARE(occurrence->startTime(), startTime.addDays(14));
    // second is exception
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime::fromString(info->recurrenceId()),
                                                                    startTime.addDays(1));

    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), modifiedSecond);

    // ensure all gone
    calendarApi.removeAll(uid);
    mSavedEvents.remove(uid);
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime(), startTime.addDays(-1));
    QVERIFY(!occurrence->startTime().isValid());
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime::fromString(info->recurrenceId()),
                                                                    startTime.addDays(1));
    QVERIFY(!occurrence->startTime().isValid());

    delete info;
    delete recurrenceException;
}

// saves event and tries to watch for new uid
bool tst_CalendarEvent::saveEvent(NemoCalendarEventModification *eventMod, QString *uid)
{
    NemoCalendarAgendaModel agendaModel;
    agendaModel.setStartDate(eventMod->startTime().date());
    agendaModel.setEndDate(eventMod->endTime().date());

    // Wait for the agendaModel to get updated, no way to determine when that happens, so just wait
    QTest::qWait(2000);

    int count = agendaModel.count();
    QSignalSpy countSpy(&agendaModel, SIGNAL(countChanged()));
    if (countSpy.count() != 0) {
        qWarning() << "saveEvent() - countSpy == 0";
        return false;
    }

    eventMod->save();
    for (int i = 0; i < 30; i++) {
        if (agendaModel.count() > count)
            break;

        QTest::qWait(100);
    }

    if (agendaModel.count() != count + 1
            || countSpy.count() == 0) {
        qWarning() << "saveEvent() - invalid counts";
        return false;
    }

    for (int i = 0; i < agendaModel.count(); ++i) {
        QVariant eventVariant = agendaModel.get(i, NemoCalendarAgendaModel::EventObjectRole);
        NemoCalendarEvent *modelEvent = qvariant_cast<NemoCalendarEvent*>(eventVariant);
        // assume no left-over events with same description
        if (modelEvent && modelEvent->description() == eventMod->description()) {
            *uid = modelEvent->uniqueId();
            break;
        }
    }

    return true;
}

void tst_CalendarEvent::cleanupTestCase()
{
    NemoCalendarApi calendarApi(this);
    foreach (const QString &uid, mSavedEvents) {
        calendarApi.removeAll(uid);
        // make sure worker thread has time to complete removal before being destroyed.
        // TODO: finish method invocation queue before quitting?
        QTest::qWait(1000);
    }
}

#include "tst_calendarevent.moc"
QTEST_MAIN(tst_CalendarEvent)
