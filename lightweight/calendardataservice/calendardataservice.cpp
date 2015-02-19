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

#include "calendardataservice.h"

#include <QtCore/QDate>
#include <QtCore/QDebug>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include "calendardataserviceadaptor.h"
#include "../../src/calendaragendamodel.h"
#include "../../src/calendarevent.h"
#include "../../src/calendareventoccurrence.h"

CalendarDataService::CalendarDataService(QObject *parent) :
    QObject(parent), mAgendaModel(0)
{
    registerCalendarDataServiceTypes();
    new CalendarDataServiceAdaptor(this);
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerService("org.nemomobile.calendardataservice"))
        qWarning("Can't register org.nemomobile.calendardataservice service on the session bus");

    if (!connection.registerObject("/org/nemomobile/calendardataservice", this))
        qWarning("Can't register org/nemomobile/calendardataservice object for the D-Bus service.");
}

void CalendarDataService::getEvents(const QString &startDate, const QString &endDate)
{
    QDate start = QDate::fromString(startDate, Qt::ISODate);
    QDate end = QDate::fromString(endDate, Qt::ISODate);
    if (!start.isValid() || !end.isValid()) {
        qWarning() << "Invalid date parameter(s):" << startDate << ", " << endDate;
        exit(1);
    }
    initialize();
    mAgendaModel->setStartDate(start);
    mAgendaModel->setEndDate(end);
}

void CalendarDataService::updated()
{
    EventDataList reply;
    for (int i = 0; i < mAgendaModel->count(); i++) {
        QVariant variant = mAgendaModel->get(i, NemoCalendarAgendaModel::EventObjectRole);
        QVariant occurrenceVariant = mAgendaModel->get(i, NemoCalendarAgendaModel::OccurrenceObjectRole);
        if (variant.canConvert<NemoCalendarEvent *>() && occurrenceVariant.canConvert<NemoCalendarEventOccurrence *>()) {
            NemoCalendarEvent* event = variant.value<NemoCalendarEvent *>();
            NemoCalendarEventOccurrence* occurrence = occurrenceVariant.value<NemoCalendarEventOccurrence *>();
            EventData eventStruct;
            eventStruct.displayLabel = event->displayLabel();
            eventStruct.description = event->description();
            eventStruct.startTime = occurrence->startTime().toString(Qt::ISODate);
            eventStruct.endTime = occurrence->endTime().toString(Qt::ISODate);
            eventStruct.allDay = event->allDay();
            eventStruct.color = event->color();
            eventStruct.recurrenceId = event->recurrenceIdString();
            eventStruct.uniqueId = event->uniqueId();
            reply << eventStruct;
        }
    }
    emit getEventsResult(reply);
    exit(0);
}

void CalendarDataService::initialize()
{
    if (!mAgendaModel) {
        mAgendaModel = new NemoCalendarAgendaModel(this);
        connect(mAgendaModel, SIGNAL(updated()), this, SLOT(updated()));
    }
}
