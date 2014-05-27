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

#ifndef CALENDARMANAGER_H
#define CALENDARMANAGER_H

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include "calendardata.h"
#include "calendarevent.h"

class NemoCalendarWorker;
class NemoCalendarAgendaModel;
class NemoCalendarEventOccurrence;

class NemoCalendarManager : public QObject
{
    Q_OBJECT
private:
    NemoCalendarManager();

public:
    static NemoCalendarManager *instance();
    ~NemoCalendarManager();

    NemoCalendarEvent* eventObject(const QString &eventUid);
    NemoCalendarEvent* createEvent(); // Synchronous DB thread access
    void saveEvent(const QString &uid, const QString &calendarUid);
    void deleteEvent(const QString &uid, const QDateTime &dateTime = QDateTime());
    void save();

    // Synchronous DB thread access
    QString convertEventToVCalendarSync(const QString &uid, const QString &prodId);

    // Event
    NemoCalendarData::Event getEvent(const QString& uid);

    void setAllDay(const QString &uid, bool allDay);
    void setDescription(const QString &uid, const QString &description);
    void setDisplayLabel(const QString &uid, const QString &displayLabel);
    void setEndTime(const QString &uid, const KDateTime &endTime);
    void setExceptions(const QString &uid, QList<KDateTime> exceptions);
    void setLocation(const QString &uid, const QString &location);
    void setStartTime(const QString &uid, const KDateTime &startTime);
    void setRecurrence(const QString &uid, NemoCalendarEvent::Recur recur);
    void setReminder(const QString &uid, NemoCalendarEvent::Reminder reminder);

    // Notebooks
    QList<NemoCalendarData::Notebook> notebooks();
    QString defaultNotebook() const;
    void setDefaultNotebook(const QString &notebookUid);
    QStringList excludedNotebooks();
    void setExcludedNotebooks(const QStringList &list);
    void excludeNotebook(const QString &notebookUid, bool exclude);
    void setNotebookColor(const QString &notebookUid, const QString &color);
    QString getNotebookColor(const QString &notebookUid) const;

    // AgendaModel
    void cancelAgendaRefresh(NemoCalendarAgendaModel *model);
    void scheduleAgendaRefresh(NemoCalendarAgendaModel *model);

    // Caller gets ownership of returned NemoCalendarEventOccurrence object
    // Does synchronous DB thread access - no DB operations, though, fast when no ongoing DB ops
    NemoCalendarEventOccurrence* getNextOccurence(const QString &uid, const QDateTime &start);

private slots:
    void storageModifiedSlot(QString info);

    void eventNotebookChanged(QString oldEventUid, QString newEventUid, QString notebookUid);

    void excludedNotebooksChangedSlot(QStringList excludedNotebooks);
    void notebooksChangedSlot(QList<NemoCalendarData::Notebook> notebooks);

    void rangesLoadedSlot(QList<NemoCalendarData::Range> ranges,
                          QHash<QString, NemoCalendarData::Event> events,
                          QHash<QString, NemoCalendarData::EventOccurrence> occurrences,
                          QHash<QDate, QStringList> dailyOccurences,
                          bool reset);
    void timeout();

signals:
    void excludedNotebooksChanged(QStringList excludedNotebooks);
    void notebooksAboutToChange();
    void notebooksChanged(QList<NemoCalendarData::Notebook> notebooks);
    void notebookColorChanged(QString notebookUid);
    void defaultNotebookChanged(QString notebookUid);
    void storageModified();
    void dataUpdated();
    void eventUidChanged(QString oldUid, QString newUid);

private:
    friend class tst_NemoCalendarManager;

    void doAgendaRefresh();
    bool isRangeLoaded(const QPair<QDate, QDate> &r, QList<NemoCalendarData::Range> *newRanges);
    QList<NemoCalendarData::Range> addRanges(const QList<NemoCalendarData::Range> &oldRanges,
                                             const QList<NemoCalendarData::Range> &newRanges);
    void updateAgendaModel(NemoCalendarAgendaModel *model);
    void sendEventChangeSignals(const NemoCalendarData::Event &newEvent,
                                const NemoCalendarData::Event &oldEvent);

    QThread mWorkerThread;
    NemoCalendarWorker *mCalendarWorker;
    QHash<QString, NemoCalendarData::Event> mEvents;
    QHash<QString, NemoCalendarData::Event> mModifiedEvents;
    QHash<QString, NemoCalendarEvent *> mEventObjects;
    QHash<QString, NemoCalendarData::EventOccurrence> mEventOccurrences;
    QHash<QDate, QStringList> mEventOccurrenceForDates;
    QList<NemoCalendarAgendaModel *> mAgendaRefreshList;
    QStringList mExcludedNotebooks;
    QHash<QString, NemoCalendarData::Notebook> mNotebooks;

    QTimer *mTimer;

    // If true indicates that NemoCalendarWorker::loadRanges(...) has been called, and the response
    // has not been received in slot NemoCalendarManager::rangesLoaded(...)
    bool mLoadPending;

    // If true the next call to doAgendaRefresh() will cause a complete reload of calendar data
    bool mResetPending;

    // A list of non-overlapping loaded ranges sorted by range start date
    QList<NemoCalendarData::Range > mLoadedRanges;
};

#endif // CALENDARMANAGER_H
