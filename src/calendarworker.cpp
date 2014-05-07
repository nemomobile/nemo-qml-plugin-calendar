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

void NemoCalendarWorker::deleteEvent(const QString &uid, const QDateTime &dateTime)
{
    KCalCore::Event::Ptr event = mCalendar->event(uid);

    if (!event)
        return;

    if (event->recurs() && dateTime.isValid())
        event->recurrence()->addExDateTime(KDateTime(dateTime, KDateTime::Spec(KDateTime::LocalZone)));
    else
        mCalendar->deleteEvent(event);
}

// eventToVEvent() is protected
class NemoCalendarVCalFormat : public KCalCore::VCalFormat
{
public:
    QString convertEventToVEvent(const KCalCore::Event::Ptr &event, const QString &prodId)
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
        free(vEventObj);
        free(vCalObj);
        return retn;
    }
};

QString NemoCalendarWorker::convertEventToVCalendar(const QString &uid, const QString &prodId) const
{
    KCalCore::Event::Ptr event = mCalendar->event(uid);
    if (event.isNull()) {
        qWarning() << "No event with uid " << uid << ", unable to create vEvent";
        return "";
    }

    NemoCalendarVCalFormat fmt;
    return fmt.convertEventToVEvent(event,
                                    prodId.isEmpty() ?
                                        QLatin1String("-//NemoMobile.org/Nemo//NONSGML v1.0//EN") :
                                        prodId);
}

void NemoCalendarWorker::save()
{
    mStorage->save();
}

void NemoCalendarWorker::saveEvent(const NemoCalendarData::Event &eventData, const QString &notebookUid)
{
    if (!notebookUid.isEmpty() && !mStorage->isValidNotebook(notebookUid))
        return;

    KCalCore::Event::Ptr event = mCalendar->event(eventData.uniqueId);
    bool changed = false;
    if (!event) {
        event = KCalCore::Event::Ptr(new KCalCore::Event);
        event->setUid(eventData.uniqueId);
        // Set event start & end dates as default ctor KDateTime gives false positives for ==
        event->setDtEnd(eventData.endTime);
        event->setDtStart(eventData.startTime);
        if (notebookUid.isEmpty())
            mCalendar->addEvent(event);
        else
            mCalendar->addEvent(event, notebookUid);

        changed = true;
    } else if (!notebookUid.isEmpty() && mCalendar->notebook(event) != notebookUid) {
        // mkcal does funny things when moving event between notebooks, work around by changing uid
        KCalCore::Event::Ptr newEvent = KCalCore::Event::Ptr(event->clone());
        newEvent->setUid(KCalCore::CalFormat::createUniqueId());
        emit eventNotebookChanged(event->uid(), newEvent->uid(), notebookUid);
        mCalendar->deleteEvent(event);
        mCalendar->addEvent(newEvent, notebookUid);
        event = newEvent;
        changed = true;
    } else {
        event->setRevision(event->revision() + 1);
    }

    changed = setAlarmProgram(event, eventData.alarmProgram) ? true : changed;

    if (event->allDay() != eventData.allDay) {
        event->setAllDay(eventData.allDay);
        changed = true;
    }
    if (event->description() != eventData.description) {
        event->setDescription(eventData.description);
        changed = true;
    }
    if (event->summary() != eventData.displayLabel) {
        event->setSummary(eventData.displayLabel);
        changed = true;
    }
    if (event->dtEnd() != eventData.endTime) {
        event->setDtEnd(eventData.endTime);
        changed = true;
    }
    if (event->location() != eventData.location) {
        event->setLocation(eventData.location);
        changed = true;
    }
    if (event->isReadOnly() != eventData.readonly) {
        event->setReadOnly(eventData.readonly);
        changed = true;
    }
    changed = setRecurrence(event, eventData.recur) ? true : changed;
    changed = setExceptions(event, eventData.recurExceptionDates) ? true : changed;
    changed = setReminder(event, eventData.reminder) ? true : changed;
    if (event->dtStart() != eventData.startTime) {
        event->setDtStart(eventData.startTime);
        changed = true;
    }

    if (changed)
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

bool NemoCalendarWorker::setAlarmProgram(KCalCore::Event::Ptr &event, const QString &program)
{
    KCalCore::Alarm::List alarms = event->alarms();

    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure
                && alarms.at(ii)->programArguments() == event->uid()) {
            if (alarms.at(ii)->programFile() != program) {
                alarms[ii]->setProgramFile(program);
                return true;
            }
            return false;
        }
    }

    KCalCore::Alarm::Ptr alarm = event->newAlarm();
    alarm->setEnabled(true);
    alarm->setType(KCalCore::Alarm::Procedure);
    alarm->setProcedureAlarm(program, event->uid());
    return true;
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

bool NemoCalendarWorker::setExceptions(KCalCore::Event::Ptr &event, const QList<KDateTime> &exceptions)
{
    if (!event || !event->recurs())
        return false;

    KCalCore::DateTimeList newList;
    newList.append(exceptions);
    newList.sortUnique();
    KCalCore::DateTimeList oldList = event->recurrence()->exDateTimes();
    if (newList == oldList)
        return false;

    event->recurrence()->setExDateTimes(newList);
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
    mStorage->setDefaultNotebook(mStorage->notebook(notebookUid));
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

QHash<QString, NemoCalendarData::EventOccurrence> NemoCalendarWorker::eventOccurences(const QList<NemoCalendarData::Range> &ranges) const
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
        occurrence.startTime = exp.first.dtStart;
        occurrence.endTime = exp.first.dtEnd;
        filtered.insert(occurrence.getId(), occurrence);
    }

    return filtered;
}

QHash<QDate, QStringList> NemoCalendarWorker::dailyEventOccurrences(const QList<NemoCalendarData::Range> &ranges,
                                                                   const QStringList &allDay,
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
                         || (eo.endTime.date() == start && (allDay.contains(eo.eventUid)
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

void NemoCalendarWorker::loadRanges(const QList<NemoCalendarData::Range> &ranges, bool reset)
{
    foreach (const NemoCalendarData::Range &range, ranges)
        mStorage->load(range.first, range.second);

    // Load all recurring incidences, we have no other way to detect if they occur within a range
    mStorage->loadRecurringIncidences();

    KCalCore::Event::List list = mCalendar->rawEvents();

    if (reset)
        mSentEvents.clear();

    QHash<QString, NemoCalendarData::Event> events;
    QStringList allDay;
    foreach (const KCalCore::Event::Ptr e, list) {
        // The database may have changed after loading the events, make sure that the notebook
        // of the event still exists.
        if (mStorage->notebook(mCalendar->notebook(e)).isNull())
            continue;

        NemoCalendarData::Event event = createEventStruct(e);
        if (!mSentEvents.contains(event.uniqueId)) {
            mSentEvents.append(event.uniqueId);
            events.insert(event.uniqueId, event);
            if (event.allDay)
                allDay << event.uniqueId;
        }
    }

    QHash<QString, NemoCalendarData::EventOccurrence> occurrences = eventOccurences(ranges);
    QHash<QDate, QStringList> dailyOccurences = dailyEventOccurrences(ranges, allDay, occurrences.values());

    emit rangesLoaded(ranges, events, occurrences, dailyOccurences, reset);
}

NemoCalendarData::Event NemoCalendarWorker::createEventStruct(const KCalCore::Event::Ptr &e) const
{
    NemoCalendarData::Event event;
    event.uniqueId = e->uid();
    KCalCore::Alarm::List alarms = e->alarms();
    for (int ii = 0; ii < alarms.count(); ++ii) {
        if (alarms.at(ii)->type() == KCalCore::Alarm::Procedure &&
                alarms.at(ii)->programArguments() == event.uniqueId) {
            event.alarmProgram = alarms.at(ii)->programFile();
            break;
        }
    }

    event.allDay = e->allDay();
    event.calendarUid = mCalendar->notebook(e);
    event.description = e->description();
    event.displayLabel = e->summary();
    event.endTime = e->dtEnd();
    event.location = e->location();
    event.readonly = mStorage->notebook(event.calendarUid)->isReadOnly();
    event.recur = convertRecurrence(e);
    event.recurExceptionDates = e->recurrence()->exDateTimes();
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


NemoCalendarData::EventOccurrence NemoCalendarWorker::getNextOccurence(const QString &uid, const QDateTime &start) const
{
    KCalCore::Event::Ptr event = mCalendar->event(uid);
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
        occurrence.startTime = eiv.dtStart;
        occurrence.endTime = eiv.dtEnd;
    }

    return occurrence;
}
