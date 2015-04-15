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

#ifndef CALENDARIMPORTEVENT_H
#define CALENDARIMPORTEVENT_H

#include <QObject>

// kCalCore
#include <event.h>

#include "calendarevent.h"

class CalendarImportEvent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString displayLabel READ displayLabel CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QDateTime startTime READ startTime CONSTANT)
    Q_PROPERTY(QDateTime endTime READ endTime CONSTANT)
    Q_PROPERTY(bool allDay READ allDay CONSTANT)
    Q_PROPERTY(NemoCalendarEvent::Recur recur READ recur CONSTANT)
    Q_PROPERTY(NemoCalendarEvent::Reminder reminder READ reminder CONSTANT)
    Q_PROPERTY(QString uniqueId READ uniqueId CONSTANT)
    Q_PROPERTY(QString color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QString location READ location CONSTANT)
    Q_PROPERTY(QList<QObject*> attendees READ attendees CONSTANT)

public:
    CalendarImportEvent(KCalCore::Event::Ptr event);

    QString displayLabel() const;
    QString description() const;
    QDateTime startTime() const;
    QDateTime endTime() const;
    bool allDay() const;
    NemoCalendarEvent::Recur recur();
    NemoCalendarEvent::Reminder reminder() const;
    QString uniqueId() const;
    QString color() const;
    QString location() const;
    QList<QObject*> attendees() const;

    void setColor(const QString &color);

public slots:
    QObject *nextOccurrence();

signals:
    void colorChanged();

private:
    KCalCore::Event::Ptr mEvent;
    QString mColor;
};
#endif // CALENDARIMPORTEVENT_H
