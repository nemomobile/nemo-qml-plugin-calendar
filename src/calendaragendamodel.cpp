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


#include "calendaragendamodel.h"

#include "calendarevent.h"
#include "calendareventoccurrence.h"
#include "calendarmanager.h"

NemoCalendarAgendaModel::NemoCalendarAgendaModel(QObject *parent)
    : QAbstractListModel(parent), mIsComplete(true), mFilterMode(FilterNone)
{
    connect(NemoCalendarManager::instance(), SIGNAL(storageModified()), this, SLOT(refresh()));
    connect(NemoCalendarManager::instance(), SIGNAL(dataUpdated()), this, SLOT(refresh()));
}

NemoCalendarAgendaModel::~NemoCalendarAgendaModel()
{
    NemoCalendarManager::instance()->cancelAgendaRefresh(this);
    qDeleteAll(mEvents);
    mEvents.clear();
}

QHash<int, QByteArray> NemoCalendarAgendaModel::roleNames() const
{
    QHash<int,QByteArray> roleNames;
    roleNames[EventObjectRole] = "event";
    roleNames[OccurrenceObjectRole] = "occurrence";
    roleNames[SectionBucketRole] = "sectionBucket";
    return roleNames;
}

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

    NemoCalendarManager::instance()->scheduleAgendaRefresh(this);
}

static bool eventsEqual(const NemoCalendarEventOccurrence *e1,
                        const NemoCalendarEventOccurrence *e2)
{
    return e1->startTime() == e2->startTime() &&
           e1->endTime() == e2->endTime() &&
           (e1->eventObject() && e2->eventObject() && e1->eventObject()->uniqueId() == e2->eventObject()->uniqueId());
}

static bool eventsLessThan(const NemoCalendarEventOccurrence *e1,
                           const NemoCalendarEventOccurrence *e2)
{
    if (e1->startTime() == e2->startTime()) {
        int cmp = QString::compare(e1->eventObject()->displayLabel(),
                                   e2->eventObject()->displayLabel(),
                                   Qt::CaseInsensitive);
        if (cmp == 0)
            return QString::compare(e1->eventObject()->uniqueId(), e2->eventObject()->uniqueId()) < 0;
        else
            return cmp < 0;
    } else {
        return e1->startTime() < e2->startTime();
    }
}

void NemoCalendarAgendaModel::doRefresh(QList<NemoCalendarEventOccurrence *> newEvents)
{
    QList<NemoCalendarEventOccurrence *> events = mEvents;
    QList<NemoCalendarEventOccurrence *> skippedEvents;

    // filter out if necessary
    if (mFilterMode == FilterNonAllDay) {
        QList<NemoCalendarEventOccurrence *>::iterator it = newEvents.begin();
        while (it != newEvents.end()) {
            if (!(*it)->eventObject()->allDay()) {
                skippedEvents.append(*it);
                it = newEvents.erase(it);
            } else {
                it++;
            }
        }
    }

    qSort(newEvents.begin(), newEvents.end(), eventsLessThan);

    int oldEventCount = mEvents.count();
    int newEventsCounter = 0;
    int eventsCounter = 0;
    int mEventsIndex = 0;

    while (newEventsCounter < newEvents.count() || eventsCounter < events.count()) {
        // Remove old events
        int removeCount = 0;
        while ((eventsCounter + removeCount) < events.count()
                && (newEventsCounter >= newEvents.count()
                    || eventsLessThan(events.at(eventsCounter + removeCount),
                                      newEvents.at(newEventsCounter)))) {
            removeCount++;
        }

        if (removeCount) {
            beginRemoveRows(QModelIndex(), mEventsIndex, mEventsIndex + removeCount - 1);
            mEvents.erase(mEvents.begin() + mEventsIndex, mEvents.begin() + mEventsIndex + removeCount);
            endRemoveRows();
            for (int ii = eventsCounter; ii < eventsCounter + removeCount; ++ii)
                delete events.at(ii);
            eventsCounter += removeCount;
        }

        // Skip matching events
        while (eventsCounter < events.count() && newEventsCounter < newEvents.count() &&
               eventsEqual(newEvents.at(newEventsCounter), events.at(eventsCounter))) {
            skippedEvents.append(newEvents.at(newEventsCounter));
            eventsCounter++;
            newEventsCounter++;
            mEventsIndex++;
        }

        // Insert new events
        int insertCount = 0;
        while ((newEventsCounter + insertCount) < newEvents.count()
               && (eventsCounter >= events.count()
                   || !(eventsLessThan(events.at(eventsCounter),
                                       newEvents.at(newEventsCounter + insertCount))))) {
            insertCount++;
        }

        if (insertCount) {
            beginInsertRows(QModelIndex(), mEventsIndex, mEventsIndex + insertCount - 1);
            for (int ii = 0; ii < insertCount; ++ii) {
                mEvents.insert(mEventsIndex++, newEvents.at(newEventsCounter + ii));
            }
            newEventsCounter += insertCount;
            endInsertRows();
        }
    }

    qDeleteAll(skippedEvents);

    if (oldEventCount != mEvents.count())
        emit countChanged();

    emit updated();
}

int NemoCalendarAgendaModel::count() const
{
    return mEvents.size();
}

int NemoCalendarAgendaModel::filterMode() const
{
    return mFilterMode;
}

void NemoCalendarAgendaModel::setFilterMode(int mode)
{
    if (mode != mFilterMode) {
        mFilterMode = mode;
        emit filterModeChanged();
        refresh();
    }
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
