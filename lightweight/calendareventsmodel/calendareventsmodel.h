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

#ifndef CALENDAREVENTSMODEL_H
#define CALENDAREVENTSMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QTimer>

#include "../common/eventdata.h"

class QDBusPendingCallWatcher;
class CalendarDataServiceProxy;
class QFileSystemWatcher;

class NemoCalendarEventsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_ENUMS(FilterMode)
    Q_ENUMS(ContentType)
    Q_PROPERTY(QDateTime startDate READ startDate WRITE setStartDate NOTIFY startDateChanged)
    Q_PROPERTY(QDateTime endDate READ endDate WRITE setEndDate NOTIFY endDateChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int filterMode READ filterMode WRITE setFilterMode NOTIFY filterModeChanged)
    Q_PROPERTY(QDateTime creationDate READ creationDate NOTIFY creationDateChanged)
    Q_PROPERTY(QDateTime expiryDate READ expiryDate NOTIFY expiryDateChanged)
    Q_PROPERTY(int eventLimit READ eventLimit WRITE setEventLimit NOTIFY eventLimitChanged)
    Q_PROPERTY(int contentType READ contentType WRITE setContentType NOTIFY contentTypeChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)

public:
    enum FilterMode {
        FilterNone,
        FilterPast,
        FilterPastAndCurrent
    };

    enum ContentType {
        ContentAllDay,
        ContentEvents,
        ContentAll
    };

    enum {
        DisplayLabelRole = Qt::UserRole,
        DescriptionRole,
        StartTimeRole,
        EndTimeRole,
        RecurrenceIdRole,
        AllDayRole,
        LocationRole,
        CalendarUidRole,
        UidRole,
        ColorRole
    };

    explicit NemoCalendarEventsModel(QObject *parent = 0);

    QDateTime startDate() const;
    void setStartDate(const QDateTime &startDate);

    QDateTime endDate() const;
    void setEndDate(const QDateTime &endDate);

    int filterMode() const;
    void setFilterMode(int mode);

    int contentType() const;
    void setContentType(int contentType);

    int count() const;

    int totalCount() const;

    QDateTime creationDate() const;

    QDateTime expiryDate() const;

    int eventLimit() const;
    void setEventLimit(int limit);

    virtual int rowCount(const QModelIndex &index) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

signals:
    void startDateChanged();
    void endDateChanged();
    void countChanged();
    void filterModeChanged();
    void contentTypeChanged();
    void creationDateChanged();
    void expiryDateChanged();
    void eventLimitChanged();
    void totalCountChanged();

public slots:
    void update();

private slots:
    void updateFinished(QDBusPendingCallWatcher *call);
    void getEventsResult(const EventDataList &eventDataList);

protected:
    virtual QHash<int, QByteArray> roleNames() const;

private:
    void restartUpdateTimer();

    CalendarDataServiceProxy *mProxy;
    QFileSystemWatcher *mWatcher;
    QTimer mUpdateDelayTimer;
    EventDataList mEventDataList;
    QDateTime mStartDate;
    QDateTime mEndDate;
    QDateTime mCreationDate;
    QDateTime mExpiryDate;
    int mFilterMode;
    int mContentType;
    int mEventLimit;
    int mTotalCount;
};

#endif // CALENDAREVENTSMODEL_H
