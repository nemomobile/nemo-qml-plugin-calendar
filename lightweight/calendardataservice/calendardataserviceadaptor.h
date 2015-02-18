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

#ifndef CALENDARTOOLADAPTOR_H_1422885580
#define CALENDARTOOLADAPTOR_H_1422885580

#include <QtCore/QObject>
#include <QtDBus/QtDBus>

#include "../common/eventdata.h"

/*
 * Adaptor class for interface org.nemomobile.calendardataservice
 */
class CalendarDataServiceAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nemomobile.calendardataservice")
    Q_CLASSINFO("D-Bus Introspection", ""
                "  <interface name=\"org.nemomobile.calendardataservice\">\n"
                "    <method name=\"getEvents\">\n"
                "      <arg direction=\"in\" type=\"s\" name=\"startDate\"/>\n"
                "      <arg direction=\"in\" type=\"s\" name=\"endDate\"/>\n"
                "    </method>\n"
                "    <signal name=\"getEventsResult\">\n"
                "      <arg type=\"a(sssssbssss)\" name=\"eventList\"/>\n"
                "    </signal>\n"
                "  </interface>\n"
                "")

public:
    CalendarDataServiceAdaptor(QObject *parent);
    virtual ~CalendarDataServiceAdaptor();

public Q_SLOTS:
    void getEvents(const QString &startDate, const QString &endDate);

Q_SIGNALS:
    void getEventsResult(const EventDataList &eventDataList);
};

#endif
