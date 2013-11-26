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

#ifdef NEMO_USE_QT5
# include <QtQml>
# include <QQmlEngine>
# include <QQmlExtensionPlugin>
# define QDeclarativeEngine QQmlEngine
# define QDeclarativeExtensionPlugin QQmlExtensionPlugin
#include "calendarapi.h"
#include "calendareventquery.h"
#include "calendarnotebookmodel.h"
#include "calendardb.h"
#else
# include <QtDeclarative/qdeclarative.h>
# include <QtDeclarative/QDeclarativeExtensionPlugin>
#endif

#include "calendarevent.h"
#include "calendaragendamodel.h"

#ifdef NEMO_USE_QT5
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


class DbReleaser: public QObject
{
    Q_OBJECT

public:
    DbReleaser(QObject *parent)
        : QObject(parent)
    {
    }

    ~DbReleaser()
    {
        // mkcal uses internally some static KTimeZone instances. Need to explicitly
        // drop references so shutting down application has deterministic order for deletions.
        NemoCalendarDb::dropReferences();
    }
};
#endif


class Q_DECL_EXPORT NemoCalendarPlugin : public QDeclarativeExtensionPlugin
{
    Q_OBJECT
#ifdef NEMO_USE_QT5
    Q_PLUGIN_METADATA(IID "org.nemomobile.calendar")
#endif
public:

#ifdef NEMO_USE_QT5
    void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        Q_UNUSED(uri)
        new DbReleaser(engine);
    }
#endif

    void registerTypes(const char *uri)
    {
        Q_ASSERT(uri == QLatin1String("org.nemomobile.calendar"));
        qmlRegisterUncreatableType<NemoCalendarEvent>(uri, 1, 0, "CalendarEvent", "Create CalendarEvent instances through a model");
        qmlRegisterType<NemoCalendarAgendaModel>(uri, 1, 0, "AgendaModel");
#ifdef NEMO_USE_QT5
        qmlRegisterType<NemoCalendarEventQuery>(uri, 1, 0, "EventQuery");
        qmlRegisterType<NemoCalendarNotebookModel>(uri, 1, 0, "NotebookModel");
        qmlRegisterSingletonType<QtDate>(uri, 1, 0, "QtDate", QtDate::New);
        qmlRegisterSingletonType<NemoCalendarApi>(uri, 1, 0, "Calendar", NemoCalendarApi::New);
#endif
    }
};

#ifndef NEMO_USE_QT5
Q_EXPORT_PLUGIN2(nemocalendar, NemoCalendarPlugin);
#endif

#ifdef NEMO_USE_QT5
#include "plugin.moc"
#endif
