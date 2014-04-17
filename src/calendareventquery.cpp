/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Aaron Kennedy <aaron.kennedy@jollamobile.com>
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

#include "calendareventquery.h"

#include "calendarmanager.h"
#include "calendareventoccurrence.h"

NemoCalendarEventQuery::NemoCalendarEventQuery()
: mIsComplete(true), mOccurrence(0)
{
    connect(NemoCalendarManager::instance(), SIGNAL(dataUpdated()), this, SLOT(refresh()));
    connect(NemoCalendarManager::instance(), SIGNAL(eventUidChanged(QString,QString)),
            this, SLOT(eventUidChanged(QString,QString)));
}

// The uid of the matched event
QString NemoCalendarEventQuery::uniqueId() const
{
    return mUid;
}

void NemoCalendarEventQuery::setUniqueId(const QString &uid)
{
    if (uid == mUid)
        return;
    mUid = uid;
    emit uniqueIdChanged();

    refresh();
}

// The ideal start time of the occurrence.  If there is no occurrence with
// the exact start time, the first occurrence following startTime is returned.
// If there is no following occurrence, the previous occurrence is returned.
QDateTime NemoCalendarEventQuery::startTime() const
{
    return mStartTime;
}

void NemoCalendarEventQuery::setStartTime(const QDateTime &t)
{
    if (t == mStartTime)
        return;
    mStartTime = t;
    emit startTimeChanged();

    refresh();
}

void NemoCalendarEventQuery::resetStartTime()
{
    setStartTime(QDateTime());
}

QObject *NemoCalendarEventQuery::event() const
{
    if (mUid.isEmpty())
        return 0;

    return NemoCalendarManager::instance()->eventObject(mUid);
}

QObject *NemoCalendarEventQuery::occurrence() const
{
    return mOccurrence;
}

void NemoCalendarEventQuery::classBegin()
{
    mIsComplete = false;
}

void NemoCalendarEventQuery::componentComplete()
{
    mIsComplete = true;
    refresh();
}

void NemoCalendarEventQuery::refresh()
{
    if (!mIsComplete || mUid.isEmpty() || !mStartTime.isValid())
        return;

    NemoCalendarEventOccurrence *occurrence = NemoCalendarManager::instance()->getNextOccurence(mUid, mStartTime);

    if (occurrence) {
        mOccurrence = occurrence;
        mOccurrence->setParent(this);
        emit occurrenceChanged();
        emit eventChanged();
    }
}

void NemoCalendarEventQuery::eventUidChanged(QString oldUid, QString newUid)
{
    if (mUid == oldUid)
        mUid = newUid;
}

