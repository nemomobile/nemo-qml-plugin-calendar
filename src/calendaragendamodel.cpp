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

// Qt
#include <QDebug>

// mkcal
#include <event.h>

#include "calendareventcache.h"
#include "calendaragendamodel.h"
#include "calendarevent.h"
#include "calendardb.h"

NemoCalendarAgendaModel::NemoCalendarAgendaModel(QObject *parent)
: QAbstractListModel(parent), mBuffer(0), mIsComplete(true)
{
    mRoleNames[EventObjectRole] = "event";
    mRoleNames[OccurrenceObjectRole] = "occurrence";
    mRoleNames[SectionBucketRole] = "sectionBucket";

#ifndef NEMO_USE_QT5
    setRoleNames(mRoleNames);
#endif

    connect(NemoCalendarEventCache::instance(), SIGNAL(modelReset()), this, SLOT(refresh()));
}

NemoCalendarAgendaModel::~NemoCalendarAgendaModel()
{
    NemoCalendarEventCache::instance()->cancelAgendaRefresh(this);
    qDeleteAll(mEvents);
}

#ifdef NEMO_USE_QT5
QHash<int, QByteArray> NemoCalendarAgendaModel::roleNames() const
{
    return mRoleNames;
}
#endif

QDate NemoCalendarAgendaModel::startDate() const
{
    return mStartDate;
}

void NemoCalendarAgendaModel::setStartDate(const QDate &startDate)
{
    if (mStartDate == startDate)
        return;

    mStartDate = startDate;
    emit startDateChanged();

    refresh();
}

QDate NemoCalendarAgendaModel::endDate() const
{
    return mEndDate;
}

void NemoCalendarAgendaModel::setEndDate(const QDate &endDate)
{
    if (mEndDate == endDate)
        return;

    mEndDate = endDate;
    emit endDateChanged();

    refresh();
}

void NemoCalendarAgendaModel::refresh()
{
    if (!mIsComplete)
        return;

    NemoCalendarEventCache::instance()->scheduleAgendaRefresh(this);
}

static bool eventsEqual(const mKCal::ExtendedCalendar::ExpandedIncidence &e1,
                        const mKCal::ExtendedCalendar::ExpandedIncidence &e2)
{
    return e1.first.dtStart == e2.first.dtStart &&
           e1.first.dtEnd == e2.first.dtEnd &&
           (e1.second == e2.second || (e1.second && e2.second && e1.second->uid() == e2.second->uid()));
}

static bool eventsLessThan(const mKCal::ExtendedCalendar::ExpandedIncidence &e1,
                           const mKCal::ExtendedCalendar::ExpandedIncidence &e2)
{
    if (e1.second.isNull() != e2.second.isNull()) {
        return e1.second.data() < e2.second.data();
    } else if (e1.first.dtStart == e2.first.dtStart) {
        int cmp = QString::compare(e1.second->summary(), e2.second->summary(), Qt::CaseInsensitive);
        if (cmp == 0) return QString::compare(e1.second->uid(), e2.second->uid()) < 0;
        else return cmp < 0;
    } else {
        return e1.first.dtStart < e2.first.dtStart;
    }
}

void NemoCalendarAgendaModel::doRefresh(mKCal::ExtendedCalendar::ExpandedIncidenceList newEvents, bool reset)
{
    // Filter out excluded notebooks
    for (int ii = 0; ii < newEvents.count(); ++ii) {
        if (!NemoCalendarEventCache::instance()->mNotebooks.contains(NemoCalendarDb::calendar()->notebook(newEvents.at(ii).second))) {
            newEvents.remove(ii);
            --ii;
        }
    }

    qSort(newEvents.begin(), newEvents.end(), eventsLessThan);

    int oldEventCount = mEvents.count();

    if (reset) {
        beginResetModel();
        qDeleteAll(mEvents);
        mEvents.clear();
    }

    QList<NemoCalendarEventOccurrence *> events = mEvents;

    int newEventsCounter = 0;
    int eventsCounter = 0;

    int mEventsIndex = 0;

    while (newEventsCounter < newEvents.count() || eventsCounter < events.count()) {
        // Remove old events
        int removeCount = 0;
        while ((eventsCounter + removeCount) < events.count() &&
               (newEventsCounter >= newEvents.count() ||
                eventsLessThan(events.at(eventsCounter + removeCount)->expandedEvent(),
                               newEvents.at(newEventsCounter))))
            removeCount++;

        if (removeCount) {
            Q_ASSERT(false == reset);
            beginRemoveRows(QModelIndex(), mEventsIndex, mEventsIndex + removeCount - 1);
            mEvents.erase(mEvents.begin() + mEventsIndex, mEvents.begin() + mEventsIndex + removeCount);
            endRemoveRows();
            for (int ii = eventsCounter; ii < eventsCounter + removeCount; ++ii)
                delete events.at(ii);
            eventsCounter += removeCount;
        }

        // Skip matching events
        while (eventsCounter < events.count() && newEventsCounter < newEvents.count() &&
               eventsEqual(newEvents.at(newEventsCounter), events.at(eventsCounter)->expandedEvent())) {
            Q_ASSERT(false == reset);
            eventsCounter++;
            newEventsCounter++;
            mEventsIndex++;
        }

        // Insert new events
        int insertCount = 0;
        while ((newEventsCounter + insertCount) < newEvents.count() && 
               (eventsCounter >= events.count() ||
                eventsLessThan(newEvents.at(newEventsCounter + insertCount),
                               events.at(eventsCounter)->expandedEvent())))
            insertCount++;

        if (insertCount) {
            if (!reset) beginInsertRows(QModelIndex(), mEventsIndex, mEventsIndex + insertCount - 1);
            for (int ii = 0; ii < insertCount; ++ii) {
                NemoCalendarEventOccurrence *event = 
                    new NemoCalendarEventOccurrence(newEvents.at(newEventsCounter + ii));
                mEvents.insert(mEventsIndex++, event);
            }
            newEventsCounter += insertCount;
            if (!reset) endInsertRows();
        }
    }

    if (reset)
        endResetModel();

    if (oldEventCount != mEvents.count())
        emit countChanged();
}

int NemoCalendarAgendaModel::count() const
{
    return mEvents.size();
}

int NemoCalendarAgendaModel::minimumBuffer() const
{
    return mBuffer;
}

void NemoCalendarAgendaModel::setMinimumBuffer(int b)
{
    if (mBuffer == b)
        return;

    mBuffer = b;
    emit minimumBufferChanged();
}

int NemoCalendarAgendaModel::startDateIndex() const
{
    return 0;
}

int NemoCalendarAgendaModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return mEvents.size();
}

QVariant NemoCalendarAgendaModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= mEvents.count())
        return QVariant();

    switch (role) {
        case EventObjectRole:
            return QVariant::fromValue<QObject *>(mEvents.at(index.row())->eventObject());
        case OccurrenceObjectRole:
            return QVariant::fromValue<QObject *>(mEvents.at(index.row()));
        case SectionBucketRole:
            return mEvents.at(index.row())->startTime().date();
        default:
            return QVariant();
    }
}

void NemoCalendarAgendaModel::classBegin()
{
    mIsComplete = false;
}

void NemoCalendarAgendaModel::componentComplete()
{
    mIsComplete = true;
    refresh();
}
