#include <QObject>
#include <QtTest>
#include <QQmlEngine>
#include <QSignalSpy>

#include "calendarapi.h"
#include "calendarevent.h"
#include "calendareventquery.h"
#include "calendaragendamodel.h"

#include "plugin.cpp"

class tst_CalendarEvent : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void initialValues();
    void setters();
    void testSignals();
    void recurExceptions();
    void testSave();
    void cleanupTestCase();

private:
    QQmlEngine *engine;
    QList <NemoCalendarEvent *> mSavedEvents;
};

void tst_CalendarEvent::initTestCase()
{
    // Create plugin, it shuts down the DB in proper order
    engine = new QQmlEngine(this);
    NemoCalendarPlugin* plugin = new NemoCalendarPlugin();
    plugin->initializeEngine(engine, "foobar");
}

void tst_CalendarEvent::initialValues()
{
    NemoCalendarApi calendarApi(this);
    NemoCalendarEvent *event = calendarApi.createEvent();
    QVERIFY(event != 0);

    // Check default values
    QVERIFY(!event->uniqueId().isEmpty());
    QVERIFY(!event->allDay());
    QVERIFY(event->calendarUid().isEmpty());
    QVERIFY(event->color() == "");
    QVERIFY(event->description().isEmpty());
    QVERIFY(event->displayLabel().isEmpty());
    QVERIFY(event->location().isEmpty());
    QVERIFY(!event->endTime().isValid());
    QVERIFY(!event->readonly());
    QVERIFY(event->recur() == NemoCalendarEvent::RecurOnce);
    QVERIFY(event->recurExceptions() == 0);
    QVERIFY(event->reminder() == NemoCalendarEvent::ReminderNone);
    QVERIFY(!event->startTime().isValid());
}

void tst_CalendarEvent::setters()
{
    NemoCalendarApi calendarApi(this);
    NemoCalendarEvent *event = calendarApi.createEvent();
    QVERIFY(event != 0);

    QSignalSpy allDaySpy(event, SIGNAL(allDayChanged()));
    bool allDay = !event->allDay();
    event->setAllDay(allDay);
    QCOMPARE(allDaySpy.count(), 1);
    QCOMPARE(event->allDay(), allDay);

    QSignalSpy descriptionSpy(event, SIGNAL(descriptionChanged()));
    QLatin1String description("Test event");
    event->setDescription(description);
    QCOMPARE(descriptionSpy.count(), 1);
    QCOMPARE(event->description(), description);

    QSignalSpy displayLabelSpy(event, SIGNAL(displayLabelChanged()));
    QLatin1String displayLabel("Test display label");
    event->setDisplayLabel(displayLabel);
    QCOMPARE(displayLabelSpy.count(), 1);
    QCOMPARE(event->displayLabel(), displayLabel);

    QSignalSpy locationSpy(event, SIGNAL(locationChanged()));
    QLatin1String location("Test location");
    event->setLocation(location);
    QCOMPARE(locationSpy.count(), 1);
    QCOMPARE(event->location(), location);

    QSignalSpy endTimeSpy(event, SIGNAL(endTimeChanged()));
    QDateTime endTime = QDateTime::currentDateTime();
    event->setEndTime(endTime, NemoCalendarEvent::SpecLocalZone);
    QCOMPARE(endTimeSpy.count(), 1);
    QCOMPARE(event->endTime(), endTime);

    QSignalSpy recurSpy(event, SIGNAL(recurChanged()));
    NemoCalendarEvent::Recur recur = NemoCalendarEvent::RecurDaily; // Default value is RecurOnce
    event->setRecur(recur);
    QCOMPARE(recurSpy.count(), 1);
    QCOMPARE(event->recur(), recur);

    QSignalSpy reminderSpy(event, SIGNAL(reminderChanged()));
    NemoCalendarEvent::Reminder reminder = NemoCalendarEvent::ReminderTime; // default is ReminderNone
    event->setReminder(reminder);
    QCOMPARE(reminderSpy.count(), 1);
    QCOMPARE(event->reminder(), reminder);

    QSignalSpy startTimeSpy(event, SIGNAL(startTimeChanged()));
    QDateTime startTime = QDateTime::currentDateTime();
    event->setStartTime(startTime, NemoCalendarEvent::SpecLocalZone);
    QCOMPARE(startTimeSpy.count(), 1);
    QCOMPARE(event->startTime(), startTime);
}

// Check that changes in one NemoCalendarEvent instance is reflected in other instances
void tst_CalendarEvent::testSignals()
{
    NemoCalendarApi calendarApi(this);
    NemoCalendarEvent *eventA = calendarApi.createEvent();
    QVERIFY(eventA != 0);

    NemoCalendarEventQuery query;
    query.setUniqueId(eventA->uniqueId());
    for (int i = 0; i < 30; i++) {
        if (query.event())
            break;

        QTest::qWait(100);
    }
    NemoCalendarEvent *eventB = (NemoCalendarEvent*) query.event();
    QVERIFY(eventB != 0);

    QSignalSpy labelSpyA(eventA, SIGNAL(displayLabelChanged()));
    QSignalSpy labelSpyB(eventB, SIGNAL(displayLabelChanged()));
    QLatin1String label("event display label");

    eventA->setDisplayLabel(label);
    QCOMPARE(labelSpyA.count(), 1);
    QCOMPARE(labelSpyB.count(), 1);
    QCOMPARE(eventA->displayLabel(), label);
    QCOMPARE(eventB->displayLabel(), label);

    eventA->setDisplayLabel(label);
    QCOMPARE(labelSpyA.count(), 1);
    QCOMPARE(labelSpyB.count(), 1);
    QCOMPARE(eventA->displayLabel(), label);
    QCOMPARE(eventB->displayLabel(), label);

    label = QLatin1String("another event label");
    eventA->setDisplayLabel(label);
    QCOMPARE(labelSpyA.count(), 2);
    QCOMPARE(labelSpyB.count(), 2);
    QCOMPARE(eventA->displayLabel(), label);
    QCOMPARE(eventB->displayLabel(), label);

    label = QLatin1String("yet another event label");
    eventB->setDisplayLabel(label);
    QCOMPARE(labelSpyA.count(), 3);
    QCOMPARE(labelSpyB.count(), 3);
    QCOMPARE(eventA->displayLabel(), label);
    QCOMPARE(eventB->displayLabel(), label);
}

void tst_CalendarEvent::recurExceptions()
{
    NemoCalendarApi calendarApi(this);
    NemoCalendarEvent *event = calendarApi.createEvent();
    QVERIFY(event != 0);

    event->setStartTime(QDateTime::currentDateTime(), NemoCalendarEvent::SpecLocalZone);
    event->setEndTime(QDateTime::currentDateTime().addSecs(3600), NemoCalendarEvent::SpecLocalZone);
    QCOMPARE(event->recurExceptions(), 0);
    QCOMPARE(event->recur(), NemoCalendarEvent::RecurOnce);
    QSignalSpy recurExceptionSpy(event, SIGNAL(recurExceptionsChanged()));

    // Add invalid recur exception date to a non-recurring event, nothing should happen
    event->addException(QDateTime());
    QCOMPARE(recurExceptionSpy.count(), 0);

    // Add valid recur exception date to a non-recurring event, nothing should happen
    event->addException(event->startTime().addDays(1));
    QCOMPARE(recurExceptionSpy.count(), 0);

    event->setRecur(NemoCalendarEvent::RecurDaily);
    // Add invalid recur exception date to a recurring event, nothing should happen
    event->addException(QDateTime());
    QCOMPARE(recurExceptionSpy.count(), 0);

    // Add valid recur exception date to a recurring event
    QDateTime exceptionDateTimeA = event->startTime().addDays(1);
    event->addException(exceptionDateTimeA);
    QCOMPARE(recurExceptionSpy.count(), 1);
    QCOMPARE(event->recurExceptions(), 1);
    QCOMPARE(event->recurException(0), exceptionDateTimeA);

    // Add a second recur exception
    QDateTime exceptionDateTimeB = event->startTime().addDays(2);
    event->addException(exceptionDateTimeB);
    QCOMPARE(recurExceptionSpy.count(), 2);
    QCOMPARE(event->recurExceptions(), 2);
    QCOMPARE(event->recurException(0), exceptionDateTimeA);
    QCOMPARE(event->recurException(1), exceptionDateTimeB);

    // Request non-valid recurException, valid index are 0,1.
    QVERIFY(!event->recurException(-1).isValid());
    QVERIFY(!event->recurException(17).isValid());

    // Remove invalid recur exception, valid index are 0,1.
    event->removeException(-7);
    QCOMPARE(event->recurExceptions(), 2);
    QCOMPARE(recurExceptionSpy.count(), 2);
    event->removeException(4);
    QCOMPARE(event->recurExceptions(), 2);
    QCOMPARE(recurExceptionSpy.count(), 2);

    // Remove valid recur exception
    event->removeException(0);
    QCOMPARE(recurExceptionSpy.count(), 3);
    QCOMPARE(event->recurException(0), exceptionDateTimeB);
}

void tst_CalendarEvent::testSave()
{
    NemoCalendarApi calendarApi(this);
    NemoCalendarEvent *event = calendarApi.createEvent();
    QVERIFY(event != 0);

    bool allDay = false;
    event->setAllDay(allDay);
    QCOMPARE(event->allDay(), allDay);

    QLatin1String description("Test event");
    event->setDescription(description);
    QCOMPARE(event->description(), description);

    QLatin1String displayLabel("Test display label");
    event->setDisplayLabel(displayLabel);
    QCOMPARE(event->displayLabel(), displayLabel);

    QLatin1String location("Test location");
    event->setLocation(location);
    QCOMPARE(event->location(), location);

    QDateTime endTime = QDateTime::currentDateTime();
    event->setEndTime(endTime, NemoCalendarEvent::SpecLocalZone);
    QCOMPARE(event->endTime(), endTime);

    NemoCalendarEvent::Recur recur = NemoCalendarEvent::RecurDaily;
    event->setRecur(recur);
    QCOMPARE(event->recur(), recur);

    NemoCalendarEvent::Reminder reminder = NemoCalendarEvent::ReminderTime;
    event->setReminder(reminder);
    QCOMPARE(event->reminder(), reminder);

    QDateTime startTime = QDateTime::currentDateTime();
    event->setStartTime(startTime, NemoCalendarEvent::SpecLocalZone);
    QCOMPARE(event->startTime(), startTime);

    NemoCalendarAgendaModel agendaModel;
    agendaModel.setStartDate(startTime.date());
    agendaModel.setEndDate(endTime.date());

    // Wait for the agendaModel to get updated, no way to determine when that happens, so just wait
    QTest::qWait(2000);

    int count = agendaModel.count();
    QSignalSpy countSpy(&agendaModel, SIGNAL(countChanged()));
    QCOMPARE(countSpy.count(), 0);
    event->save();
    mSavedEvents.append(event);
    for (int i = 0; i < 30; i++) {
        if (agendaModel.count() > count)
            break;

        QTest::qWait(100);
    }

    QCOMPARE(agendaModel.count(), count + 1);
    QVERIFY(countSpy.count() > 0);

    NemoCalendarEventQuery query;
    query.setUniqueId(event->uniqueId());
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
    QCOMPARE(eventB->reminder(), reminder);

    calendarApi.remove(event->uniqueId());
}

void tst_CalendarEvent::cleanupTestCase()
{
    NemoCalendarApi calendarApi(this);
    foreach (NemoCalendarEvent *event, mSavedEvents) {
        if (event) {
            calendarApi.remove(event->uniqueId());
            QTest::qWait(1000);
        }
    }
}

#include "tst_calendarevent.moc"
QTEST_MAIN(tst_CalendarEvent)
