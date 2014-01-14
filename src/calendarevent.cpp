/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Robin Burchell <robin.burchell@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "calendarevent.h"


#ifdef NEMO_USE_QT5
#include <QQmlInfo>
#else
#include <QDeclarativeInfo>
#endif

#include "calendardb.h"
#include "calendareventcache.h"

#include <vcalformat.h>
#include <libical/vobject.h>
#include <libical/vcaltmp.h>

NemoCalendarEvent::NemoCalendarEvent(QObject *parent)
: QObject(parent), mNewEvent(true), mEvent(KCalCore::Event::Ptr(new KCalCore::Event))
{
    NemoCalendarEventCache::instance()->mEvents.insert(this);
}

NemoCalendarEvent::NemoCalendarEvent(const KCalCore::Event::Ptr &event, QObject *parent)
: QObject(parent), mNewEvent(false), mEvent(event)
{
    NemoCalendarEventCache::instance()->mEvents.insert(this);
}

NemoCalendarEvent::~NemoCalendarEvent()
{
    NemoCalendarEventCache::instance()->mEvents.remove(this);
}

QString NemoCalendarEvent::displayLabel() const
{
    return mEvent ? mEvent->summary() : QString();
}

void NemoCalendarEvent::setDisplayLabel(const QString &displayLabel)
{
    if (!mEvent || mEvent->summary() == displayLabel)
        return;

    mEvent->setSummary(displayLabel);

    foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
        emit event->displayLabelChanged();
}

QString NemoCalendarEvent::description() const
{
    return mEvent ? mEvent->description() : QString();
}

void NemoCalendarEvent::setDescription(const QString &description)
{
    if (!mEvent || mEvent->description() == description)
        return;

    mEvent->setDescription(description);

    foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
        emit event->descriptionChanged();
}

QDateTime NemoCalendarEvent::startTime() const
{
    return mEvent ? mEvent->dtStart().toLocalZone().dateTime() : QDateTime();
}

void NemoCalendarEvent::setStartTime(const QDateTime &startTime)
{
    if (!mEvent || mEvent->dtStart().toLocalZone().dateTime() == startTime)
        return;

    mEvent->setDtStart(KDateTime(startTime, KDateTime::Spec(KDateTime::LocalZone)));

    foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
        emit event->startTimeChanged();
}

QDateTime NemoCalendarEvent::endTime() const
{
    return mEvent ? mEvent->dtEnd().toLocalZone().dateTime() : QDateTime();
}

void NemoCalendarEvent::setEndTime(const QDateTime &endTime)
{
    if (!mEvent || mEvent->dtEnd().toLocalZone().dateTime() == endTime)
        return;

    mEvent->setDtEnd(KDateTime(endTime, KDateTime::Spec(KDateTime::LocalZone)));

    foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
        emit event->endTimeChanged();
}

bool NemoCalendarEvent::allDay() const
{
    return mEvent ? mEvent->allDay() : false;
}

void NemoCalendarEvent::setAllDay(bool a)
{
    if (!mEvent || allDay() == a)
        return;

    mEvent->setAllDay(a);

    foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
        emit event->allDayChanged();
}

NemoCalendarEvent::Recur NemoCalendarEvent::recur() const
{
    if (mEvent && mEvent->recurs()) {
        if (mEvent->recurrence()->rRules().count() != 1) {
            return RecurCustom;
        } else {
            ushort rt = mEvent->recurrence()->recurrenceType();
            int freq = mEvent->recurrence()->frequency();

            if (rt == KCalCore::Recurrence::rDaily && freq == 1) {
                return RecurDaily;
            } else if (rt == KCalCore::Recurrence::rWeekly && freq == 1) {
                return RecurWeekly;
            } else if (rt == KCalCore::Recurrence::rWeekly && freq == 2) {
                return RecurBiweekly;
            } else if (rt == KCalCore::Recurrence::rMonthlyDay && freq == 1) {
                return RecurMonthly;
            } else if (rt == KCalCore::Recurrence::rYearlyMonth && freq == 1) {
                return RecurYearly;
            } else {
                return RecurCustom;
            }
        }
    } else {
        return RecurOnce;
    }
}

void NemoCalendarEvent::setRecur(Recur r)
{
    if (!mEvent)
        return;

    Recur oldRecur = recur();
    int oldExceptions = recurExceptions();

    if (r == RecurCustom) {
        qmlInfo(this) << "Cannot assign RecurCustom";
        r = RecurOnce;
    }

    if (r == RecurOnce)
        mEvent->recurrence()->clear();

    if (oldRecur != r) {
        switch (r) {
            case RecurOnce:
                break;
            case RecurDaily:
                mEvent->recurrence()->setDaily(1);
                break;
            case RecurWeekly:
                mEvent->recurrence()->setWeekly(1);
                break;
            case RecurBiweekly:
                mEvent->recurrence()->setWeekly(2);
                break;
            case RecurMonthly:
                mEvent->recurrence()->setMonthly(1);
                break;
            case RecurYearly:
                mEvent->recurrence()->setYearly(1);
                break;
            case RecurCustom:
                break;
        }

        foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
            emit event->recurChanged();
    }

    if (recurExceptions() != oldExceptions) {
        foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
            emit event->recurExceptionsChanged();
    }
}

int NemoCalendarEvent::recurExceptions() const
{
    return mEvent->recurs() ? mEvent->recurrence()->exDateTimes().count() : 0;
}

void NemoCalendarEvent::removeException(int index)
{
    if (mEvent->recurs()) {
        KCalCore::DateTimeList list = mEvent->recurrence()->exDateTimes();
        if (list.count() > index) {
            list.removeAt(index);
            mEvent->recurrence()->setExDateTimes(list);
            foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
                emit event->recurExceptionsChanged();
        }
    }
}

void NemoCalendarEvent::addException(const QDateTime &date)
{
    if (mEvent->recurs()) {
        KCalCore::DateTimeList list = mEvent->recurrence()->exDateTimes();
        list.append(KDateTime(date, KDateTime::Spec(KDateTime::LocalZone)));
        mEvent->recurrence()->setExDateTimes(list);
        foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
            emit event->recurExceptionsChanged();
    } else {
        qmlInfo(this) << "Cannot add exception to non-recurring event";
    }
}

QDateTime NemoCalendarEvent::recurException(int index) const
{
    if (mEvent->recurs()) {
        KCalCore::DateTimeList list = mEvent->recurrence()->exDateTimes();
        if (list.count() > index)
            return list.at(index).toLocalZone().dateTime();
    }
    return QDateTime();
}

NemoCalendarEvent::Reminder NemoCalendarEvent::reminder() const
{
    KCalCore::Alarm::List alarms = mEvent->alarms();

    KCalCore::Alarm::Ptr alarm;

    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure)
            continue;

        if (alarm)
            return ReminderNone;
        else
            alarm = alarms.at(ii);
    }

    if (!alarm)
        return ReminderNone;

    KCalCore::Duration d = alarm->startOffset();
    int sec = d.asSeconds();

    switch (sec) {
    case 0:
        return ReminderTime;
    case -5 * 60:
        return Reminder5Min;
    case -15 * 60:
        return Reminder15Min;
    case -30 * 60:
        return Reminder30Min;
    case -60 * 60:
        return Reminder1Hour;
    case -2 * 60 * 60:
        return Reminder2Hour;
    case -24 * 60 * 60:
        return Reminder1Day;
    case -2 * 24 * 60 * 60:
        return Reminder2Day;
    default:
        return ReminderNone;
    }
}

void NemoCalendarEvent::setReminder(Reminder r)
{
    Reminder old = reminder();

    KCalCore::Alarm::List alarms = mEvent->alarms();
    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure)
            continue;
        mEvent->removeAlarm(alarms.at(ii));
    }

    KCalCore::Duration offset(0);

    switch (r) {
    default:
    case ReminderNone:
    case ReminderTime:
        break;
    case Reminder5Min:
        offset = KCalCore::Duration(-5 * 60);
        break;
    case Reminder15Min:
        offset = KCalCore::Duration(-15 * 60);
        break;
    case Reminder30Min:
        offset = KCalCore::Duration(-30 * 60);
        break;
    case Reminder1Hour:
        offset = KCalCore::Duration(-60 * 60);
        break;
    case Reminder2Hour:
        offset = KCalCore::Duration(-2 * 60 * 60);
        break;
    case Reminder1Day:
        offset = KCalCore::Duration(-24 * 60 * 60);
        break;
    case Reminder2Day:
        offset = KCalCore::Duration(-2 * 24 * 60 * 60);
        break;
    }

    if (r != ReminderNone) {
        KCalCore::Alarm::Ptr alarm = mEvent->newAlarm();
        alarm->setEnabled(true);
        alarm->setStartOffset(offset);
    }

    if (r != old) {
        foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
            emit event->reminderChanged();
    }
}

QString NemoCalendarEvent::uniqueId() const
{
    return mEvent->uid();
}

QString NemoCalendarEvent::color() const
{
    QString eventNotebook = NemoCalendarDb::calendar()->notebook(mEvent);
    return NemoCalendarEventCache::instance()->notebookColor(eventNotebook);
}

QString NemoCalendarEvent::alarmProgram() const
{
    KCalCore::Alarm::List alarms = mEvent->alarms();

    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure &&
            alarms.at(ii)->programArguments() == uniqueId())
            return alarms.at(ii)->programFile();
    }

    return QString();
}

void NemoCalendarEvent::setAlarmProgram(const QString &program)
{
    KCalCore::Alarm::List alarms = mEvent->alarms();

    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure &&
            alarms.at(ii)->programArguments() == uniqueId()) {

            if (alarms.at(ii)->programFile() != program) {
                alarms[ii]->setProgramFile(program);
                foreach(NemoCalendarEvent *event, NemoCalendarEventCache::events(mEvent))
                    emit event->alarmProgramChanged();
            }

            return;
        }
    }

    KCalCore::Alarm::Ptr alarm = mEvent->newAlarm();
    alarm->setEnabled(true);
    alarm->setType(KCalCore::Alarm::Procedure);
    alarm->setProcedureAlarm(program, uniqueId());
}

bool NemoCalendarEvent::readonly() const
{
    QString eventNotebook = NemoCalendarDb::calendar()->notebook(mEvent);
    return NemoCalendarDb::storage()->notebook(eventNotebook)->isReadOnly();
}

QString NemoCalendarEvent::calendarUid() const
{
    return NemoCalendarDb::calendar()->notebook(mEvent);
}

void NemoCalendarEvent::save(const QString &calendarUid)
{
    QString uid = calendarUid.isEmpty() ? mNewEvent ? NemoCalendarDb::storage()->defaultNotebook()->uid()
                                                    : this->calendarUid()
                                        : calendarUid;

    mKCal::Notebook::Ptr notebook = NemoCalendarDb::storage()->notebook(uid);

    if (notebook == 0) {
        return;
    }

    if (mNewEvent || this->calendarUid() != uid) {

        if (mNewEvent) {
            mNewEvent = false;
        } else if (this->calendarUid() != uid) {
            remove();
        }

        NemoCalendarDb::calendar()->addEvent(mEvent, uid);
        emit calendarUidChanged();
    }

    mEvent->setRevision(mEvent->revision() + 1);

    // TODO: this sucks
    NemoCalendarDb::storage()->save();
}

// Removes the entire event
void NemoCalendarEvent::remove()
{
    if (!mNewEvent) {
        NemoCalendarDb::calendar()->deleteEvent(mEvent);

        // TODO: this sucks
        NemoCalendarDb::storage()->save();
    }
}

// eventToVEvent() is protected
class NemoCalendarVCalFormat : public KCalCore::VCalFormat
{
public:
    QString convertEventToVEvent(const KCalCore::Event::Ptr &event, const QString &prodId)
    {
        VObject *vCalObj = vcsCreateVCal(
            QDateTime::currentDateTime().toString(Qt::ISODate).toLatin1().data(),
            NULL,
            prodId.toLatin1().data(),
            NULL,
            "1.0");
        VObject *vEventObj = eventToVEvent(event);
        addVObjectProp(vCalObj, vEventObj);
        char *memVObject = writeMemVObject(0, 0, vCalObj);
        QString retn = QLatin1String(memVObject);
        free(memVObject);
        free(vEventObj);
        free(vCalObj);
        return retn;
    }
};

// Returns the event as a VCalendar string
QString NemoCalendarEvent::vCalendar(const QString &prodId) const
{
    NemoCalendarVCalFormat fmt;
    return fmt.convertEventToVEvent(mEvent,
                                    prodId.isEmpty() ?
                                        QLatin1String("-//NemoMobile.org/Nemo//NONSGML v1.0//EN") :
                                        prodId);
}

void NemoCalendarEvent::setEvent(const KCalCore::Event::Ptr &event)
{
    if (mEvent == event)
        return;

    QString dl = displayLabel();
    QString de = description();
    QDateTime st = startTime();
    QDateTime et = endTime();
    bool ad = allDay();
    Recur re = recur();

    mEvent = event;

    if (displayLabel() != dl) emit displayLabelChanged();
    if (description() != de) emit descriptionChanged();
    if (startTime() != st) emit startTimeChanged();
    if (endTime() != et) emit endTimeChanged();
    if (allDay() != ad) emit allDayChanged();
    if (recur() != re) emit recurChanged();
    emit colorChanged();
}

QString NemoCalendarEvent::location() const
{
    return mEvent ? mEvent->location() : QString();
}

void NemoCalendarEvent::setLocation(const QString &newLocation)
{
    if (newLocation != location() && mEvent) {
        mEvent->setLocation(newLocation);
        emit locationChanged();
    }
}

NemoCalendarEventOccurrence::NemoCalendarEventOccurrence(const mKCal::ExtendedCalendar::ExpandedIncidence &o,
                                                         QObject *parent)
: QObject(parent), mOccurrence(o), mEvent(0)
{
    NemoCalendarEventCache::instance()->mEventOccurrences.insert(this);
}

NemoCalendarEventOccurrence::~NemoCalendarEventOccurrence()
{
    NemoCalendarEventCache::instance()->mEventOccurrences.remove(this);
}

QDateTime NemoCalendarEventOccurrence::startTime() const
{
    return mOccurrence.first.dtStart;
}

QDateTime NemoCalendarEventOccurrence::endTime() const
{
    return mOccurrence.first.dtEnd;
}

NemoCalendarEvent *NemoCalendarEventOccurrence::eventObject()
{
    if (!mEvent) mEvent = new NemoCalendarEvent(mOccurrence.second.dynamicCast<KCalCore::Event>(), this);
    return mEvent;
}

void NemoCalendarEventOccurrence::setEvent(const KCalCore::Event::Ptr &event)
{
    mOccurrence.second = event;
    if (mEvent) mEvent->setEvent(event);
}

// Removes just this occurrence of the event.  If this is a recurring event, it adds an exception for
// this instance
void NemoCalendarEventOccurrence::remove()
{
    if (mOccurrence.second->recurs()) {
        mOccurrence.second->recurrence()->addExDateTime(KDateTime(mOccurrence.first.dtStart,
                                                                  KDateTime::Spec(KDateTime::LocalZone)));
    } else {
        NemoCalendarDb::calendar()->deleteEvent(mOccurrence.second.dynamicCast<KCalCore::Event>());
    }

    // TODO: this sucks
    NemoCalendarDb::storage()->save();
}

