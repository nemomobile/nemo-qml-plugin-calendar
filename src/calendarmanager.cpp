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

NemoCalendarManager::NemoCalendarManager() :
    QObject(0), mLoadPending(false), mResetPending(false)
{
    qRegisterMetaType<KDateTime>("KDateTime");
    qRegisterMetaType<QList<KDateTime> >("QList<KDateTime>");
    qRegisterMetaType<NemoCalendarEvent::Recur>("NemoCalendarEvent::Recur");
    qRegisterMetaType<NemoCalendarEvent::Reminder>("NemoCalendarEvent::Reminder");
    qRegisterMetaType<QHash<QString,NemoCalendarData::EventOccurrence> >("QHash<QString,NemoCalendarData::EventOccurrence>");
    qRegisterMetaType<QHash<QString,NemoCalendarData::Event> >("QHash<QString,NemoCalendarData::Event>");
    qRegisterMetaType<QHash<QDate,QStringList> >("QHash<QDate,QStringList>");
    qRegisterMetaType<NemoCalendarData::Range>("NemoCalendarData::Range");
    qRegisterMetaType<QList<NemoCalendarData::Range > >("QList<NemoCalendarData::Range>");
    qRegisterMetaType<QList<NemoCalendarData::Notebook> >("QList<NemoCalendarData::Notebook>");

    mCalendarWorker = new NemoCalendarWorker();
    mCalendarWorker->moveToThread(&mWorkerThread);

    connect(&mWorkerThread, SIGNAL(finished()), mCalendarWorker, SLOT(deleteLater()));

    connect(mCalendarWorker, SIGNAL(storageModifiedSignal(QString)),
            this, SLOT(storageModifiedSlot(QString)));
    connect(mCalendarWorker, SIGNAL(displayLabelChanged(QString,QString)),
            this, SLOT(displayLabelChangedSlot(QString,QString)));
    connect(mCalendarWorker, SIGNAL(descriptionChanged(QString,QString)),
            this, SLOT(descriptionChangedSlot(QString,QString)));
    connect(mCalendarWorker, SIGNAL(locationChanged(QString,QString)),
            this, SLOT(locationChangedSlot(QString,QString)));
    connect(mCalendarWorker, SIGNAL(startTimeChanged(QString,KDateTime)),
            this, SLOT(startTimeChangedSlot(QString,KDateTime)));
    connect(mCalendarWorker, SIGNAL(endTimeChanged(QString,KDateTime)),
            this, SLOT(endTimeChangedSlot(QString,KDateTime)));
    connect(mCalendarWorker, SIGNAL(allDayChanged(QString,bool)),
            this, SLOT(allDayChangedSlot(QString,bool)));
    connect(mCalendarWorker, SIGNAL(alarmProgramChanged(QString,QString)),
            this, SLOT(alarmProgramChangedSlot(QString,QString)));
    connect(mCalendarWorker, SIGNAL(recurrenceChanged(QString,NemoCalendarEvent::Recur)),
            this, SLOT(recurrenceChangedSlot(QString,NemoCalendarEvent::Recur)));
    connect(mCalendarWorker, SIGNAL(reminderChanged(QString,NemoCalendarEvent::Reminder)),
            this, SLOT(reminderChangedSlot(QString,NemoCalendarEvent::Reminder)));
    connect(mCalendarWorker, SIGNAL(exceptionsChanged(QString,QList<KDateTime>)),
            this, SLOT(exceptionsChangedSlot(QString,QList<KDateTime>)));

    connect(mCalendarWorker, SIGNAL(excludedNotebooksChanged(QStringList)),
            this, SLOT(excludedNotebooksChangedSlot(QStringList)));
    connect(mCalendarWorker, SIGNAL(notebooksChanged(QList<NemoCalendarData::Notebook>)),
            this, SLOT(notebooksChangedSlot(QList<NemoCalendarData::Notebook>)));

    connect(mCalendarWorker, SIGNAL(rangesLoaded(QList<NemoCalendarData::Range>,QHash<QString,NemoCalendarData::Event>,QHash<QString,NemoCalendarData::EventOccurrence>,QHash<QDate,QStringList>,bool)),
            this, SLOT(rangesLoadedSlot(QList<NemoCalendarData::Range>,QHash<QString,NemoCalendarData::Event>,QHash<QString,NemoCalendarData::EventOccurrence>,QHash<QDate,QStringList>,bool)));


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

void NemoCalendarManager::setDefaultNotebook(const QString &notebookUid)
{
    QMetaObject::invokeMethod(mCalendarWorker, "setDefaultNotebook", Qt::QueuedConnection,
                              Q_ARG(QString, notebookUid));
}

NemoCalendarEvent* NemoCalendarManager::eventObject(const QString &eventUid)
{
    if (mEvents.contains(eventUid)) {
        if (!mEventObjects.contains(eventUid))
            mEventObjects.insert(eventUid, new NemoCalendarEvent(this, eventUid));

        return mEventObjects.value(eventUid);
    }

    // TODO: maybe attempt to read event from DB? This situation should not happen.
    qWarning() << Q_FUNC_INFO << "No event with uid" << eventUid << ", returning empty event";

    return new NemoCalendarEvent(this, "");
}

NemoCalendarEvent* NemoCalendarManager::createEvent()
{
    QString uid;
    QMetaObject::invokeMethod(mCalendarWorker, "createEvent", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QString, uid));

    mEventObjects.insert(uid, new NemoCalendarEvent(this, uid, true));

    NemoCalendarData::Event event;
    event.uniqueId = uid;
    event.recur = NemoCalendarEvent::RecurOnce;
    event.reminder = NemoCalendarEvent::ReminderNone;
    event.allDay = false;
    event.readonly = false;
    mEvents.insert(uid, event);

    return mEventObjects.value(uid);
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
    return mNotebooks.value(notebookUid, NemoCalendarData::Notebook()).color;
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
        foreach (const QString &uid, mEventOccurrenceForDates.value(model->startDate())) {
            if (mEventOccurrences.contains(uid)) {
                NemoCalendarData::EventOccurrence eo = mEventOccurrences.value(uid);
                filtered.append(new NemoCalendarEventOccurrence(eo.eventUid, eo.startTime, eo.endTime));
            } else {
                qWarning() << "no occurrence with uid" << uid;
            }
        }
    } else {
        foreach (const NemoCalendarData::EventOccurrence &eo, mEventOccurrences.values()) {
            NemoCalendarEvent *event = eventObject(eo.eventUid);
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
                filtered.append(new NemoCalendarEventOccurrence(eo.eventUid, eo.startTime, eo.endTime));
            }
        }
    }

    model->doRefresh(filtered);
}

void NemoCalendarManager::doAgendaRefresh()
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
    }

    if (!missingRanges.isEmpty()) {
        mLoadPending = true;
        QMetaObject::invokeMethod(mCalendarWorker, "loadRanges", Qt::QueuedConnection,
                                  Q_ARG(QList<NemoCalendarData::Range>, missingRanges),
                                  Q_ARG(bool, mResetPending));
        mResetPending = false;
    }
}

void NemoCalendarManager::timeout() {
    if (mLoadPending)
        return;

    if (!mAgendaRefreshList.isEmpty() || mResetPending)
        doAgendaRefresh();
}

void NemoCalendarManager::saveEvent(const QString &uid, const QString &calendarUid)
{
    QMetaObject::invokeMethod(mCalendarWorker, "saveEvent", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(QString, calendarUid));
}

void NemoCalendarManager::deleteEvent(const QString &uid, const QDateTime &dateTime)
{
    QMetaObject::invokeMethod(mCalendarWorker, "deleteEvent", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(QDateTime, dateTime));
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

NemoCalendarData::Event NemoCalendarManager::getEvent(const QString &uid)
{
    return mEvents.value(uid, NemoCalendarData::Event());
}

void NemoCalendarManager::setLocation(const QString &uid, const QString &location)
{
    if (!mEvents.contains(uid) || mEvents.value(uid).location == location)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setLocation", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(QString, location));
}

void NemoCalendarManager::setDisplayLabel(const QString &uid, const QString &displayLabel)
{
    if (!mEvents.contains(uid) || mEvents.value(uid).displayLabel == displayLabel)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setDisplayLabel", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(QString, displayLabel));
}

void NemoCalendarManager::setDescription(const QString &uid, const QString &description)
{
    if (!mEvents.contains(uid) || mEvents.value(uid).description == description)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setDescription", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(QString, description));
}

void NemoCalendarManager::setStartTime(const QString &uid, const KDateTime &dateTime)
{
    if (!mEvents.contains(uid) || mEvents.value(uid).startTime == dateTime)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setStartTime", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(KDateTime, dateTime));
}

void NemoCalendarManager::setEndTime(const QString &uid, const KDateTime &dateTime)
{
    if (!mEvents.contains(uid) || mEvents.value(uid).endTime == dateTime)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setEndTime", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(KDateTime, dateTime));
}

void NemoCalendarManager::setAllDay(const QString &uid, bool allDay)
{
    if (!mEvents.contains(uid) || mEvents.value(uid).allDay == allDay)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setAllDay", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(bool, allDay));
}

void NemoCalendarManager::setAlarmProgram(const QString &uid, const QString &program)
{
    if (!mEvents.contains(uid) || mEvents.value(uid).alarmProgram == program)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setAlarmProgram", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(QString, program));
}

void NemoCalendarManager::setRecurrence(const QString &uid, NemoCalendarEvent::Recur recur)
{
    if (!mEvents.contains(uid) || mEvents.value(uid).recur == recur)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setRecurrence", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(NemoCalendarEvent::Recur, recur));
}

void NemoCalendarManager::setReminder(const QString &uid, NemoCalendarEvent::Reminder reminder)
{
    if (!mEvents.contains(uid) || mEvents.value(uid).reminder == reminder)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setReminder", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(NemoCalendarEvent::Reminder, reminder));
}

void NemoCalendarManager::setExceptions(const QString &uid, QList<KDateTime> exceptions)
{
    if (!mEvents.contains(uid))
        return;

    bool changed = true;
    QList<KDateTime> oldExceptions =  mEvents.value(uid).recurExceptionDates;
    if (exceptions.count() == oldExceptions.count()) {
        changed = false;
        for (int i = 0; i < exceptions.size(); i++) {
            if (!oldExceptions.contains(exceptions.at(i))) {
                changed = true;
                break;
            }
        }
    }

    if (!changed)
        return;

    QMetaObject::invokeMethod(mCalendarWorker, "setExceptions", Qt::QueuedConnection,
                              Q_ARG(QString, uid),
                              Q_ARG(QList<KDateTime>, exceptions));
}

void NemoCalendarManager::storageModifiedSlot(QString info)
{
    Q_UNUSED(info)
    mResetPending = true;
    emit storageModified();
}

void NemoCalendarManager::locationChangedSlot(QString uid, QString location)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].location = location;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->locationChanged();
}

void NemoCalendarManager::displayLabelChangedSlot(QString uid, QString displayLabel)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].displayLabel = displayLabel;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->displayLabelChanged();
}

void NemoCalendarManager::descriptionChangedSlot(QString uid, QString description)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].description = description;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->descriptionChanged();
}

void NemoCalendarManager::startTimeChangedSlot(QString uid, KDateTime dateTime)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].startTime = dateTime;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->startTimeChanged();
}

void NemoCalendarManager::endTimeChangedSlot(QString uid, KDateTime dateTime)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].endTime = dateTime;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->endTimeChanged();
}

void NemoCalendarManager::allDayChangedSlot(QString uid, bool allDay)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].allDay = allDay;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->allDayChanged();
}

void NemoCalendarManager::alarmProgramChangedSlot(QString uid, QString program)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].alarmProgram = program;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->alarmProgramChanged();
}

void NemoCalendarManager::recurrenceChangedSlot(QString uid, NemoCalendarEvent::Recur recur)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].recur = recur;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->recurChanged();
}

void NemoCalendarManager::reminderChangedSlot(QString uid, NemoCalendarEvent::Reminder reminder)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].reminder = reminder;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->reminderChanged();
}

void NemoCalendarManager::exceptionsChangedSlot(QString uid, QList<KDateTime> exceptions)
{
    if (!mEvents.contains(uid))
        return;

    mEvents[uid].recurExceptionDates = exceptions;
    if (mEventObjects.contains(uid))
        emit mEventObjects.value(uid)->recurExceptionsChanged();
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
    foreach (const NemoCalendarData::Notebook &notebook, notebooks) {
        bool notify = false;
        if (mNotebooks.contains(notebook.uid)) {
            if (mNotebooks.value(notebook.uid).color != notebook.color)
                notify = true;
        }
        mNotebooks.insert(notebook.uid, notebook);
        if (notify)
            emit notebookColorChanged(notebook.uid);
    }

    emit notebooksChanged(mNotebooks.values());
}

NemoCalendarEventOccurrence* NemoCalendarManager::getNextOccurence(const QString &uid, const QDateTime &start)
{
    NemoCalendarData::EventOccurrence eo;
    QMetaObject::invokeMethod(mCalendarWorker, "getNextOccurence", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(NemoCalendarData::EventOccurrence, eo),
                              Q_ARG(QString, uid),
                              Q_ARG(QDateTime, start));

    if (!eo.startTime.isValid()) {
        qWarning() << Q_FUNC_INFO << "Unable to find occurrence for event" << uid;
        return new NemoCalendarEventOccurrence(QString(), QDateTime(), QDateTime());
    }

    return new NemoCalendarEventOccurrence(eo.eventUid, eo.startTime, eo.endTime);
}

void NemoCalendarManager::rangesLoadedSlot(QList<NemoCalendarData::Range> ranges,
                                           QHash<QString, NemoCalendarData::Event> events,
                                           QHash<QString, NemoCalendarData::EventOccurrence> occurrences,
                                           QHash<QDate, QStringList> dailyOccurences,
                                           bool reset)
{
    QList<NemoCalendarData::Event> oldEvents;
    foreach (const QString &uid, mEventObjects.keys()) {
        if (events.contains(uid))
            oldEvents.append(mEvents.value(uid));
    }

    if (reset) {
        mEvents.clear();
        mEventOccurrences.clear();
        mEventOccurrenceForDates.clear();
    }

    mLoadedRanges = addRanges(mLoadedRanges, ranges);
    mEvents = mEvents.unite(events);
    mEventOccurrences = mEventOccurrences.unite(occurrences);
    mEventOccurrenceForDates = mEventOccurrenceForDates.unite(dailyOccurences);
    mLoadPending = false;

    foreach (const NemoCalendarData::Event &oldEvent, oldEvents)
        sendEventChangeSignals(mEvents.value(oldEvent.uniqueId), oldEvent);

    emit dataUpdated();
    mTimer->start();
}

void NemoCalendarManager::sendEventChangeSignals(const NemoCalendarData::Event &newEvent, const NemoCalendarData::Event &oldEvent)
{
    NemoCalendarEvent *eventObject = mEventObjects.value(newEvent.uniqueId);
    if (!eventObject)
        return;

    if (newEvent.alarmProgram != oldEvent.alarmProgram)
        emit eventObject->alarmProgramChanged();

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

    if (newEvent.recurExceptionDates != oldEvent.recurExceptionDates)
        emit eventObject->recurExceptionsChanged();

    if (newEvent.reminder != oldEvent.reminder)
        emit eventObject->reminderChanged();

    if (newEvent.startTime != oldEvent.startTime)
        emit eventObject->startTimeChanged();
}
