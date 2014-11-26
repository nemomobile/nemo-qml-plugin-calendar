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
    void cleanupTestCase();

    void modSetters();
    void testSave();
    void testRecurrenceException();
    void testDate_data();
    void testDate();

private:
    bool saveEvent(NemoCalendarEventModification *eventMod, QString *uid);
    QQmlEngine *engine;
    NemoCalendarApi *calendarApi;
    QSet<QString> mSavedEvents;
};

void tst_CalendarEvent::initTestCase()
{
    // Create plugin, it shuts down the DB in proper order
    engine = new QQmlEngine(this);
    NemoCalendarPlugin* plugin = new NemoCalendarPlugin();
    plugin->initializeEngine(engine, "foobar");
    calendarApi = new NemoCalendarApi(this);

    // FIXME: calls made directly after instantiating seem to have threading issues.
    // KDateTime/Timezone initialization somehow fails and caches invalid timezones,
    // resulting in times such as 2014-11-26T00:00:00--596523:-14
    // (Above offset hour is -2147482800 / (60*60))
    QTest::qWait(100);
}

void tst_CalendarEvent::modSetters()
{
    NemoCalendarEventModification *eventMod = calendarApi->createNewEvent();
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
    NemoCalendarEventModification *eventMod = calendarApi->createNewEvent();
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

    calendarApi->remove(uid);
    mSavedEvents.remove(uid);

    delete eventMod;
}

void tst_CalendarEvent::testRecurrenceException()
{
    NemoCalendarEventModification *event = calendarApi->createNewEvent();
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

    // adjust second occurrence a bit
    NemoCalendarEventModification *recurrenceException = calendarApi->createModification(savedEvent);
    QVERIFY(recurrenceException != 0);
    QDateTime modifiedSecond = secondStart.addSecs(10*60); // 12:10
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
    recurrenceException = 0;

    // update the exception time
    QSignalSpy eventChangeSpy(&query, SIGNAL(eventChanged()));
    query.resetStartTime();
    query.setRecurrenceIdString(info->recurrenceId());
    eventChangeSpy.wait();
    QVERIFY(eventChangeSpy.count() > 0);
    QVERIFY(query.event());

    recurrenceException = calendarApi->createModification(static_cast<NemoCalendarEvent*>(query.event()));
    QVERIFY(recurrenceException != 0);

    modifiedSecond = modifiedSecond.addSecs(20*60); // 12:30
    recurrenceException->setStartTime(modifiedSecond, NemoCalendarEvent::SpecLocalZone);
    recurrenceException->setEndTime(modifiedSecond.addSecs(10*60), NemoCalendarEvent::SpecLocalZone);
    QString modifiedLabel("Modified recurring event instance, ver 2");
    recurrenceException->setDisplayLabel(modifiedLabel);
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

    ///////
    // update the main event time within a day, exception stays intact
    NemoCalendarEventModification *mod = calendarApi->createModification(savedEvent);
    QVERIFY(mod != 0);
    QDateTime modifiedStart = startTime.addSecs(40*60); // 12:40
    mod->setStartTime(modifiedStart, NemoCalendarEvent::SpecLocalZone);
    mod->setEndTime(modifiedStart.addSecs(40*60), NemoCalendarEvent::SpecLocalZone);
    mod->save();
    QTest::qWait(1000);

    // and check
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime(), startTime.addDays(-1));
    QCOMPARE(occurrence->startTime(), modifiedStart);
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime(), startTime.addDays(1));
    // TODO: Would be the best if second occurrence in the main series stays away, but at the moment it doesn't.
    //QCOMPARE(occurrence->startTime(), modifiedStart.addDays(14));
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime::fromString(info->recurrenceId()),
                                                                    startTime.addDays(1));
    QVERIFY(occurrence);
    QCOMPARE(occurrence->startTime(), modifiedSecond);

    // at least the recurrence exception should be found at second occurrence date. for now we allow also newly
    // appeared occurrence from main event
    NemoCalendarAgendaModel agendaModel;
    agendaModel.setStartDate(startTime.addDays(7).date());
    agendaModel.setEndDate(agendaModel.startDate());
    QTest::qWait(2000);

    bool modificationFound = false;
    for (int i = 0; i < agendaModel.count(); ++i) {
        QVariant eventVariant = agendaModel.get(i, NemoCalendarAgendaModel::EventObjectRole);
        NemoCalendarEvent *modelEvent = qvariant_cast<NemoCalendarEvent*>(eventVariant);
        // assuming no left-over events
        if (modelEvent && modelEvent->displayLabel() == modifiedLabel) {
            modificationFound = true;
            break;
        }
    }
    QVERIFY(modificationFound);

    // ensure all gone
    calendarApi->removeAll(uid);
    mSavedEvents.remove(uid);
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime(), startTime.addDays(-1));
    QVERIFY(!occurrence->startTime().isValid());
    occurrence = NemoCalendarManager::instance()->getNextOccurrence(uid, KDateTime::fromString(info->recurrenceId()),
                                                                    startTime.addDays(1));
    QVERIFY(!occurrence->startTime().isValid());

    delete info;
    delete recurrenceException;
    delete mod;
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

void tst_CalendarEvent::testDate_data()
{
    QTest::addColumn<QDate>("date");
    QTest::newRow("2014/12/7") << QDate(2014, 12, 7);
    QTest::newRow("2014/12/8") << QDate(2014, 12, 8);
}

void tst_CalendarEvent::testDate()
{
    QFETCH(QDate, date);

    NemoCalendarEventModification *eventMod = calendarApi->createNewEvent();
    QVERIFY(eventMod != 0);

    eventMod->setDisplayLabel(QString("test event for ") + date.toString());
    QDateTime startTime(date, QTime(12, 0));
    eventMod->setStartTime(startTime, NemoCalendarEvent::SpecLocalZone);
    eventMod->setEndTime(startTime.addSecs(10*60), NemoCalendarEvent::SpecLocalZone);

    QString uid;
    bool ok = saveEvent(eventMod, &uid);
    if (!ok) {
        QFAIL("Failed to fetch new event uid");
    }

    QVERIFY(!uid.isEmpty());
    mSavedEvents.insert(uid);
}

void tst_CalendarEvent::cleanupTestCase()
{
    foreach (const QString &uid, mSavedEvents) {
        calendarApi->removeAll(uid);
        // make sure worker thread has time to complete removal before being destroyed.
        // TODO: finish method invocation queue before quitting?
        QTest::qWait(1000);
    }
}

#include "tst_calendarevent.moc"
QTEST_MAIN(tst_CalendarEvent)
