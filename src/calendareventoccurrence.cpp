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

#include "calendareventoccurrence.h"

#include "calendarevent.h"
#include "calendarmanager.h"

NemoCalendarEventOccurrence::NemoCalendarEventOccurrence(const QString &eventUid,
                                                         const QDateTime &startTime,
                                                         const QDateTime &endTime,
                                                         QObject *parent)
    : QObject(parent), mEventUid(eventUid), mStartTime(startTime), mEndTime(endTime)
{
}

NemoCalendarEventOccurrence::~NemoCalendarEventOccurrence()
{
}

QDateTime NemoCalendarEventOccurrence::startTime() const
{
    return mStartTime;
}

QDateTime NemoCalendarEventOccurrence::endTime() const
{
    return mEndTime;
}

NemoCalendarEvent *NemoCalendarEventOccurrence::eventObject() const
{
    return NemoCalendarManager::instance()->eventObject(mEventUid);
}

// Removes just this occurrence of the event.  If this is a recurring event, it adds an exception for
// this instance
void NemoCalendarEventOccurrence::remove()
{
    NemoCalendarManager::instance()->deleteEvent(mEventUid, mStartTime);
    NemoCalendarManager::instance()->save();
}
