/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Robin Burchell <robin.burchell@jollamobile.com>
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

#include <QtGlobal>

#include <QtQml>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include "calendarapi.h"
#include "calendareventquery.h"
#include "calendarnotebookmodel.h"
#include "calendarevent.h"
#include "calendaragendamodel.h"
#include "calendarmanager.h"
#include "calendarnotebookquery.h"

class QtDate : public QObject
{
    Q_OBJECT
public:
    QtDate(QObject *parent);

public slots:
    int daysTo(const QDate &, const QDate &);
    QDate addDays(const QDate &, int);

    static QObject *New(QQmlEngine *e, QJSEngine *);
};

QtDate::QtDate(QObject *parent)
: QObject(parent)
{
}

int QtDate::daysTo(const QDate &from, const QDate &to)
{
    return from.daysTo(to);
}

QDate QtDate::addDays(const QDate &date, int days)
{
    return date.addDays(days);
}

QObject *QtDate::New(QQmlEngine *e, QJSEngine *)
{
    return new QtDate(e);
}

class CalendarManagerReleaser: public QObject
{
    Q_OBJECT

public:
    CalendarManagerReleaser(QObject *parent)
        : QObject(parent)
    {
    }

    ~CalendarManagerReleaser()
    {
        // Call NemoCalendarManager dtor to ensure that the QThread managed by it
        // will be destroyed via deleteLater when control returns to the event loop.
        // Deleting NemoCalendarManager in NemoCalendarPlugin dtor is not an option
        // as it is called after the event loop is stopped.
        delete NemoCalendarManager::instance();
    }
};

class Q_DECL_EXPORT NemoCalendarPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.nemomobile.calendar")

public:
    void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        Q_UNUSED(uri)
        new CalendarManagerReleaser(engine);
    }

    void registerTypes(const char *uri)
    {
        Q_ASSERT(uri == QLatin1String("org.nemomobile.calendar"));
        qmlRegisterUncreatableType<NemoCalendarEvent>(uri, 1, 0, "CalendarEvent", "Create CalendarEvent instances through a model");
        qmlRegisterType<NemoCalendarAgendaModel>(uri, 1, 0, "AgendaModel");
        qmlRegisterType<NemoCalendarEventQuery>(uri, 1, 0, "EventQuery");
        qmlRegisterUncreatableType<Person>(uri, 1, 0, "Person", "Persons reachable only through EventQuery");
        qmlRegisterType<NemoCalendarNotebookModel>(uri, 1, 0, "NotebookModel");
        qmlRegisterType<NemoCalendarNotebookQuery>(uri, 1, 0, "NotebookQuery");
        qmlRegisterSingletonType<QtDate>(uri, 1, 0, "QtDate", QtDate::New);
        qmlRegisterSingletonType<NemoCalendarApi>(uri, 1, 0, "Calendar", NemoCalendarApi::New);
    }
};

#include "plugin.moc"
