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

#ifndef CALENDAREVENTQUERY_H
#define CALENDAREVENTQUERY_H

#include <QObject>
#include <QDateTime>
#include <QQmlParserStatus>

#include "calendardata.h"

class NemoCalendarEventOccurrence;
class NemoCalendarEventQuery : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString uniqueId READ uniqueId WRITE setUniqueId NOTIFY uniqueIdChanged)
    Q_PROPERTY(QDateTime startTime READ startTime WRITE setStartTime RESET resetStartTime NOTIFY startTimeChanged)
    Q_PROPERTY(QObject *event READ event NOTIFY eventChanged)
    Q_PROPERTY(QObject *occurrence READ occurrence NOTIFY occurrenceChanged)

public:
    NemoCalendarEventQuery();
    ~NemoCalendarEventQuery();

    QString uniqueId() const;
    void setUniqueId(const QString &);

    QDateTime startTime() const;
    void setStartTime(const QDateTime &);
    void resetStartTime();

    QObject *event() const;
    QObject *occurrence() const;

    virtual void classBegin();
    virtual void componentComplete();

    void doRefresh(NemoCalendarData::Event event);

signals:
    void uniqueIdChanged();
    void eventChanged();
    void occurrenceChanged();
    void startTimeChanged();

    // Indicates that the event UID has changed in database, event has been moved between notebooks.
    // The property uniqueId will not be changed, the data pointer properties event and occurrence
    // will reset to null pointers.
    void newUniqueId(QString newUid);

private slots:
    void refresh();
    void eventUidChanged(QString oldUid, QString newUid);

private:
    bool mIsComplete;
    QString mUid;
    QDateTime mStartTime;
    NemoCalendarData::Event mEvent;
    NemoCalendarEventOccurrence *mOccurrence;
};

#endif
