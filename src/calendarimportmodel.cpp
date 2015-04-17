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

#include "calendarimportmodel.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QUrl>

// mkcal
#include <extendedcalendar.h>
#include <extendedstorage.h>

// kcalcore
#include <icalformat.h>
#include <vcalformat.h>
#include <memorycalendar.h>

#include "calendarimportevent.h"

NemoCalendarImportModel::NemoCalendarImportModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

NemoCalendarImportModel::~NemoCalendarImportModel()
{
}

int NemoCalendarImportModel::count() const
{
    return mEventList.count();
}

QString NemoCalendarImportModel::fileName() const
{
    return mFileName;
}

void NemoCalendarImportModel::setFileName(const QString &fileName)
{
    if (mFileName == fileName)
        return;

    mFileName = fileName;
    emit fileNameChanged();
    if (!mFileName.isEmpty()) {
        importToMemory(fileName);
    } else if (!mEventList.isEmpty()) {
        beginResetModel();
        mEventList.clear();
        endResetModel();
        emit countChanged();
    }
}

QObject *NemoCalendarImportModel::getEvent(int index)
{
    if (index < 0 || index >= mEventList.count())
        return 0;

    return new CalendarImportEvent(mEventList.at(index));
}

int NemoCalendarImportModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return mEventList.count();
}

QVariant NemoCalendarImportModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= mEventList.count())
        return QVariant();

    KCalCore::Event::Ptr event = mEventList.at(index.row());

    switch(role) {
    case DisplayLabelRole:
        return event->summary();
    case DescriptionRole:
        return event->description();
    case StartTimeRole:
        return event->dtStart().dateTime();
    case EndTimeRole:
        return event->dtEnd().dateTime();
    case AllDayRole:
        return event->allDay();
    case LocationRole:
        return event->location();
    case UidRole:
        return event->uid();
    default:
        return QVariant();
    }
}

bool NemoCalendarImportModel::importToNotebook(const QString &notebookUid)
{
    mKCal::ExtendedCalendar::Ptr calendar(new mKCal::ExtendedCalendar(KDateTime::Spec::LocalZone()));
    mKCal::ExtendedStorage::Ptr storage = calendar->defaultStorage(calendar);
    if (!storage->open()) {
        qWarning() << "Unable to open calendar DB";
        return false;
    }

    if (!notebookUid.isEmpty()) {
        if (! (storage->defaultNotebook() && storage->defaultNotebook()->uid() == notebookUid)) {
            mKCal::Notebook::Ptr notebook = storage->notebook(notebookUid);
            if (notebook) {
                // TODO: should we change default notebook back if we change it?
                storage->setDefaultNotebook(notebook);
            } else {
                qWarning() << "Invalid notebook UID" << notebookUid;
                return false;
            }
        }
    }

    if (importFromFile(mFileName, calendar))
        storage->save();

    storage->close();

    return true;
}

QHash<int, QByteArray> NemoCalendarImportModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[DisplayLabelRole] = "displayLabel";
    roleNames[DescriptionRole] = "description";
    roleNames[StartTimeRole] = "startTime";
    roleNames[EndTimeRole] = "endTime";
    roleNames[AllDayRole] = "allDay";
    roleNames[LocationRole] = "location";
    roleNames[UidRole] = "uid";

    return roleNames;
}

bool NemoCalendarImportModel::importFromFile(const QString &fileName,
                                             KCalCore::Calendar::Ptr calendar)
{
    QString filePath;
    QUrl url(fileName);
    if (url.isLocalFile())
        filePath = url.toLocalFile();
    else
        filePath = fileName;

    if (!(filePath.endsWith(".vcs") || filePath.endsWith(".ics"))) {
        qWarning() << "Unsupported file format" << filePath;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Unable to open file for reading" << filePath;
        return false;
    }
    QString fileContent(file.readAll());

    bool ok = false;
    if (filePath.endsWith(".vcs")) {
        KCalCore::VCalFormat vcalFormat;
        ok = vcalFormat.fromString(calendar, fileContent);
    } else if (filePath.endsWith(".ics")) {
        KCalCore::ICalFormat icalFormat;
        ok = icalFormat.fromString(calendar, fileContent);
    }
    if (!ok)
        qWarning() << "Failed to import from file" << filePath;

    return ok;
}

static bool incidenceLessThan(const KCalCore::Incidence::Ptr e1,
                              const KCalCore::Incidence::Ptr e2)
{
    if (e1->dtStart() == e2->dtStart()) {
        int cmp = QString::compare(e1->summary(),
                                   e2->summary(),
                                   Qt::CaseInsensitive);
        if (cmp == 0)
            return QString::compare(e1->uid(), e2->uid()) < 0;
        else
            return cmp < 0;
    } else {
        return e1->dtStart() < e2->dtStart();
    }
}

bool NemoCalendarImportModel::importToMemory(const QString &fileName)
{
    if (!mEventList.isEmpty())
        mEventList.clear();

    beginResetModel();
    KCalCore::MemoryCalendar::Ptr cal(new KCalCore::MemoryCalendar(KDateTime::Spec::LocalZone()));
    importFromFile(fileName, cal);
    KCalCore::Incidence::List incidenceList = cal->incidences();
    for (int i = 0; i < incidenceList.size(); i++) {
        KCalCore::Incidence::Ptr incidence = incidenceList.at(i);
        if (incidence->type() == KCalCore::IncidenceBase::TypeEvent)
            mEventList.append(incidence.staticCast<KCalCore::Event>());
    }
    if (!mEventList.isEmpty())
        qSort(mEventList.begin(), mEventList.end(), incidenceLessThan);

    endResetModel();
    emit countChanged();
    return true;
}

