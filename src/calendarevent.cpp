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

NemoCalendarEvent::NemoCalendarEvent(QObject *parent)
    : QObject(parent)
    , mEvent(KCalCore::Event::Ptr(new KCalCore::Event))
{
}

NemoCalendarEvent::NemoCalendarEvent(const KCalCore::Event::Ptr &event, QObject *parent)
    : QObject(parent)
    , mEvent(event)
{
}

QString NemoCalendarEvent::displayLabel() const
{
    return mEvent->summary();
}

void NemoCalendarEvent::setDisplayLabel(const QString &displayLabel)
{
    if (mEvent->summary() == displayLabel)
        return;

    mEvent->setSummary(displayLabel);
    emit displayLabelChanged();
}

QString NemoCalendarEvent::description() const
{
    return mEvent->description();
}

void NemoCalendarEvent::setDescription(const QString &description)
{
    if (mEvent->description() == description)
        return;

    mEvent->setDescription(description);
    emit descriptionChanged();
}

QDateTime NemoCalendarEvent::startTime() const
{
    return mEvent->dtStart().dateTime();
}

void NemoCalendarEvent::setStartTime(const QDateTime &startTime)
{
    if (mEvent->dtStart().dateTime() == startTime)
        return;

    mEvent->setDtStart(KDateTime(startTime));
    emit startTimeChanged();
}

QDateTime NemoCalendarEvent::endTime() const
{
    return mEvent->dtEnd().dateTime();
}

void NemoCalendarEvent::setEndTime(const QDateTime &endTime)
{
    if (mEvent->dtEnd().dateTime() == endTime)
        return;

    mEvent->setDtEnd(KDateTime(endTime));
    emit endTimeChanged();
}

bool NemoCalendarEvent::allDay() const
{
    return mEvent->allDay();
}

void NemoCalendarEvent::setAllDay(bool a)
{
    if (allDay() == a)
        return;

    mEvent->setAllDay(a);
    emit allDayChanged();
}

NemoCalendarEvent::Recur NemoCalendarEvent::recur() const
{
    if (mEvent->recurs()) {
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
    }

    emit recurChanged();
}

void NemoCalendarEvent::save()
{
    if (mEvent->revision() == 0) {
        NemoCalendarDb::calendar()->addEvent(mEvent, NemoCalendarDb::storage()->defaultNotebook()->uid());
    } else {
        mEvent->setRevision(mEvent->revision() + 1);
    }

    //
    // TODO: this sucks
    NemoCalendarDb::storage()->save();
}

NemoCalendarEventOccurrence::NemoCalendarEventOccurrence(const mKCal::ExtendedCalendar::ExpandedIncidence &o,
                                                         QObject *parent)
: QObject(parent), mOccurrence(o), mEvent(0)
{
}

QDateTime NemoCalendarEventOccurrence::startTime() const
{
    return mOccurrence.first.dtStart;
}

QDateTime NemoCalendarEventOccurrence::endTime() const
{
    return mOccurrence.first.dtEnd;
}

NemoCalendarEvent *NemoCalendarEventOccurrence::event()
{
    if (!mEvent) mEvent = new NemoCalendarEvent(mOccurrence.second.dynamicCast<KCalCore::Event>(), this);
    return mEvent;
}

