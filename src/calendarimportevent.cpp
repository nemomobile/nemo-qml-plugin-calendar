/*
 * Copyright (C) 2015 Jolla Ltd.
 * Contact: Petri M. Gerdt <petri.gerdt@jollamobile.com>
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

#include "calendarimportevent.h"

#include "calendareventoccurrence.h"
#include "calendarutils.h"

CalendarImportEvent::CalendarImportEvent(KCalCore::Event::Ptr event)
    : QObject(),
      mEvent(event),
      mColor("#ffffff")
{
}

QString CalendarImportEvent::displayLabel() const
{
    if (!mEvent)
        return "";

    return mEvent->summary();
}

QString CalendarImportEvent::description() const
{
    if (!mEvent)
        return "";

    return mEvent->description();
}

QDateTime CalendarImportEvent::startTime() const
{
    if (!mEvent)
        return QDateTime();

    return mEvent->dtStart().dateTime();
}

QDateTime CalendarImportEvent::endTime() const
{
    if (!mEvent)
        return QDateTime();

    return mEvent->dtEnd().dateTime();
}

bool CalendarImportEvent::allDay() const
{
    if (!mEvent)
        return false;

    return mEvent->allDay();
}

NemoCalendarEvent::Recur CalendarImportEvent::recur()
{
    if (!mEvent)
        return NemoCalendarEvent::RecurOnce;

    return NemoCalendarUtils::convertRecurrence(mEvent);
}

NemoCalendarEvent::Reminder CalendarImportEvent::reminder() const
{
    if (!mEvent)
        return NemoCalendarEvent::ReminderNone;

    return NemoCalendarUtils::getReminder(mEvent);
}

QString CalendarImportEvent::uniqueId() const
{
    if (!mEvent)
        return "";

    return mEvent->uid();
}

QString CalendarImportEvent::color() const
{
    return mColor;
}

QString CalendarImportEvent::location() const
{
    if (!mEvent)
        return "";

    return mEvent->location();
}

QList<QObject *> CalendarImportEvent::attendees() const
{
    if (!mEvent)
        return QList<QObject *>();

    return NemoCalendarUtils::convertAttendeeList(NemoCalendarUtils::getEventAttendees(mEvent));
}

void CalendarImportEvent::setColor(const QString &color)
{
    if (mColor == color)
        return;

    mColor = color;
    emit colorChanged();
}

QObject *CalendarImportEvent::nextOccurrence()
{
    if (!mEvent)
        return 0;

    NemoCalendarData::EventOccurrence eo = NemoCalendarUtils::getNextOccurrence(mEvent);
    return new NemoCalendarEventOccurrence(eo.eventUid,
                                           eo.recurrenceId,
                                           eo.startTime,
                                           eo.endTime);
}
