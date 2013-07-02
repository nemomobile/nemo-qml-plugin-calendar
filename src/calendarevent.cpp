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

#include <QQmlInfo>
#include "calendardb.h"
#include "calendareventcache.h"

NemoCalendarEvent::NemoCalendarEvent(QObject *parent)
: QObject(parent), mEvent(KCalCore::Event::Ptr(new KCalCore::Event))
{
    NemoCalendarEventCache::instance()->mEvents.insert(this);
}

NemoCalendarEvent::NemoCalendarEvent(const KCalCore::Event::Ptr &event, QObject *parent)
: QObject(parent), mEvent(event)
{
    NemoCalendarEventCache::instance()->mEvents.insert(this);
}

NemoCalendarEvent::~NemoCalendarEvent()
{
    NemoCalendarEventCache::instance()->mEvents.remove(this);
}

QString NemoCalendarEvent::displayLabel() const
{
    return mEvent?mEvent->summary():QString();
}

void NemoCalendarEvent::setDisplayLabel(const QString &displayLabel)
{
    if (!mEvent || mEvent->summary() == displayLabel)
        return;

    mEvent->setSummary(displayLabel);
    emit displayLabelChanged();
}

QString NemoCalendarEvent::description() const
{
    return mEvent?mEvent->description():QString();
}

void NemoCalendarEvent::setDescription(const QString &description)
{
    if (!mEvent || mEvent->description() == description)
        return;

    mEvent->setDescription(description);
    emit descriptionChanged();
}

QDateTime NemoCalendarEvent::startTime() const
{
    return mEvent?mEvent->dtStart().dateTime():QDateTime();
}

void NemoCalendarEvent::setStartTime(const QDateTime &startTime)
{
    if (!mEvent || mEvent->dtStart().dateTime() == startTime)
        return;

    mEvent->setDtStart(KDateTime(startTime));
    emit startTimeChanged();
}

QDateTime NemoCalendarEvent::endTime() const
{
    return mEvent?mEvent->dtEnd().dateTime():QDateTime();
}

void NemoCalendarEvent::setEndTime(const QDateTime &endTime)
{
    if (!mEvent || mEvent->dtEnd().dateTime() == endTime)
        return;

    mEvent->setDtEnd(KDateTime(endTime));
    emit endTimeChanged();
}

bool NemoCalendarEvent::allDay() const
{
    return mEvent?mEvent->allDay():false;
}

void NemoCalendarEvent::setAllDay(bool a)
{
    if (!mEvent || allDay() == a)
        return;

    mEvent->setAllDay(a);
    emit allDayChanged();
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

    if (r == RecurCustom) {
        qmlInfo(this) << "Cannot assign RecurCustom";
        r = RecurOnce;
    }

    if (r == RecurOnce)
        mEvent->recurrence()->clear();

    if (oldRecur == r)
        return;

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

    emit recurChanged();
}

QString NemoCalendarEvent::uniqueId() const
{
    return mEvent->uid();
}

QString NemoCalendarEvent::color() const
{
    return "#00aeef"; // TODO: hardcoded, as we only support local events for now
}

void NemoCalendarEvent::save()
{
    if (mEvent->revision() == 0) {
        NemoCalendarDb::calendar()->addEvent(mEvent, NemoCalendarDb::storage()->defaultNotebook()->uid());
    } else {
        mEvent->setRevision(mEvent->revision() + 1);
    }

    // TODO: this sucks
    NemoCalendarDb::storage()->save();
}

// Removes the entire event
void NemoCalendarEvent::remove()
{
    NemoCalendarDb::calendar()->deleteEvent(mEvent);

    // TODO: this sucks
    NemoCalendarDb::storage()->save();
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

