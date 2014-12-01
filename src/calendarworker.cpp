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

#include "calendarworker.h"

#include <QDebug>
#include <QSettings>

// mkcal
#include <event.h>
#include <notebook.h>

// kCalCore
#include <calformat.h>
#include <vcalformat.h>
#include <recurrence.h>
#include <recurrencerule.h>

#include <libical/vobject.h>
#include <libical/vcaltmp.h>

NemoCalendarWorker::NemoCalendarWorker() :
    QObject(0)
{
}

NemoCalendarWorker::~NemoCalendarWorker()
{
    if (mStorage.data())
        mStorage->close();

    mCalendar.clear();
    mStorage.clear();
}

void NemoCalendarWorker::storageModified(mKCal::ExtendedStorage *storage, const QString &info)
{
    Q_UNUSED(storage)
    Q_UNUSED(info)

    // 'info' is either a path to the database (in which case we're screwed, we
    // have no idea what changed, so tell all interested models to reload) or a
    // space-seperated list of event UIDs.
    //
    // unfortunately we don't know *what* about these events changed with the
    // current mkcal API, so we'll have to try our best to guess when the time
    // comes.
    mSentEvents.clear();
    loadNotebooks();
    emit storageModifiedSignal(info);
}

void NemoCalendarWorker::storageProgress(mKCal::ExtendedStorage *storage, const QString &info)
{
    Q_UNUSED(storage)
    Q_UNUSED(info)
}

void NemoCalendarWorker::storageFinished(mKCal::ExtendedStorage *storage, bool error, const QString &info)
{
    Q_UNUSED(storage)
    Q_UNUSED(error)
    Q_UNUSED(info)
}

void NemoCalendarWorker::deleteEvent(const QString &uid, const KDateTime &recurrenceId, const QDateTime &dateTime)
{
    KCalCore::Event::Ptr event = mCalendar->event(uid, recurrenceId);

    if (!event)
        return;

    if (event->recurs() && dateTime.isValid()) {
        event->recurrence()->addExDateTime(KDateTime(dateTime, KDateTime::Spec(KDateTime::LocalZone)));
    } else {
        mCalendar->deleteEvent(event);
    }
}

void NemoCalendarWorker::deleteAll(const QString &uid)
{
    KCalCore::Event::Ptr event = mCalendar->event(uid);
    if (!event) {
        qWarning() << "Failed to delete event, not found" << uid;
        return;
    }

    mCalendar->deleteEventInstances(event);
    mCalendar->deleteEvent(event);
}

// eventToVEvent() is protected
class NemoCalendarVCalFormat : public KCalCore::VCalFormat
{
public:
    QString convertEventToVCalendar(const KCalCore::Event::Ptr &event, const QString &prodId)
    {
        VObject *vCalObj = vcsCreateVCal(
                    QDateTime::currentDateTime().toString(Qt::ISODate).toLatin1().data(),
                    NULL,
                    prodId.toLatin1().data(),
                    NULL,
                    (char *) "1.0");
        VObject *vEventObj = eventToVEvent(event);
        addVObjectProp(vCalObj, vEventObj);
        char *memVObject = writeMemVObject(0, 0, vCalObj);
        QString retn = QLatin1String(memVObject);
        free(memVObject);
        cleanVObject(vCalObj);
        return retn;
    }
};

QString NemoCalendarWorker::convertEventToVCalendar(const QString &uid, const QString &prodId) const
{
    // NOTE: not fetching eventInstances() with different recurrenceId for VCalendar.
    KCalCore::Event::Ptr event = mCalendar->event(uid);
    if (event.isNull()) {
        qWarning() << "No event with uid " << uid << ", unable to create VCalendar";
        return "";
    }

    NemoCalendarVCalFormat fmt;
    return fmt.convertEventToVCalendar(event,
                                       prodId.isEmpty() ? QLatin1String("-//NemoMobile.org/Nemo//NONSGML v1.0//EN")
                                                        : prodId);
}

void NemoCalendarWorker::save()
{
    mStorage->save();
}

void NemoCalendarWorker::saveEvent(const NemoCalendarData::Event &eventData)
{
    QString notebookUid = eventData.calendarUid;

    if (!notebookUid.isEmpty() && !mStorage->isValidNotebook(notebookUid))
        return;

    KCalCore::Event::Ptr event;
    bool createNew = eventData.uniqueId.isEmpty();

    if (createNew) {
        event = KCalCore::Event::Ptr(new KCalCore::Event);
    } else {
        event = mCalendar->event(eventData.uniqueId, eventData.recurrenceId);

        if (!event) {
            // possibility that event was removed while changes were edited. options to either skip, as done now,
            // or resurrect the event
            qWarning("Event to be saved not found");
            return;
        }

        if (!notebookUid.isEmpty() && mCalendar->notebook(event) != notebookUid) {
            // mkcal does funny things when moving event between notebooks, work around by changing uid
            KCalCore::Event::Ptr newEvent = KCalCore::Event::Ptr(event->clone());
            newEvent->setUid(KCalCore::CalFormat::createUniqueId());
            emit eventNotebookChanged(event->uid(), newEvent->uid(), notebookUid);
            mCalendar->deleteEvent(event);
            mCalendar->addEvent(newEvent, notebookUid);
            event = newEvent;
        } else {
            event->setRevision(event->revision() + 1);
        }
    }

    setEventData(event, eventData);

    if (createNew) {
        if (notebookUid.isEmpty())
            mCalendar->addEvent(event);
        else
            mCalendar->addEvent(event, notebookUid);
    }

    save();
}

void NemoCalendarWorker::setEventData(KCalCore::Event::Ptr &event, const NemoCalendarData::Event &eventData)
{
    event->setDescription(eventData.description);
    event->setSummary(eventData.displayLabel);
    event->setDtStart(eventData.startTime);
    event->setDtEnd(eventData.endTime);
    // setDtStart() overwrites allDay status based on KDateTime::isDateOnly(), avoid by setting that later
    event->setAllDay(eventData.allDay);
    event->setLocation(eventData.location);
    setReminder(event, eventData.reminder);
    setRecurrence(event, eventData.recur);

    if (eventData.recur != NemoCalendarEvent::RecurOnce) {
        event->recurrence()->setEndDate(eventData.recurEndDate);
        if (!eventData.recurEndDate.isValid()) {
            // Recurrence/RecurrenceRule don't have separate method to clear the end date, and currently
            // setting invalid date doesn't make the duration() indicate recurring infinitely.
            event->recurrence()->setDuration(-1);
        }
    }
}

void NemoCalendarWorker::replaceOccurrence(const NemoCalendarData::Event &eventData, const QDateTime &startTime)
{
    QString notebookUid = eventData.calendarUid;
    if (!notebookUid.isEmpty() && !mStorage->isValidNotebook(notebookUid)) {
        qWarning("replaceOccurrence() - invalid notebook given");
        emit occurrenceExceptionFailed(eventData, startTime);
        return;
    }

    KCalCore::Event::Ptr event = mCalendar->event(eventData.uniqueId, eventData.recurrenceId);
    if (!event) {
        qWarning("Event to create occurrence replacement for not found");
        emit occurrenceExceptionFailed(eventData, startTime);
        return;
    }

    // Note: occurrence given in local time. Thus if timezone changes between fetching and changing, this won't work
    KDateTime occurrenceTime = KDateTime(startTime, KDateTime::LocalZone);

    KCalCore::Incidence::Ptr replacementIncidence = mCalendar->dissociateSingleOccurrence(event,
                                                                                          occurrenceTime,
                                                                                          KDateTime::LocalZone);
    KCalCore::Event::Ptr replacement = replacementIncidence.staticCast<KCalCore::Event>();
    if (!replacement) {
        qWarning("Didn't find event occurrence to replace");
        emit occurrenceExceptionFailed(eventData, startTime);
        return;
    }

    setEventData(replacement, eventData);

    mCalendar->addEvent(replacement, notebookUid);
    emit occurrenceExceptionCreated(eventData, startTime, replacement->recurrenceId());
    save();
}

void NemoCalendarWorker::init()
{
    mCalendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::LocalZone()));
    mStorage = mCalendar->defaultStorage(mCalendar);
    mStorage->open();
    mStorage->registerObserver(this);
    loadNotebooks();
}

NemoCalendarEvent::Recur NemoCalendarWorker::convertRecurrence(const KCalCore::Event::Ptr &event) const
{
    if (!event->recurs())
        return NemoCalendarEvent::RecurOnce;

    if (event->recurrence()->rRules().count() != 1)
        return NemoCalendarEvent::RecurCustom;

    ushort rt = event->recurrence()->recurrenceType();
    int freq = event->recurrence()->frequency();

    if (rt == KCalCore::Recurrence::rDaily && freq == 1) {
        return NemoCalendarEvent::RecurDaily;
    } else if (rt == KCalCore::Recurrence::rWeekly && freq == 1) {
        return NemoCalendarEvent::RecurWeekly;
    } else if (rt == KCalCore::Recurrence::rWeekly && freq == 2) {
        return NemoCalendarEvent::RecurBiweekly;
    } else if (rt == KCalCore::Recurrence::rMonthlyDay && freq == 1) {
        return NemoCalendarEvent::RecurMonthly;
    } else if (rt == KCalCore::Recurrence::rYearlyMonth && freq == 1) {
        return NemoCalendarEvent::RecurYearly;
    }

    return NemoCalendarEvent::RecurCustom;
}

bool NemoCalendarWorker::setRecurrence(KCalCore::Event::Ptr &event, NemoCalendarEvent::Recur recur)
{
    if (!event)
        return false;

    NemoCalendarEvent::Recur oldRecur = convertRecurrence(event);

    if (recur == NemoCalendarEvent::RecurCustom) {
        qWarning() << "Cannot assign RecurCustom, will assing RecurOnce";
        recur = NemoCalendarEvent::RecurOnce;
    }

    if (recur == NemoCalendarEvent::RecurOnce)
        event->recurrence()->clear();

    if (oldRecur != recur) {
        switch (recur) {
        case NemoCalendarEvent::RecurOnce:
            break;
        case NemoCalendarEvent::RecurDaily:
            event->recurrence()->setDaily(1);
            break;
        case NemoCalendarEvent::RecurWeekly:
            event->recurrence()->setWeekly(1);
            break;
        case NemoCalendarEvent::RecurBiweekly:
            event->recurrence()->setWeekly(2);
            break;
        case NemoCalendarEvent::RecurMonthly:
            event->recurrence()->setMonthly(1);
            break;
        case NemoCalendarEvent::RecurYearly:
            event->recurrence()->setYearly(1);
            break;
        case NemoCalendarEvent::RecurCustom:
            break;
        }
        return true;
    }

    return false;
}

KCalCore::Duration NemoCalendarWorker::reminderToDuration(NemoCalendarEvent::Reminder reminder) const
{
    KCalCore::Duration offset(0);
    switch (reminder) {
    default:
    case NemoCalendarEvent::ReminderNone:
    case NemoCalendarEvent::ReminderTime:
        break;
    case NemoCalendarEvent::Reminder5Min:
        offset = KCalCore::Duration(-5 * 60);
        break;
    case NemoCalendarEvent::Reminder15Min:
        offset = KCalCore::Duration(-15 * 60);
        break;
    case NemoCalendarEvent::Reminder30Min:
        offset = KCalCore::Duration(-30 * 60);
        break;
    case NemoCalendarEvent::Reminder1Hour:
        offset = KCalCore::Duration(-60 * 60);
        break;
    case NemoCalendarEvent::Reminder2Hour:
        offset = KCalCore::Duration(-2 * 60 * 60);
        break;
    case NemoCalendarEvent::Reminder1Day:
        offset = KCalCore::Duration(-24 * 60 * 60);
        break;
    case NemoCalendarEvent::Reminder2Day:
        offset = KCalCore::Duration(-2 * 24 * 60 * 60);
        break;
    }
    return offset;
}

NemoCalendarEvent::Reminder NemoCalendarWorker::getReminder(const KCalCore::Event::Ptr &event) const
{
    KCalCore::Alarm::List alarms = event->alarms();

    KCalCore::Alarm::Ptr alarm;

    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure)
            continue;

        if (alarm)
            return NemoCalendarEvent::ReminderNone;
        else
            alarm = alarms.at(ii);
    }

    if (!alarm)
        return NemoCalendarEvent::ReminderNone;

    KCalCore::Duration d = alarm->startOffset();
    int sec = d.asSeconds();

    switch (sec) {
    case 0:
        return NemoCalendarEvent::ReminderTime;
    case -5 * 60:
        return NemoCalendarEvent::Reminder5Min;
    case -15 * 60:
        return NemoCalendarEvent::Reminder15Min;
    case -30 * 60:
        return NemoCalendarEvent::Reminder30Min;
    case -60 * 60:
        return NemoCalendarEvent::Reminder1Hour;
    case -2 * 60 * 60:
        return NemoCalendarEvent::Reminder2Hour;
    case -24 * 60 * 60:
        return NemoCalendarEvent::Reminder1Day;
    case -2 * 24 * 60 * 60:
        return NemoCalendarEvent::Reminder2Day;
    default:
        return NemoCalendarEvent::ReminderNone;
    }
}

bool NemoCalendarWorker::setReminder(KCalCore::Event::Ptr &event, NemoCalendarEvent::Reminder reminder)
{
    if (!event)
        return false;

    if (getReminder(event) == reminder)
        return false;

    KCalCore::Alarm::List alarms = event->alarms();
    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure)
            continue;
        event->removeAlarm(alarms.at(ii));
    }

    if (reminder != NemoCalendarEvent::ReminderNone) {
        KCalCore::Alarm::Ptr alarm = event->newAlarm();
        alarm->setEnabled(true);
        alarm->setStartOffset(reminderToDuration(reminder));
    }

    return true;
}

QList<NemoCalendarData::Notebook> NemoCalendarWorker::notebooks() const
{
    return mNotebooks.values();
}

void NemoCalendarWorker::excludeNotebook(const QString &notebookUid, bool exclude)
{
    if (saveExcludeNotebook(notebookUid, exclude)) {
        emit excludedNotebooksChanged(excludedNotebooks());
        emit notebooksChanged(mNotebooks.values());
    }
}

void NemoCalendarWorker::setDefaultNotebook(const QString &notebookUid)
{
    if (mStorage->defaultNotebook() && mStorage->defaultNotebook()->uid() == notebookUid)
        return;

    mStorage->setDefaultNotebook(mStorage->notebook(notebookUid));
    mStorage->save();
}

QStringList NemoCalendarWorker::excludedNotebooks() const
{
    QStringList excluded;
    foreach (const NemoCalendarData::Notebook &notebook, mNotebooks.values()) {
        if (notebook.excluded)
            excluded << notebook.uid;
    }
    return excluded;
}

bool NemoCalendarWorker::saveExcludeNotebook(const QString &notebookUid, bool exclude)
{
    if (!mNotebooks.contains(notebookUid))
        return false;

    if (mNotebooks.value(notebookUid).excluded == exclude)
        return false;

    NemoCalendarData::Notebook notebook = mNotebooks.value(notebookUid);
    QSettings settings("nemo", "nemo-qml-plugin-calendar");
    notebook.excluded = exclude;
    if (exclude)
        settings.setValue("exclude/" + notebook.uid, true);
    else
        settings.remove("exclude/" + notebook.uid);

    mNotebooks.insert(notebook.uid, notebook);
    return true;
}

void NemoCalendarWorker::setExcludedNotebooks(const QStringList &list)
{
    bool changed = false;

    QStringList excluded = excludedNotebooks();

    foreach (const QString &notebookUid, list) {
        if (!excluded.contains(notebookUid)) {
            if (saveExcludeNotebook(notebookUid, true))
                changed = true;
        }
    }

    foreach (const QString &notebookUid, excluded) {
        if (!list.contains(notebookUid)) {
            if (saveExcludeNotebook(notebookUid, false))
                changed = true;
        }
    }

    if (changed) {
        emit excludedNotebooksChanged(excludedNotebooks());
        emit notebooksChanged(mNotebooks.values());
    }
}

void NemoCalendarWorker::setNotebookColor(const QString &notebookUid, const QString &color)
{
    if (!mNotebooks.contains(notebookUid))
        return;

    if (mNotebooks.value(notebookUid).color != color) {
        NemoCalendarData::Notebook notebook = mNotebooks.value(notebookUid);
        notebook.color = color;
        mNotebooks.insert(notebook.uid, notebook);

        QSettings settings("nemo", "nemo-qml-plugin-calendar");
        settings.setValue("colors/" + notebook.uid, notebook.color);

        emit notebooksChanged(mNotebooks.values());
    }
}

QHash<QString, NemoCalendarData::EventOccurrence>
NemoCalendarWorker::eventOccurrences(const QList<NemoCalendarData::Range> &ranges) const
{
    mKCal::ExtendedCalendar::ExpandedIncidenceList events;
    foreach (NemoCalendarData::Range range, ranges) {
        // mkcal fails to consider all day event end time inclusivity on this, add -1 days to start date
        mKCal::ExtendedCalendar::ExpandedIncidenceList newEvents =
                mCalendar->rawExpandedEvents(range.first.addDays(-1), range.second,
                                             false, false, KDateTime::Spec(KDateTime::LocalZone));
        events = events << newEvents;
    }

    QStringList excluded = excludedNotebooks();
    QHash<QString, NemoCalendarData::EventOccurrence> filtered;

    for (int kk = 0; kk < events.count(); ++kk) {
        // Filter out excluded notebooks
        if (excluded.contains(mCalendar->notebook(events.at(kk).second)))
            continue;

        mKCal::ExtendedCalendar::ExpandedIncidence exp = events.at(kk);
        NemoCalendarData::EventOccurrence occurrence;
        occurrence.eventUid = exp.second->uid();
        occurrence.recurrenceId = exp.second->recurrenceId();
        occurrence.startTime = exp.first.dtStart;
        occurrence.endTime = exp.first.dtEnd;
        filtered.insert(occurrence.getId(), occurrence);
    }

    return filtered;
}

QHash<QDate, QStringList>
NemoCalendarWorker::dailyEventOccurrences(const QList<NemoCalendarData::Range> &ranges,
                                          const QMultiHash<QString, KDateTime> &allDay,
                                          const QList<NemoCalendarData::EventOccurrence> &occurrences)
{
    QHash<QDate, QStringList> occurrenceHash;
    foreach (const NemoCalendarData::Range &range, ranges) {
        QDate start = range.first;
        while (start <= range.second) {
            foreach (const NemoCalendarData::EventOccurrence &eo, occurrences) {
                // On all day events the end time is inclusive, otherwise not
                if ((eo.startTime.date() < start
                     && (eo.endTime.date() > start
                         || (eo.endTime.date() == start && (allDay.contains(eo.eventUid, eo.recurrenceId)
                                                            || eo.endTime.time() > QTime(0, 0)))))
                        || (eo.startTime.date() >= start && eo.startTime.date() <= start)) {
                    occurrenceHash[start].append(eo.getId());
                }
            }
            start = start.addDays(1);
        }
    }
    return occurrenceHash;
}

void NemoCalendarWorker::loadData(const QList<NemoCalendarData::Range> &ranges,
                                  const QStringList &uidList,
                                  bool reset)
{
    foreach (const NemoCalendarData::Range &range, ranges)
        mStorage->load(range.first, range.second);

    // Note: omitting recurrence ids since loadRecurringIncidences() loads them anyway
    foreach (const QString &uid, uidList)
        mStorage->load(uid);

    // Load all recurring incidences, we have no other way to detect if they occur within a range
    mStorage->loadRecurringIncidences();

    KCalCore::Event::List list = mCalendar->rawEvents();

    if (reset)
        mSentEvents.clear();

    QMultiHash<QString, NemoCalendarData::Event> events;
    QMultiHash<QString, KDateTime> allDay;

    foreach (const KCalCore::Event::Ptr e, list) {
        // The database may have changed after loading the events, make sure that the notebook
        // of the event still exists.
        if (mStorage->notebook(mCalendar->notebook(e)).isNull())
            continue;

        NemoCalendarData::Event event = createEventStruct(e);

        if (!mSentEvents.contains(event.uniqueId, event.recurrenceId)) {
            mSentEvents.insert(event.uniqueId, event.recurrenceId);
            events.insert(event.uniqueId, event);
            if (event.allDay)
                allDay.insert(event.uniqueId, event.recurrenceId);
        }
    }

    QHash<QString, NemoCalendarData::EventOccurrence> occurrences = eventOccurrences(ranges);
    QHash<QDate, QStringList> dailyOccurrences = dailyEventOccurrences(ranges, allDay, occurrences.values());

    emit dataLoaded(ranges, uidList, events, occurrences, dailyOccurrences, reset);
}

NemoCalendarData::Event NemoCalendarWorker::createEventStruct(const KCalCore::Event::Ptr &e) const
{
    NemoCalendarData::Event event;
    event.uniqueId = e->uid();
    event.recurrenceId = e->recurrenceId();
    event.allDay = e->allDay();
    event.calendarUid = mCalendar->notebook(e);
    event.description = e->description();
    event.displayLabel = e->summary();
    event.endTime = e->dtEnd();
    event.location = e->location();
    event.readonly = mStorage->notebook(event.calendarUid)->isReadOnly();
    event.recur = convertRecurrence(e);
    KCalCore::RecurrenceRule *defaultRule = e->recurrence()->defaultRRule();
    if (defaultRule) {
        event.recurEndDate = defaultRule->endDt().date();
    }
    event.reminder = getReminder(e);
    event.startTime = e->dtStart();
    return event;
}

void NemoCalendarWorker::loadNotebooks()
{
    QStringList defaultNotebookColors = QStringList() << "#00aeef" << "red" << "blue" << "green" << "pink" << "yellow";
    int nextDefaultNotebookColor = 0;

    mKCal::Notebook::List notebooks = mStorage->notebooks();
    QSettings settings("nemo", "nemo-qml-plugin-calendar");

    QHash<QString, NemoCalendarData::Notebook> newNotebooks;

    bool changed = mNotebooks.isEmpty();
    for (int ii = 0; ii < notebooks.count(); ++ii) {
        NemoCalendarData::Notebook notebook = mNotebooks.value(notebooks.at(ii)->uid(), NemoCalendarData::Notebook());
        notebook.name = notebooks.at(ii)->name();
        notebook.uid = notebooks.at(ii)->uid();
        notebook.description = notebooks.at(ii)->description();
        notebook.isDefault = notebooks.at(ii)->isDefault();
        notebook.readOnly = notebooks.at(ii)->isReadOnly();
        notebook.localCalendar = notebooks.at(ii)->isMaster()
                && !notebooks.at(ii)->isShared()
                && notebooks.at(ii)->pluginName().isEmpty();

        notebook.excluded = settings.value("exclude/" + notebook.uid, false).toBool();

        notebook.color = settings.value("colors/" + notebook.uid, QString()).toString();
        if (notebook.color.isEmpty())
            notebook.color = notebooks.at(ii)->color();
        if (notebook.color.isEmpty())
            notebook.color = defaultNotebookColors.at((nextDefaultNotebookColor++) % defaultNotebookColors.count());

        if (mNotebooks.contains(notebook.uid) && mNotebooks.value(notebook.uid) != notebook)
            changed = true;

        newNotebooks.insert(notebook.uid, notebook);
    }

    if (changed || mNotebooks.count() != newNotebooks.count()) {
        mNotebooks = newNotebooks;
        emit excludedNotebooksChanged(excludedNotebooks());
        emit notebooksChanged(mNotebooks.values());
    }
}


NemoCalendarData::EventOccurrence NemoCalendarWorker::getNextOccurrence(const QString &uid,
                                                                        const KDateTime &recurrenceId,
                                                                        const QDateTime &start) const
{
    KCalCore::Event::Ptr event = mCalendar->event(uid, recurrenceId);
    NemoCalendarData::EventOccurrence occurrence;

    if (event) {
        mKCal::ExtendedCalendar::ExpandedIncidenceValidity eiv = {
            event->dtStart().toLocalZone().dateTime(),
            event->dtEnd().toLocalZone().dateTime()
        };

        if (!start.isNull() && event->recurs()) {
            KDateTime startTime = KDateTime(start, KDateTime::Spec(KDateTime::LocalZone));
            KCalCore::Recurrence *recurrence = event->recurrence();
            if (recurrence->recursAt(startTime)) {
                eiv.dtStart = startTime.toLocalZone().dateTime();
                eiv.dtEnd = KCalCore::Duration(event->dtStart(), event->dtEnd()).end(startTime).toLocalZone().dateTime();
            } else {
                KDateTime match = recurrence->getNextDateTime(startTime);
                if (match.isNull())
                    match = recurrence->getPreviousDateTime(startTime);

                if (!match.isNull()) {
                    eiv.dtStart = match.toLocalZone().dateTime();
                    eiv.dtEnd = KCalCore::Duration(event->dtStart(), event->dtEnd()).end(match).toLocalZone().dateTime();
                }
            }
        }

        occurrence.eventUid = event->uid();
        occurrence.recurrenceId = event->recurrenceId();
        occurrence.startTime = eiv.dtStart;
        occurrence.endTime = eiv.dtEnd;
    }

    return occurrence;
}

QList<NemoCalendarData::Attendee> NemoCalendarWorker::getEventAttendees(const QString &uid, const KDateTime &recurrenceId)
{
    QList<NemoCalendarData::Attendee> result;

    KCalCore::Event::Ptr event = mCalendar->event(uid, recurrenceId);

    if (event.isNull()) {
        return result;
    }

    KCalCore::Person::Ptr calOrganizer = event->organizer();

    NemoCalendarData::Attendee organizer;

    if (!calOrganizer.isNull() && !calOrganizer->isEmpty()) {
        organizer.isOrganizer = true;
        organizer.name = calOrganizer->name();
        organizer.email = calOrganizer->email();
        organizer.participationRole = KCalCore::Attendee::ReqParticipant;
        result.append(organizer);
    }

    KCalCore::Attendee::List attendees = event->attendees();
    NemoCalendarData::Attendee attendee;
    attendee.isOrganizer = false;

    foreach (KCalCore::Attendee::Ptr calAttendee, attendees) {
        attendee.name = calAttendee->name();
        attendee.email = calAttendee->email();
        if (attendee.name == organizer.name && attendee.email == organizer.email) {
            // avoid duplicate info
            continue;
        }
        attendee.participationRole = calAttendee->role();
        result.append(attendee);
    }

    return result;
}
