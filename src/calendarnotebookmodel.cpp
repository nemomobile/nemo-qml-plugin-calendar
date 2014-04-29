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

#include "calendarnotebookmodel.h"

#include "calendardata.h"
#include "calendarmanager.h"

NemoCalendarNotebookModel::NemoCalendarNotebookModel()
{
    connect(NemoCalendarManager::instance(), SIGNAL(notebooksChanged(QList<NemoCalendarData::Notebook>)),
            this, SLOT(notebooksChanged()));
    connect(NemoCalendarManager::instance(), SIGNAL(notebooksAboutToChange()),
            this, SLOT(notebooksAboutToChange()));
}

int NemoCalendarNotebookModel::rowCount(const QModelIndex &index) const
{
    if (index != QModelIndex())
        return 0;

    return NemoCalendarManager::instance()->notebooks().count();
}

QVariant NemoCalendarNotebookModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= NemoCalendarManager::instance()->notebooks().count())
        return QVariant();

    NemoCalendarData::Notebook notebook = NemoCalendarManager::instance()->notebooks().at(index.row());

    switch (role) {
    case NameRole:
        return notebook.name;
    case UidRole:
        return notebook.uid;
    case DescriptionRole:
        return notebook.description;
    case ColorRole:
        return notebook.color;
    case DefaultRole:
        return notebook.isDefault;
    case ReadOnlyRole:
        return notebook.readOnly;
    case LocalCalendarRole:
        return notebook.localCalendar;
    default:
        return QVariant();
    }
}

bool NemoCalendarNotebookModel::setData(const QModelIndex &index, const QVariant &data, int role)
{
    // QAbstractItemModel::setData() appears to assume values getting set synchronously, however
    // we use asynchronous functions from NemoCalendarManager in this function. The values may not
    // have changed when we emit the dataChanged() signal, in which case the model will update
    // unnecessary, the real change happening only after the notebooksChanged signal is received.
    // TODO: cache the notebook data to improve change signaling

    if (!index.isValid()
            || index.row() >= NemoCalendarManager::instance()->notebooks().count()
            || (role != ColorRole && role != DefaultRole))
        return false;

    NemoCalendarData::Notebook notebook = NemoCalendarManager::instance()->notebooks().at(index.row());

    if (role == ColorRole) {
        NemoCalendarManager::instance()->setNotebookColor(notebook.uid, data.toString());
        emit dataChanged(index, index, QVector<int>() << role);
    } else if (role == DefaultRole) {
        NemoCalendarManager::instance()->setDefaultNotebook(notebook.uid);
        emit dataChanged(this->index(0, 0), this->index(NemoCalendarManager::instance()->notebooks().count() - 1, 0), QVector<int>() << role);
    }

    return true;
}

void NemoCalendarNotebookModel::notebooksAboutToChange()
{
    beginResetModel();
}

void NemoCalendarNotebookModel::notebooksChanged()
{
    endResetModel();
}

QHash<int, QByteArray> NemoCalendarNotebookModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[NameRole] = "name";
    roleNames[UidRole] = "uid";
    roleNames[DescriptionRole] = "description";
    roleNames[ColorRole] = "color";
    roleNames[DefaultRole] = "isDefault";
    roleNames[ReadOnlyRole] = "readOnly";
    roleNames[LocalCalendarRole] = "localCalendar";

    return roleNames;
}
