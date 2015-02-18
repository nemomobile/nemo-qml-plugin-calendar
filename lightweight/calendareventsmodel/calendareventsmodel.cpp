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

#include "calendareventsmodel.h"

#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QTimer>
#include <qqmlinfo.h>

#include "calendardataserviceproxy.h"

NemoCalendarEventsModel::NemoCalendarEventsModel(QObject *parent) :
    QAbstractListModel(parent),
    mProxy(0),
    mWatcher(new QFileSystemWatcher(this)),
    mUpdateDelayTimer(new QTimer(this)),
    mFilterMode(FilterNone),
    mContentType(ContentAll),
    mEventLimit(1000),
    mTotalCount(0)
{
    registerDataTypes();
    mProxy = new CalendarDataServiceProxy("org.nemomobile.calendardataservice",
                                          "/org/nemomobile/calendardataservice",
                                          QDBusConnection::sessionBus(),
                                          this);
    connect(mProxy, SIGNAL(getEventsResult(EventDataList)),
            this, SLOT(getEventsResult(EventDataList)));

    mUpdateDelayTimer->setInterval(500);
    mUpdateDelayTimer->setSingleShot(true);
    connect(mUpdateDelayTimer, SIGNAL(timeout()), this, SLOT(update()));

    QString privilegedDataDir = QString("%1/.local/share/system/privileged/Calendar/mkcal/db").arg(QDir::homePath());
    mWatcher->addPath(privilegedDataDir);
    QSettings settings("nemo", "nemo-qml-plugin-calendar");
    mWatcher->addPath(settings.fileName());
    // Updates to the calendar db will cause several file change notifications, delay update a bit
    connect(mWatcher, SIGNAL(fileChanged(QString)), mUpdateDelayTimer, SLOT(start()));
}

int NemoCalendarEventsModel::count() const
{
    return qMin(mEventDataList.count(), mEventLimit);
}

int NemoCalendarEventsModel::totalCount() const
{
    return mTotalCount;
}

QDateTime NemoCalendarEventsModel::creationDate() const
{
    return mCreationDate;
}

QDateTime NemoCalendarEventsModel::expiryDate() const
{
    return mExpiryDate;
}

int NemoCalendarEventsModel::eventLimit() const
{
    return mEventLimit;
}

void NemoCalendarEventsModel::setEventLimit(int limit)
{
    if (mEventLimit == limit || limit <= 0)
        return;

    mEventLimit = limit;
    emit eventLimitChanged();
    restartUpdateTimer(); // TODO: Could change list content without fetching data
}

QDateTime NemoCalendarEventsModel::startDate() const
{
    return mStartDate;
}

void NemoCalendarEventsModel::setStartDate(const QDateTime &startDate)
{
    if (mStartDate == startDate)
        return;

    mStartDate = startDate;
    emit startDateChanged();

    restartUpdateTimer();
}

QDateTime NemoCalendarEventsModel::endDate() const
{
    return mEndDate;
}

void NemoCalendarEventsModel::setEndDate(const QDateTime &endDate)
{
    if (mEndDate == endDate)
        return;

    mEndDate = endDate;
    emit endDateChanged();

    restartUpdateTimer();
}

int NemoCalendarEventsModel::filterMode() const
{
    return mFilterMode;
}

void NemoCalendarEventsModel::setFilterMode(int mode)
{
    if (mFilterMode == mode)
        return;

    mFilterMode = mode;
    emit filterModeChanged();
    restartUpdateTimer();
}


int NemoCalendarEventsModel::contentType() const
{
    return mContentType;
}

void NemoCalendarEventsModel::setContentType(int contentType)
{
    if (mContentType == contentType)
        return;

    mContentType = contentType;
    emit contentTypeChanged();
    restartUpdateTimer();
}

int NemoCalendarEventsModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return mEventDataList.count();
}

QVariant NemoCalendarEventsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= mEventDataList.count())
        return QVariant();

    EventData eventData = mEventDataList.at(index.row());

    switch(role) {
    case DisplayLabelRole:
        return eventData.displayLabel;
    case DescriptionRole:
        return eventData.description;
    case StartTimeRole:
        return eventData.startTime;
    case EndTimeRole:
        return eventData.endTime;
    case RecurrenceIdRole:
        return eventData.recurrenceId;
    case AllDayRole:
        return eventData.allDay;
    case LocationRole:
        return eventData.location;
    case CalendarUidRole:
        return eventData.calendarUid;
    case UidRole:
        return eventData.uniqueId;
    case ColorRole:
        return eventData.color;
    default:
        return QVariant();
    }
}

void NemoCalendarEventsModel::update()
{
    QDateTime endDate = (mEndDate.isValid()) ? mEndDate : mStartDate;
    QDBusPendingCall pcall = mProxy->getEvents(mStartDate.toString(Qt::ISODate),
                                               endDate.toString(Qt::ISODate));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);
    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                     this, SLOT(updateFinished(QDBusPendingCallWatcher*)));
}

void NemoCalendarEventsModel::updateFinished(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<void> reply = *call;
    if (reply.isError())
        qWarning() << "dbus error:" << reply.error().name() << reply.error().message();

    call->deleteLater();
}

void NemoCalendarEventsModel::getEventsResult(const EventDataList &eventDataList)
{
    if (mEventDataList.isEmpty() && eventDataList.isEmpty())
        return;

    int oldcount = mEventDataList.count();
    int oldTotalCount = mTotalCount;
    beginResetModel();
    mEventDataList.clear();
    QDateTime now = QDateTime::currentDateTime();
    QDateTime expiryDate;
    mTotalCount = 0;
    foreach (const EventData &e, eventDataList) {
        if ((e.allDay && mContentType == ContentEvents)
                || (!e.allDay && mContentType == ContentAllDay)) {
            continue;
        }

        QDateTime startTime = QDateTime::fromString(e.startTime, Qt::ISODate);
        QDateTime endTime = QDateTime::fromString(e.endTime, Qt::ISODate);
        if (e.allDay
                || (mFilterMode == FilterPast && now < endTime)
                || (mFilterMode == FilterPastAndCurrent && now < startTime)
                || (mFilterMode == FilterNone)) {
            if (mEventDataList.count() < mEventLimit) {
                mEventDataList.append(e);
                if (!e.allDay) {
                    if (mFilterMode == FilterPast && (!expiryDate.isValid() || expiryDate > endTime)) {
                        expiryDate = endTime;
                    } else if (mFilterMode == FilterPastAndCurrent && (!expiryDate.isValid() || expiryDate > startTime)) {
                        expiryDate = startTime;
                    }
                }
            }
            mTotalCount++;
        }
    }

    mCreationDate = QDateTime::currentDateTime();
    emit creationDateChanged();

    if (!expiryDate.isValid()) {
        if (mEndDate.isValid()) {
            expiryDate = mEndDate;
        } else {
            expiryDate = mStartDate.addDays(1);
            expiryDate.setTime(QTime(0,0,0,1));
        }
    }
    mExpiryDate = expiryDate;
    emit expiryDateChanged();

    endResetModel();
    if (count() != oldcount) {
        emit countChanged();
    }
    if (mTotalCount != oldTotalCount) {
        emit totalCountChanged();
    }
}

QHash<int, QByteArray> NemoCalendarEventsModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[DisplayLabelRole] = "displayLabel";
    roleNames[DescriptionRole] = "description";
    roleNames[StartTimeRole] = "startTime";
    roleNames[EndTimeRole] = "endTime";
    roleNames[RecurrenceIdRole] = "recurrenceId";
    roleNames[AllDayRole] = "allDay";
    roleNames[LocationRole] = "location";
    roleNames[CalendarUidRole] = "calendarUid";
    roleNames[UidRole] = "uid";
    roleNames[ColorRole] = "color";

    return roleNames;
}

void NemoCalendarEventsModel::restartUpdateTimer()
{
    if (mStartDate.isValid())
        mUpdateDelayTimer->start();
    else
        mUpdateDelayTimer->stop();
}
