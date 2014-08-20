#include "calendareventmodification.h"
#include "calendarmanager.h"

NemoCalendarEventModification::NemoCalendarEventModification(NemoCalendarData::Event data, QObject *parent)
    : QObject(parent), m_event(data)
{
}

NemoCalendarEventModification::NemoCalendarEventModification(QObject *parent) :
    QObject(parent)
{
    m_event.recur = NemoCalendarEvent::RecurOnce;
    m_event.reminder = NemoCalendarEvent::ReminderNone;
    m_event.allDay = false;
    m_event.readonly = false;
    m_event.startTime = KDateTime(QDateTime(), KDateTime::LocalZone);
    m_event.endTime = KDateTime(QDateTime(), KDateTime::LocalZone);
}

NemoCalendarEventModification::~NemoCalendarEventModification()
{
}

QString NemoCalendarEventModification::displayLabel() const
{
    return m_event.displayLabel;
}

void NemoCalendarEventModification::setDisplayLabel(const QString &displayLabel)
{
    if (m_event.displayLabel != displayLabel) {
        m_event.displayLabel = displayLabel;
        emit displayLabelChanged();
    }
}

QString NemoCalendarEventModification::description() const
{
    return m_event.description;
}

void NemoCalendarEventModification::setDescription(const QString &description)
{
    if (m_event.description != description) {
        m_event.description = description;
        emit descriptionChanged();
    }
}

QDateTime NemoCalendarEventModification::startTime() const
{
    return m_event.startTime.dateTime();
}

void NemoCalendarEventModification::setStartTime(const QDateTime &startTime, int spec)
{
    KDateTime::SpecType kSpec = KDateTime::LocalZone;
    if (spec == NemoCalendarEvent::SpecClockTime) {
        kSpec = KDateTime::ClockTime;
    }

    KDateTime time(startTime, kSpec);
    if (m_event.startTime != time) {
        m_event.startTime = time;
        emit startTimeChanged();
    }
}

QDateTime NemoCalendarEventModification::endTime() const
{
    return m_event.endTime.dateTime();
}

void NemoCalendarEventModification::setEndTime(const QDateTime &endTime, int spec)
{
    KDateTime::SpecType kSpec = KDateTime::LocalZone;
    if (spec == NemoCalendarEvent::SpecClockTime) {
        kSpec = KDateTime::ClockTime;
    }
    KDateTime time(endTime, kSpec);

    if (m_event.endTime != time) {
        m_event.endTime = time;
        emit endTimeChanged();
    }
}

bool NemoCalendarEventModification::allDay() const
{
    return m_event.allDay;
}

void NemoCalendarEventModification::setAllDay(bool allDay)
{
    if (m_event.allDay != allDay) {
        m_event.allDay = allDay;
        emit allDayChanged();
    }
}

NemoCalendarEvent::Recur NemoCalendarEventModification::recur() const
{
    return m_event.recur;
}

void NemoCalendarEventModification::setRecur(NemoCalendarEvent::Recur recur)
{
    if (m_event.recur != recur) {
        m_event.recur = recur;
        emit recurChanged();
    }
}

QDateTime NemoCalendarEventModification::recurEndDate() const
{
    return QDateTime(m_event.recurEndDate);
}

bool NemoCalendarEventModification::hasRecurEndDate() const
{
    return m_event.recurEndDate.isValid();
}

void NemoCalendarEventModification::setRecurEndDate(const QDateTime &dateTime)
{
    bool wasValid = m_event.recurEndDate.isValid();
    QDate date = dateTime.date();

    if (m_event.recurEndDate != date) {
        m_event.recurEndDate = date;
        emit recurEndDateChanged();

        if (date.isValid() != wasValid) {
            emit hasRecurEndDateChanged();
        }
    }
}

void NemoCalendarEventModification::unsetRecurEndDate()
{
    setRecurEndDate(QDateTime());
}

NemoCalendarEvent::Reminder NemoCalendarEventModification::reminder() const
{
    return m_event.reminder;
}

void NemoCalendarEventModification::setReminder(NemoCalendarEvent::Reminder reminder)
{
    if (reminder != m_event.reminder) {
        m_event.reminder = reminder;
        emit reminderChanged();
    }
}

QString NemoCalendarEventModification::location() const
{
    return m_event.location;
}

void NemoCalendarEventModification::setLocation(const QString &newLocation)
{
    if (newLocation != m_event.location) {
        m_event.location = newLocation;
        emit locationChanged();
    }
}

QString NemoCalendarEventModification::calendarUid() const
{
    return m_event.calendarUid;
}

void NemoCalendarEventModification::setCalendarUid(const QString &uid)
{
    if (m_event.calendarUid != uid) {
        m_event.calendarUid = uid;
        emit calendarUidChanged();
    }
}

void NemoCalendarEventModification::save()
{
    NemoCalendarManager::instance()->saveModification(m_event);
}

void NemoCalendarEventModification::replaceOccurrence(NemoCalendarEventOccurrence *occurrence)
{
    NemoCalendarManager::instance()->replaceOccurrence(m_event, occurrence);
}
