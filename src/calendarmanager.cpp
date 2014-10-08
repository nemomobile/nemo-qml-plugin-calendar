/*
 * Copyright (C) 2014 Jolla Ltd.
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

#include "calendarmanager.h"

#include <QDebug>

#include "calendarworker.h"
#include "calendarevent.h"
#include "calendaragendamodel.h"
#include "calendareventoccurrence.h"
#include "calendareventquery.h"
#include "calendarchangeinformation.h"

// kCalCore
#include <calformat.h>

NemoCalendarManager::NemoCalendarManager() :
    QObject(0), mLoadPending(false), mResetPending(false)
{
    qRegisterMetaType<KDateTime>("KDateTime");
    qRegisterMetaType<QList<KDateTime> >("QList<KDateTime>");
    qRegisterMetaType<NemoCalendarEvent::Recur>("NemoCalendarEvent::Recur");
    qRegisterMetaType<NemoCalendarEvent::Reminder>("NemoCalendarEvent::Reminder");
    qRegisterMetaType<QHash<QString,NemoCalendarData::EventOccurrence> >("QHash<QString,NemoCalendarData::EventOccurrence>");
    qRegisterMetaType<NemoCalendarData::Event>("NemoCalendarData::Event");
    qRegisterMetaType<QMultiHash<QString,NemoCalendarData::Event> >("QMultiHash<QString,NemoCalendarData::Event>");
    qRegisterMetaType<QHash<QDate,QStringList> >("QHash<QDate,QStringList>");
    qRegisterMetaType<NemoCalendarData::Range>("NemoCalendarData::Range");
    qRegisterMetaType<QList<NemoCalendarData::Range > >("QList<NemoCalendarData::Range>");
    qRegisterMetaType<QList<NemoCalendarData::Notebook> >("QList<NemoCalendarData::Notebook>");

    mCalendarWorker = new NemoCalendarWorker();
    mCalendarWorker->moveToThread(&mWorkerThread);

    connect(&mWorkerThread, SIGNAL(finished()), mCalendarWorker, SLOT(deleteLater()));

    connect(mCalendarWorker, SIGNAL(storageModifiedSignal(QString)),
            this, SLOT(storageModifiedSlot(QString)));

    connect(mCalendarWorker, SIGNAL(eventNotebookChanged(QString,QString,QString)),
            this, SLOT(eventNotebookChanged(QString,QString,QString)));

    connect(mCalendarWorker, SIGNAL(excludedNotebooksChanged(QStringList)),
            this, SLOT(excludedNotebooksChangedSlot(QStringList)));
    connect(mCalendarWorker, SIGNAL(notebooksChanged(QList<NemoCalendarData::Notebook>)),
            this, SLOT(notebooksChangedSlot(QList<NemoCalendarData::Notebook>)));

    connect(mCalendarWorker, SIGNAL(dataLoaded(QList<NemoCalendarData::Range>,QStringList,QMultiHash<QString,NemoCalendarData::Event>,QHash<QString,NemoCalendarData::EventOccurrence>,QHash<QDate,QStringList>,bool)),
            this, SLOT(dataLoadedSlot(QList<NemoCalendarData::Range>,QStringList,QMultiHash<QString,NemoCalendarData::Event>,QHash<QString,NemoCalendarData::EventOccurrence>,QHash<QDate,QStringList>,bool)));

    connect(mCalendarWorker, SIGNAL(occurrenceExceptionFailed(NemoCalendarData::Event,QDateTime)),
            this, SLOT(occurrenceExceptionFailedSlot(NemoCalendarData::Event,QDateTime)));
    connect(mCalendarWorker, SIGNAL(occurrenceExceptionCreated(NemoCalendarData::Event,QDateTime,KDateTime)),
            this, SLOT(occurrenceExceptionCreatedSlot(NemoCalendarData::Event,QDateTime,KDateTime)));

    mWorkerThread.setObjectName("calendarworker");
    mWorkerThread.start();

    QMetaObject::invokeMethod(mCalendarWorker, "init", Qt::QueuedConnection);

    mTimer = new QTimer(this);
    mTimer->setSingleShot(true);
    mTimer->setInterval(5);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(timeout()));
}

NemoCalendarManager *NemoCalendarManager::instance()
{
    static NemoCalendarManager *managerInstance;
    if (!managerInstance)
        managerInstance = new NemoCalendarManager;

    return managerInstance;
}

NemoCalendarManager::~NemoCalendarManager()
{
    mWorkerThread.quit();
    mWorkerThread.wait();
}

QList<NemoCalendarData::Notebook> NemoCalendarManager::notebooks()
{
    return mNotebooks.values();
}

QString NemoCalendarManager::defaultNotebook() const
{
    foreach (const NemoCalendarData::Notebook &notebook, mNotebooks) {
        if (notebook.isDefault)
            return notebook.uid;
    }
    return "";
}

void NemoCalendarManager::setDefaultNotebook(const QString &notebookUid)
{
    QMetaObject::invokeMethod(mCalendarWorker, "setDefaultNotebook", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid));
}

NemoCalendarEvent* NemoCalendarManager::eventObject(const QString &eventUid, const KDateTime &recurrenceId)
{
    NemoCalendarData::Event event = getEvent(eventUid, recurrenceId);
    if (event.isValid()) {
        QMultiHash<QString, NemoCalendarEvent *>::iterator it = mEventObjects.find(eventUid);
        while (it != mEventObjects.end() && it.key() == eventUid) {
            if ((*it)->recurrenceId() == recurrenceId) {
                return *it;
            }
            ++it;
        }

        NemoCalendarEvent *calendarEvent = new NemoCalendarEvent(this, eventUid, recurrenceId);
        mEventObjects.insert(eventUid, calendarEvent);
        return calendarEvent;
    }

    // TODO: maybe attempt to read event from DB? This situation should not happen.
    qWarning() << Q_FUNC_INFO << "No event with uid" << eventUid << ", returning empty event";

    return new NemoCalendarEvent(this, "", KDateTime());
}

void NemoCalendarManager::saveModification(NemoCalendarData::Event eventData)
{
    QMetaObject::invokeMethod(mCalendarWorker, "saveEvent", Qt::QueuedConnection,
                              Q_ARG(NemoCalendarData::Event, eventData));
}

// caller owns returned object
NemoCalendarChangeInformation *
NemoCalendarManager::replaceOccurrence(NemoCalendarData::Event eventData, NemoCalendarEventOccurrence *occurrence)
{
    if (!occurrence) {
        qWarning() << Q_FUNC_INFO << "no occurrence given";
        return 0;
    }

    if (eventData.uniqueId.isEmpty()) {
        qWarning("NemocalendarManager::replaceOccurrence() - empty uid given");
        return 0;
    }

    // save request information for signal handling
    NemoCalendarChangeInformation *changes = new NemoCalendarChangeInformation;
    OccurrenceData changeData = { eventData, occurrence->startTime(), changes };
    mPendingOccurrenceExceptions.append(changeData);

    QMetaObject::invokeMethod(mCalendarWorker, "replaceOccurrence", Qt::QueuedConnection,
                              Q_ARG(NemoCalendarData::Event, eventData),
                              Q_ARG(QDateTime, occurrence->startTime()));
    return changes;
}

QStringList NemoCalendarManager::excludedNotebooks()
{
    return mExcludedNotebooks;
}

void NemoCalendarManager::setExcludedNotebooks(const QStringList &list)
{
    QStringList sorted;
    sorted.append(list);
    sorted.sort();
    if (mExcludedNotebooks == sorted)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setExcludedNotebooks", Qt::QueuedConnection,
                              Q_ARG(QStringList, sorted));
}

void NemoCalendarManager::excludeNotebook(const QString &notebookUid, bool exclude)
{
    QMetaObject::invokeMethod(mCalendarWorker, "excludeNotebook", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid),
                              Q_ARG(bool, exclude));
}

void NemoCalendarManager::setNotebookColor(const QString &notebookUid, const QString &color)
{
    QMetaObject::invokeMethod(mCalendarWorker, "setNotebookColor", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid),
                              Q_ARG(QString, color));
}

QString NemoCalendarManager::getNotebookColor(const QString &notebookUid) const
{
    if (mNotebooks.contains(notebookUid))
        return mNotebooks.value(notebookUid, NemoCalendarData::Notebook()).color;
    else
        return "";
}

void NemoCalendarManager::cancelAgendaRefresh(NemoCalendarAgendaModel *model)
{
    mAgendaRefreshList.removeOne(model);
}

void NemoCalendarManager::scheduleAgendaRefresh(NemoCalendarAgendaModel *model)
{
    if (mAgendaRefreshList.contains(model))
        return;

    mAgendaRefreshList.append(model);

    if (!mLoadPending)
        mTimer->start();
}

void NemoCalendarManager::registerEventQuery(NemoCalendarEventQuery *query)
{
    mQueryList.append(query);
}

void NemoCalendarManager::unRegisterEventQuery(NemoCalendarEventQuery *query)
{
    mQueryList.removeOne(query);
}

void NemoCalendarManager::scheduleEventQueryRefresh(NemoCalendarEventQuery *query)
{
    if (mQueryRefreshList.contains(query))
        return;

    mQueryRefreshList.append(query);

    if (!mLoadPending)
        mTimer->start();
}

static QDate agenda_endDate(const NemoCalendarAgendaModel *model)
{
    QDate endDate = model->endDate();
    return endDate.isValid() ? endDate: model->startDate();
}

bool NemoCalendarManager::isRangeLoaded(const NemoCalendarData::Range &r, QList<NemoCalendarData::Range> *missingRanges)
{
    missingRanges->clear();
    // Range not loaded, no stored data
    if (mLoadedRanges.isEmpty()) {
        missingRanges->append(NemoCalendarData::Range());
        missingRanges->last().first = r.first;
        missingRanges->last().second = r.second;
        return false;
    }

    QDate start(r.first);
    foreach (const NemoCalendarData::Range range, mLoadedRanges) {
        // Range already loaded
        if (start >= range.first && r.second <= range.second)
            return missingRanges->isEmpty();

        // Legend:
        // * |------|: time scale
        // * x: existing loaded period
        // * n: new period
        // * l: to be loaded
        //
        // Period partially loaded: end available
        //    nnnnnn
        // |------xxxxxxx------------|
        //    llll
        //
        // or beginning and end available, middle missing
        //             nnnnnn
        // |------xxxxxxx---xxxxx----|
        //               lll
        if (start < range.first && r.second <= range.second) {
            missingRanges->append(NemoCalendarData::Range());
            missingRanges->last().first = start;
            if (r.second < range.first)
                missingRanges->last().second = r.second;
            else
                missingRanges->last().second = range.first.addDays(-1);

            return false;
        }

        // Period partially loaded: beginning available, end missing
        //           nnnnnn
        // |------xxxxxxx------------|
        //               lll
        if ((start >= range.first && start <= range.second) && r.second > range.second)
            start = range.second.addDays(1);

        // Period partially loaded, period will be splitted into two or more subperiods
        //              nnnnnnnnnnn
        // |------xxxxxxx---xxxxx----|
        //               lll     ll
        if (start < range.first && range.second < r.second) {
            missingRanges->append(NemoCalendarData::Range());
            missingRanges->last().first = start;
            missingRanges->last().second = range.first.addDays(-1);
            start = range.second.addDays(1);
        }
    }

    missingRanges->append(NemoCalendarData::Range());
    missingRanges->last().first = start;
    missingRanges->last().second = r.second;

    return false;
}

static bool range_lessThan(NemoCalendarData::Range lhs, NemoCalendarData::Range rhs)
{
    return lhs.first < rhs.first;
}

QList<NemoCalendarData::Range> NemoCalendarManager::addRanges(const QList<NemoCalendarData::Range> &oldRanges,
                                                              const QList<NemoCalendarData::Range> &newRanges)
{
    if (newRanges.isEmpty() && oldRanges.isEmpty())
        return oldRanges;

    // sort
    QList<NemoCalendarData::Range> sortedRanges;
    sortedRanges.append(oldRanges);
    sortedRanges.append(newRanges);
    qSort(sortedRanges.begin(), sortedRanges.end(), range_lessThan);

    // combine
    QList<NemoCalendarData::Range> combinedRanges;
    combinedRanges.append(sortedRanges.first());

    for (int i = 1; i < sortedRanges.count(); ++i) {
        NemoCalendarData::Range r = sortedRanges.at(i);
        if (combinedRanges.last().second.addDays(1) >= r.first)
            combinedRanges.last().second = qMax(combinedRanges.last().second, r.second);
        else
            combinedRanges.append(r);
    }

    return combinedRanges;
}

void NemoCalendarManager::updateAgendaModel(NemoCalendarAgendaModel *model)
{
    QList<NemoCalendarEventOccurrence*> filtered;
    if (model->startDate() == model->endDate() || !model->endDate().isValid()) {
        foreach (const QString &id, mEventOccurrenceForDates.value(model->startDate())) {
            if (mEventOccurrences.contains(id)) {
                NemoCalendarData::EventOccurrence eo = mEventOccurrences.value(id);
                filtered.append(new NemoCalendarEventOccurrence(eo.eventUid, eo.recurrenceId,
                                                                eo.startTime, eo.endTime));
            } else {
                qWarning() << "no occurrence with id" << id;
            }
        }
    } else {
        foreach (const NemoCalendarData::EventOccurrence &eo, mEventOccurrences.values()) {
            NemoCalendarEvent *event = eventObject(eo.eventUid, eo.recurrenceId);
            if (!event) {
                qWarning() << "no event for occurrence";
                continue;
            }
            QDate start = model->startDate();
            QDate end = model->endDate();

            // on all day events the end time is inclusive, otherwise not
            if ((eo.startTime.date() < start
                 && (eo.endTime.date() > start
                     || (eo.endTime.date() == start && (event->allDay()
                                                        || eo.endTime.time() > QTime(0, 0)))))
                    || (eo.startTime.date() >= start && eo.startTime.date() <= end)) {
                filtered.append(new NemoCalendarEventOccurrence(eo.eventUid, eo.recurrenceId,
                                                                eo.startTime, eo.endTime));
            }
        }
    }

    model->doRefresh(filtered);
}

void NemoCalendarManager::doAgendaAndQueryRefresh()
{
    QList<NemoCalendarAgendaModel *> agendaModels = mAgendaRefreshList;
    mAgendaRefreshList.clear();
    QList<NemoCalendarData::Range> missingRanges;
    foreach (NemoCalendarAgendaModel *model, agendaModels) {
        NemoCalendarData::Range range;
        range.first = model->startDate();
        range.second = agenda_endDate(model);
        QList<NemoCalendarData::Range> newRanges;
        if (isRangeLoaded(range, &newRanges))
            updateAgendaModel(model);
        else
            missingRanges = addRanges(missingRanges, newRanges);
    }

    if (mResetPending) {
        missingRanges = addRanges(missingRanges, mLoadedRanges);
        mLoadedRanges.clear();
        mLoadedQueries.clear();
    }

    QList<NemoCalendarEventQuery *> queryList = mQueryRefreshList;
    mQueryRefreshList.clear();
    QStringList missingUidList;
    foreach (NemoCalendarEventQuery *query, queryList) {
        QString eventUid = query->uniqueId();
        if (eventUid.isEmpty())
            continue;

        KDateTime recurrenceId = query->recurrenceId();
        NemoCalendarData::Event event = getEvent(eventUid, recurrenceId);
        if (event.uniqueId.isEmpty()
                && !mLoadedQueries.contains(eventUid)
                && !missingUidList.contains(eventUid)) {
            missingUidList << eventUid;
        }
        query->doRefresh(event);

        if (mResetPending && !missingUidList.contains(eventUid))
            missingUidList << eventUid;
    }

    if (!missingRanges.isEmpty() || !missingUidList.isEmpty()) {
        mLoadPending = true;
        QMetaObject::invokeMethod(mCalendarWorker, "loadData", Qt::QueuedConnection,
                                  Q_ARG(QList<NemoCalendarData::Range>, missingRanges),
                                  Q_ARG(QStringList, missingUidList),
                                  Q_ARG(bool, mResetPending));
        mResetPending = false;
    }
}

void NemoCalendarManager::timeout() {
    if (mLoadPending)
        return;

    if (!mAgendaRefreshList.isEmpty() || !mQueryRefreshList.isEmpty() || mResetPending)
        doAgendaAndQueryRefresh();
}

void NemoCalendarManager::occurrenceExceptionFailedSlot(NemoCalendarData::Event data, QDateTime occurrence)
{
    for (int i = 0; i < mPendingOccurrenceExceptions.length(); ++i) {
        const OccurrenceData &item = mPendingOccurrenceExceptions.at(i);
        if (item.event == data && item.occurrenceTime == occurrence) {
            if (item.changeObject) {
                item.changeObject->setInformation(QString(), KDateTime());
            }
            mPendingOccurrenceExceptions.removeAt(i);
            break;
        }
    }
}

void NemoCalendarManager::occurrenceExceptionCreatedSlot(NemoCalendarData::Event data, QDateTime occurrence,
                                                         KDateTime newRecurrenceId)
{
    for (int i = 0; i < mPendingOccurrenceExceptions.length(); ++i) {
        const OccurrenceData &item = mPendingOccurrenceExceptions.at(i);
        if (item.event == data && item.occurrenceTime == occurrence) {
            if (item.changeObject) {
                item.changeObject->setInformation(data.uniqueId, newRecurrenceId);
            }

            mPendingOccurrenceExceptions.removeAt(i);
            break;
        }
    }

}

void NemoCalendarManager::deleteEvent(const QString &uid, const KDateTime &recurrenceId, const QDateTime &time)
{
    QMetaObject::invokeMethod(mCalendarWorker, "deleteEvent", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(KDateTime, recurrenceId),
                              Q_ARG(QDateTime, time));
}

void NemoCalendarManager::deleteAll(const QString &uid)
{
    QMetaObject::invokeMethod(mCalendarWorker, "deleteAll", Qt::QueuedConnection,
                              Q_ARG(QString, uid));
}

void NemoCalendarManager::save()
{
    QMetaObject::invokeMethod(mCalendarWorker, "save", Qt::QueuedConnection);
}

QString NemoCalendarManager::convertEventToVCalendarSync(const QString &uid, const QString &prodId)
{
    QString vEvent;
    QMetaObject::invokeMethod(mCalendarWorker, "convertEventToVCalendar",Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QString, vEvent),
                              Q_ARG(QString, uid),
                              Q_ARG(QString, prodId));
    return vEvent;
}

NemoCalendarData::Event NemoCalendarManager::getEvent(const QString &uid, const KDateTime &recurrenceId)
{
    QMultiHash<QString, NemoCalendarData::Event>::iterator it = mEvents.find(uid);
    while (it != mEvents.end() && it.key() == uid) {
        if (it.value().recurrenceId == recurrenceId) {
            return it.value();
        }
        ++it;
    }

    return NemoCalendarData::Event();
}

void NemoCalendarManager::storageModifiedSlot(QString info)
{
    Q_UNUSED(info)
    mResetPending = true;
    emit storageModified();
}

void NemoCalendarManager::eventNotebookChanged(QString oldEventUid, QString newEventUid, QString notebookUid)
{
    // FIXME: adapt to multihash + recurrenceId.
#if 0
    if (mEvents.contains(oldEventUid)) {
        mEvents.insert(newEventUid, mEvents.value(oldEventUid));
        mEvents[newEventUid].calendarUid = notebookUid;
        mEvents.remove(oldEventUid);
    }
    if (mEventObjects.contains(oldEventUid)) {
        mEventObjects.insert(newEventUid, mEventObjects.value(oldEventUid));
        mEventObjects.remove(oldEventUid);
    }
    foreach (QString occurrenceUid, mEventOccurrences.keys()) {
        if (mEventOccurrences.value(occurrenceUid).eventUid == oldEventUid)
            mEventOccurrences[occurrenceUid].eventUid = newEventUid;
    }

    emit eventUidChanged(oldEventUid, newEventUid);

    // Event uid is changed when events are moved between notebooks, the notebook color
    // associated with this event has changed. Emit color changed after emitting eventUidChanged,
    // so that data models have the correct event uid to use when querying for NemoCalendarEvent
    // instances, see NemoCalendarEventOccurrence::eventObject(), used by NemoCalendarAgendaModel.
    NemoCalendarEvent *eventObject = mEventObjects.value(newEventUid);
    if (eventObject)
        emit eventObject->colorChanged();
#else
    Q_UNUSED(oldEventUid)
    Q_UNUSED(newEventUid)
    Q_UNUSED(notebookUid)
#endif
}

void NemoCalendarManager::excludedNotebooksChangedSlot(QStringList excludedNotebooks)
{
    QStringList sortedExcluded = excludedNotebooks;
    sortedExcluded.sort();
    if (mExcludedNotebooks != sortedExcluded) {
        mExcludedNotebooks = sortedExcluded;
        emit excludedNotebooksChanged(mExcludedNotebooks);
        mResetPending = true;
        mTimer->start();
    }
}

void NemoCalendarManager::notebooksChangedSlot(QList<NemoCalendarData::Notebook> notebooks)
{
    QHash<QString, NemoCalendarData::Notebook> newNotebooks;
    QStringList colorChangers;
    QString newDefaultNotebookUid;
    bool changed = false;
    foreach (const NemoCalendarData::Notebook &notebook, notebooks) {
        if (mNotebooks.contains(notebook.uid)) {
            if (mNotebooks.value(notebook.uid) != notebook) {
                changed = true;
                if (mNotebooks.value(notebook.uid).color != notebook.color)
                    colorChangers << notebook.uid;
            }
        }
        if (notebook.isDefault) {
            if (!mNotebooks.contains(notebook.uid) || !mNotebooks.value(notebook.uid).isDefault)
                newDefaultNotebookUid = notebook.uid;
        }

        newNotebooks.insert(notebook.uid, notebook);
    }

    if (changed || mNotebooks.count() != newNotebooks.count()) {
        emit notebooksAboutToChange();
        mNotebooks = newNotebooks;
        emit notebooksChanged(mNotebooks.values());
        foreach (const QString &uid, colorChangers)
            emit notebookColorChanged(uid);

        if (!newDefaultNotebookUid.isEmpty())
            emit defaultNotebookChanged(newDefaultNotebookUid);
    }
}

NemoCalendarEventOccurrence* NemoCalendarManager::getNextOccurrence(const QString &uid, const KDateTime &recurrenceId,
                                                                    const QDateTime &start)
{
    NemoCalendarData::EventOccurrence eo;
    QMetaObject::invokeMethod(mCalendarWorker, "getNextOccurrence", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(NemoCalendarData::EventOccurrence, eo),
                              Q_ARG(QString, uid),
                              Q_ARG(KDateTime, recurrenceId),
                              Q_ARG(QDateTime, start));

    if (!eo.startTime.isValid()) {
        qWarning() << Q_FUNC_INFO << "Unable to find occurrence for event" << uid;
        return new NemoCalendarEventOccurrence(QString(), KDateTime(), QDateTime(), QDateTime());
    }

    return new NemoCalendarEventOccurrence(eo.eventUid, eo.recurrenceId, eo.startTime, eo.endTime);
}

QList<NemoCalendarData::Attendee> NemoCalendarManager::getEventAttendees(const QString &uid, const KDateTime &recurrenceId)
{
    QList<NemoCalendarData::Attendee> attendees;
    QMetaObject::invokeMethod(mCalendarWorker, "getEventAttendees", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<NemoCalendarData::Attendee>, attendees),
                              Q_ARG(QString, uid),
                              Q_ARG(KDateTime, recurrenceId));
    return attendees;
}

void NemoCalendarManager::dataLoadedSlot(QList<NemoCalendarData::Range> ranges,
                                           QStringList uidList,
                                           QMultiHash<QString, NemoCalendarData::Event> events,
                                           QHash<QString, NemoCalendarData::EventOccurrence> occurrences,
                                           QHash<QDate, QStringList> dailyOccurrences,
                                           bool reset)
{
    QList<NemoCalendarData::Event> oldEvents;
    foreach (const QString &uid, mEventObjects.keys()) {
        // just add all matching uid, change signal emission will match recurrence ids
        if (events.contains(uid))
            oldEvents.append(mEvents.values(uid));
    }

    if (reset) {
        mEvents.clear();
        mEventOccurrences.clear();
        mEventOccurrenceForDates.clear();
    }

    mLoadedRanges = addRanges(mLoadedRanges, ranges);
    mLoadedQueries.append(uidList);
    mEvents = mEvents.unite(events);
    mEventOccurrences = mEventOccurrences.unite(occurrences);
    mEventOccurrenceForDates = mEventOccurrenceForDates.unite(dailyOccurrences);
    mLoadPending = false;

    foreach (const NemoCalendarData::Event &oldEvent, oldEvents) {
        NemoCalendarData::Event event = getEvent(oldEvent.uniqueId, oldEvent.recurrenceId);
        if (event.isValid())
            sendEventChangeSignals(event, oldEvent);
    }

    emit dataUpdated();
    mTimer->start();
}

void NemoCalendarManager::sendEventChangeSignals(const NemoCalendarData::Event &newEvent,
                                                 const NemoCalendarData::Event &oldEvent)
{
    NemoCalendarEvent *eventObject = 0;
    QMultiHash<QString, NemoCalendarEvent *>::iterator it = mEventObjects.find(newEvent.uniqueId);
    while (it != mEventObjects.end() && it.key() == newEvent.uniqueId) {
        if (it.value()->recurrenceId() == newEvent.recurrenceId) {
            eventObject = it.value();
            break;
        }
        ++it;
    }

    if (!eventObject)
        return;

    if (newEvent.allDay != oldEvent.allDay)
        emit eventObject->allDayChanged();

    if (newEvent.displayLabel != oldEvent.displayLabel)
        emit eventObject->displayLabelChanged();

    if (newEvent.description != oldEvent.description)
        emit eventObject->descriptionChanged();

    if (newEvent.endTime != oldEvent.endTime)
        emit eventObject->endTimeChanged();

    if (newEvent.location != oldEvent.location)
        emit eventObject->locationChanged();

    if (newEvent.recur != oldEvent.recur)
        emit eventObject->recurChanged();

    if (newEvent.reminder != oldEvent.reminder)
        emit eventObject->reminderChanged();

    if (newEvent.startTime != oldEvent.startTime)
        emit eventObject->startTimeChanged();
}
