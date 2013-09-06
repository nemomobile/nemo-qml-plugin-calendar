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

// Qt
#include <QDebug>
#include <QSettings>

// mkcal
#include <event.h>

#include "calendardb.h"
#include "calendarevent.h"
#include "calendareventcache.h"

NemoCalendarEventCache::NemoCalendarEventCache()
    : QObject(0)
    , mKCal::ExtendedStorageObserver()
{
    NemoCalendarDb::storage()->registerObserver(this);

    load();
}

void NemoCalendarEventCache::load()
{
    QSettings settings("nemo", "nemo-qml-plugin-calendar");

    mKCal::Notebook::List notebooks = NemoCalendarDb::storage()->notebooks();
    mNotebooks.clear();
    mNotebookColors.clear();

    QStringList defaultNotebookColors = QStringList() << "#00aeef" << "red" << "blue" << "green" << "pink" << "yellow";
    int nextDefaultNotebookColor = 0;

    for (int ii = 0; ii < notebooks.count(); ++ii) {
        QString uid = notebooks.at(ii)->uid();
        if (!settings.value("exclude/" + uid, false).toBool()) {
            mNotebooks.insert(uid);
            NemoCalendarDb::storage()->loadNotebookIncidences(uid);
        }

        QString color = settings.value("colors/" + uid, QString()).toString();
        if (color.isEmpty())
            color = notebooks.at(ii)->color();
        if (color.isEmpty())
            color = defaultNotebookColors.at((nextDefaultNotebookColor++) % defaultNotebookColors.count());

        mNotebookColors.insert(uid, color);
    }

    mKCal::ExtendedCalendar::Ptr calendar = NemoCalendarDb::calendar();

    for (QSet<NemoCalendarEvent *>::Iterator iter = mEvents.begin(); iter != mEvents.end(); ++iter) {
        QString uid = (*iter)->event()->uid();
        KCalCore::Event::Ptr event = calendar->event(uid);
        (*iter)->setEvent(event);
    }

    for (QSet<NemoCalendarEventOccurrence *>::Iterator iter = mEventOccurrences.begin();
         iter != mEventOccurrences.end(); ++iter) {
        QString uid = (*iter)->event()->uid();
        KCalCore::Event::Ptr event = calendar->event(uid);
        (*iter)->setEvent(event);
    }

    emit modelReset();
}

NemoCalendarEventCache *NemoCalendarEventCache::instance()
{
    static NemoCalendarEventCache *cacheInstance;
    if (!cacheInstance)
        cacheInstance = new NemoCalendarEventCache;

    return cacheInstance;
}

void NemoCalendarEventCache::storageModified(mKCal::ExtendedStorage *storage, const QString &info)
{
    // 'info' is either a path to the database (in which case we're screwed, we
    // have no idea what changed, so tell all interested models to reload) or a
    // space-seperated list of event UIDs.
    //
    // unfortunately we don't know *what* about these events changed with the
    // current mkcal API, so we'll have to try our best to guess when the time
    // comes.
    //
    // for now, let's just ask models to reload whenever a change happens.

    load();
}

void NemoCalendarEventCache::storageProgress(mKCal::ExtendedStorage *storage, const QString &info)
{

}

void NemoCalendarEventCache::storageFinished(mKCal::ExtendedStorage *storage, bool error, const QString &info)
{

}

QString NemoCalendarEventCache::notebookColor(const QString &notebook) const
{
    return mNotebookColors.value(notebook, "black");
}

void NemoCalendarEventCache::setNotebookColor(const QString &notebook, const QString &color)
{
    if (!mNotebookColors.contains(notebook))
        return;

    QSettings settings("nemo", "nemo-qml-plugin-calendar");

    mNotebookColors[notebook] = color;
    settings.setValue("colors/" + notebook, color);

    for (QSet<NemoCalendarEvent *>::Iterator iter = mEvents.begin(); iter != mEvents.end(); ++iter) {
        NemoCalendarEvent *e = *iter;
        if (NemoCalendarDb::calendar()->notebook(e->event()) == notebook)
            emit e->colorChanged();
    }
}

QList<NemoCalendarEvent *> NemoCalendarEventCache::events(const KCalCore::Event::Ptr &event)
{
    QList<NemoCalendarEvent *> rv;
    for (QSet<NemoCalendarEvent *>::ConstIterator iter = instance()->mEvents.begin();
            iter != instance()->mEvents.end(); ++iter) {
        if ((*iter)->event().data() == event.data())
            rv.append(*iter);
    }
    return rv;
}

