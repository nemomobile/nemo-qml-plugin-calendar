/*
 * Copyright (C) 2015 Jolla Ltd.
 * Contact: Chris Adams <chris.adams@jollamobile.com>
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

#include <QCoreApplication>
#include <QStringList>
#include <QString>
#include <QFile>
#include <QBuffer>
#include <QDataStream>
#include <QtDebug>

#include <memorycalendar.h>
#include <extendedcalendar.h>
#include <extendedstorage.h>
#include <icalformat.h>
#include <incidence.h>
#include <event.h>
#include <todo.h>
#include <journal.h>
#include <attendee.h>
#include <kdatetime.h>

#define LOG_DEBUG(msg) if (printDebug) qDebug() << msg

#define COPY_IF_NOT_EQUAL(dest, src, get, set) \
{ \
    if (dest->get != src->get) { \
        dest->set(src->get); \
    } \
}

#define RETURN_FALSE_IF_NOT_EQUAL(a, b, func, desc) {\
    if (a->func != b->func) {\
        LOG_DEBUG("Incidence" << desc << "" << "properties are not equal:" << a->func << b->func); \
        return false;\
    }\
}

#define RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(failureCheck, desc, debug) {\
    if (failureCheck) {\
        LOG_DEBUG("Incidence" << desc << "properties are not equal:" << desc << debug); \
        return false;\
    }\
}

namespace {
    mKCal::Notebook::Ptr defaultLocalCalendarNotebook(mKCal::ExtendedStorage::Ptr storage)
    {
        mKCal::Notebook::List notebooks = storage->notebooks();
        Q_FOREACH (const mKCal::Notebook::Ptr nb, notebooks) {
            if (nb->isMaster() && !nb->isShared() && nb->pluginName().isEmpty()) {
                // assume that this is the default local calendar notebook.
                return nb;
            }
        }
        qWarning() << "No default local calendar notebook found!";
        return mKCal::Notebook::Ptr();
    }
}

namespace NemoCalendarImportExport {
    namespace IncidenceHandler {
        void normalizePersonEmail(KCalCore::Person *p)
        {
            QString email = p->email().replace(QStringLiteral("mailto:"), QString(), Qt::CaseInsensitive);
            if (email != p->email()) {
                p->setEmail(email);
            }
        }

        template <typename T>
        bool pointerDataEqual(const QVector<QSharedPointer<T> > &vectorA, const QVector<QSharedPointer<T> > &vectorB)
        {
            if (vectorA.count() != vectorB.count()) {
                return false;
            }
            for (int i=0; i<vectorA.count(); i++) {
                if (vectorA[i].data() != vectorB[i].data()) {
                    return false;
                }
            }
            return true;
        }

        bool eventsEqual(const KCalCore::Event::Ptr &a, const KCalCore::Event::Ptr &b, bool printDebug)
        {
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dateEnd() != b->dateEnd(), "dateEnd", (a->dateEnd().toString() + " != " + b->dateEnd().toString()));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, transparency(), "transparency");

            // some special handling for dtEnd() depending on whether it's an all-day event or not.
            if (a->allDay() && b->allDay()) {
                RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtEnd().date() != b->dtEnd().date(), "dtEnd", (a->dtEnd().toString() + " != " + b->dtEnd().toString()));
            } else {
                RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtEnd() != b->dtEnd(), "dtEnd", (a->dtEnd().toString() + " != " + b->dtEnd().toString()));
            }

            // some special handling for isMultiday() depending on whether it's an all-day event or not.
            if (a->allDay() && b->allDay()) {
                // here we assume that both events are in "export form" (that is, exclusive DTEND)
                if (a->dtEnd().date() != b->dtEnd().date()) {
                    LOG_DEBUG("have a->dtStart()" << a->dtStart().toString() << ", a->dtEnd()" << a->dtEnd().toString());
                    LOG_DEBUG("have b->dtStart()" << b->dtStart().toString() << ", b->dtEnd()" << b->dtEnd().toString());
                    LOG_DEBUG("have a->isMultiDay()" << a->isMultiDay() << ", b->isMultiDay()" << b->isMultiDay());
                    return false;
                }
            } else {
                RETURN_FALSE_IF_NOT_EQUAL(a, b, isMultiDay(), "multiday");
            }

            // Don't compare hasEndDate() as Event(Event*) does not initialize it based on the validity of
            // dtEnd(), so it could be false when dtEnd() is valid. The dtEnd comparison above is sufficient.

            return true;
        }

        bool todosEqual(const KCalCore::Todo::Ptr &a, const KCalCore::Todo::Ptr &b, bool printDebug)
        {
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasCompletedDate(), "hasCompletedDate");
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtRecurrence() != b->dtRecurrence(), "dtRecurrence", (a->dtRecurrence().toString() + " != " + b->dtRecurrence().toString()));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasDueDate(), "hasDueDate");
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtDue() != b->dtDue(), "dtDue", (a->dtDue().toString() + " != " + b->dtDue().toString()));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasStartDate(), "hasStartDate");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, isCompleted(), "isCompleted");
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->completed() != b->completed(), "completed", (a->completed().toString() + " != " + b->completed().toString()));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, isOpenEnded(), "isOpenEnded");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, percentComplete(), "percentComplete");
            return true;
        }

        bool journalsEqual(const KCalCore::Journal::Ptr &, const KCalCore::Journal::Ptr &, bool)
        {
            // no journal-specific properties; it only uses the base incidence properties
            return true;
        }

        // Checks whether a specific set of properties are equal.
        bool copiedPropertiesAreEqual(const KCalCore::Incidence::Ptr &a, const KCalCore::Incidence::Ptr &b, bool printDebug)
        {
            if (!a || !b) {
                qWarning() << "Invalid paramters! a:" << a << "b:" << b;
                return false;
            }

            // Do not compare created() or lastModified() because we don't update these fields when
            // an incidence is updated by copyIncidenceProperties(), so they are guaranteed to be unequal.
            // TODO compare deref alarms and attachment lists to compare them also.
            // Don't compare resources() for now because KCalCore may insert QStringList("") as the resources
            // when in fact it should be QStringList(), which causes the comparison to fail.
            RETURN_FALSE_IF_NOT_EQUAL(a, b, type(), "type");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, duration(), "duration");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasDuration(), "hasDuration");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, isReadOnly(), "isReadOnly");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, comments(), "comments");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, contacts(), "contacts");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, altDescription(), "altDescription");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, categories(), "categories");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, customStatus(), "customStatus");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, description(), "description");
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(!qFuzzyCompare(a->geoLatitude(), b->geoLatitude()), "geoLatitude", (QString("%1 != %2").arg(a->geoLatitude()).arg(b->geoLatitude())));
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(!qFuzzyCompare(a->geoLongitude(), b->geoLongitude()), "geoLongitude", (QString("%1 != %2").arg(a->geoLongitude()).arg(b->geoLongitude())));
            RETURN_FALSE_IF_NOT_EQUAL(a, b, hasGeo(), "hasGeo");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, location(), "location");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, secrecy(), "secrecy");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, status(), "status");
            RETURN_FALSE_IF_NOT_EQUAL(a, b, summary(), "summary");

            // check recurrence information. Note that we only need to check the recurrence rules for equality if they both recur.
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->recurs() != b->recurs(), "recurs", a->recurs() + " != " + b->recurs());
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->recurs() && *(a->recurrence()) != *(b->recurrence()), "recurrence", "...");

            // some special handling for dtStart() depending on whether it's an all-day event or not.
            if (a->allDay() && b->allDay()) {
                RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtStart().date() != b->dtStart().date(), "dtStart", (a->dtStart().toString() + " != " + b->dtStart().toString()));
            } else {
                RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtStart() != b->dtStart(), "dtStart", (a->dtStart().toString() + " != " + b->dtStart().toString()));
            }

            // Some servers insert a mailto: in the organizer email address, so ignore this when comparing organizers
            KCalCore::Person personA(*a->organizer().data());
            KCalCore::Person personB(*b->organizer().data());
            normalizePersonEmail(&personA);
            normalizePersonEmail(&personB);
            RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(personA != personB, "organizer", (personA.fullName() + " != " + personB.fullName()));

            switch (a->type()) {
            case KCalCore::IncidenceBase::TypeEvent:
                if (!eventsEqual(a.staticCast<KCalCore::Event>(), b.staticCast<KCalCore::Event>(), printDebug)) {
                    return false;
                }
                break;
            case KCalCore::IncidenceBase::TypeTodo:
                if (!todosEqual(a.staticCast<KCalCore::Todo>(), b.staticCast<KCalCore::Todo>(), printDebug)) {
                    return false;
                }
                break;
            case KCalCore::IncidenceBase::TypeJournal:
                if (!journalsEqual(a.staticCast<KCalCore::Journal>(), b.staticCast<KCalCore::Journal>(), printDebug)) {
                    return false;
                }
                break;
            case KCalCore::IncidenceBase::TypeFreeBusy:
            case KCalCore::IncidenceBase::TypeUnknown:
                LOG_DEBUG("Unable to compare FreeBusy or Unknown incidence, assuming equal");
                break;
            }
            return true;
        }

        void copyIncidenceProperties(KCalCore::Incidence::Ptr dest, const KCalCore::Incidence::Ptr &src)
        {
            if (!dest || !src) {
                qWarning() << "Invalid parameters!";
                return;
            }
            if (dest->type() != src->type()) {
                qWarning() << "incidences do not have same type!";
                return;
            }

            KDateTime origCreated = dest->created();
            KDateTime origLastModified = dest->lastModified();

            // Copy recurrence information if required.
            if (*(dest->recurrence()) != *(src->recurrence())) {
                dest->recurrence()->clear();

                KCalCore::Recurrence *dr = dest->recurrence();
                KCalCore::Recurrence *sr = src->recurrence();

                // recurrence rules and dates
                KCalCore::RecurrenceRule::List srRRules = sr->rRules();
                for (QList<KCalCore::RecurrenceRule*>::const_iterator it = srRRules.constBegin(), end = srRRules.constEnd(); it != end; ++it) {
                    KCalCore::RecurrenceRule *r = new KCalCore::RecurrenceRule(*(*it));
                    dr->addRRule(r);
                }
                dr->setRDates(sr->rDates());
                dr->setRDateTimes(sr->rDateTimes());

                // exception rules and dates
                KCalCore::RecurrenceRule::List srExRules = sr->exRules();
                for (QList<KCalCore::RecurrenceRule*>::const_iterator it = srExRules.constBegin(), end = srExRules.constEnd(); it != end; ++it) {
                    KCalCore::RecurrenceRule *r = new KCalCore::RecurrenceRule(*(*it));
                    dr->addExRule(r);
                }
                dr->setExDates(sr->exDates());
                dr->setExDateTimes(sr->exDateTimes());
            }

            // copy the duration before the dtEnd as calling setDuration() changes the dtEnd
            COPY_IF_NOT_EQUAL(dest, src, duration(), setDuration);

            if (dest->type() == KCalCore::IncidenceBase::TypeEvent && src->type() == KCalCore::IncidenceBase::TypeEvent) {
                KCalCore::Event::Ptr destEvent = dest.staticCast<KCalCore::Event>();
                KCalCore::Event::Ptr srcEvent = src.staticCast<KCalCore::Event>();
                COPY_IF_NOT_EQUAL(destEvent, srcEvent, dtEnd(), setDtEnd);
                COPY_IF_NOT_EQUAL(destEvent, srcEvent, transparency(), setTransparency);
            }

            if (dest->type() == KCalCore::IncidenceBase::TypeTodo && src->type() == KCalCore::IncidenceBase::TypeTodo) {
                KCalCore::Todo::Ptr destTodo = dest.staticCast<KCalCore::Todo>();
                KCalCore::Todo::Ptr srcTodo = src.staticCast<KCalCore::Todo>();
                COPY_IF_NOT_EQUAL(destTodo, srcTodo, completed(), setCompleted);
                COPY_IF_NOT_EQUAL(destTodo, srcTodo, dtRecurrence(), setDtRecurrence);
                COPY_IF_NOT_EQUAL(destTodo, srcTodo, percentComplete(), setPercentComplete);
            }

            // dtStart and dtEnd changes allDay value, so must set those before copying allDay value
            COPY_IF_NOT_EQUAL(dest, src, dtStart(), setDtStart);
            COPY_IF_NOT_EQUAL(dest, src, allDay(), setAllDay);

            COPY_IF_NOT_EQUAL(dest, src, hasDuration(), setHasDuration);
            COPY_IF_NOT_EQUAL(dest, src, organizer(), setOrganizer);
            COPY_IF_NOT_EQUAL(dest, src, isReadOnly(), setReadOnly);

            if (!pointerDataEqual(src->attendees(), dest->attendees())) {
                dest->clearAttendees();
                Q_FOREACH (const KCalCore::Attendee::Ptr &attendee, src->attendees()) {
                    dest->addAttendee(attendee);
                }
            }

            if (src->comments() != dest->comments()) {
                dest->clearComments();
                Q_FOREACH (const QString &comment, src->comments()) {
                    dest->addComment(comment);
                }
            }
            if (src->contacts() != dest->contacts()) {
                dest->clearContacts();
                Q_FOREACH (const QString &contact, src->contacts()) {
                    dest->addContact(contact);
                }
            }

            COPY_IF_NOT_EQUAL(dest, src, altDescription(), setAltDescription);
            COPY_IF_NOT_EQUAL(dest, src, categories(), setCategories);
            COPY_IF_NOT_EQUAL(dest, src, customStatus(), setCustomStatus);
            COPY_IF_NOT_EQUAL(dest, src, description(), setDescription);
            COPY_IF_NOT_EQUAL(dest, src, geoLatitude(), setGeoLatitude);
            COPY_IF_NOT_EQUAL(dest, src, geoLongitude(), setGeoLongitude);
            COPY_IF_NOT_EQUAL(dest, src, hasGeo(), setHasGeo);
            COPY_IF_NOT_EQUAL(dest, src, location(), setLocation);
            COPY_IF_NOT_EQUAL(dest, src, resources(), setResources);
            COPY_IF_NOT_EQUAL(dest, src, secrecy(), setSecrecy);
            COPY_IF_NOT_EQUAL(dest, src, status(), setStatus);
            COPY_IF_NOT_EQUAL(dest, src, summary(), setSummary);
            COPY_IF_NOT_EQUAL(dest, src, revision(), setRevision);

            if (!pointerDataEqual(src->alarms(), dest->alarms())) {
                dest->clearAlarms();
                Q_FOREACH (const KCalCore::Alarm::Ptr &alarm, src->alarms()) {
                    dest->addAlarm(alarm);
                }
            }

            if (!pointerDataEqual(src->attachments(), dest->attachments())) {
                dest->clearAttachments();
                Q_FOREACH (const KCalCore::Attachment::Ptr &attachment, src->attachments()) {
                    dest->addAttachment(attachment);
                }
            }

            // Don't change created and lastModified properties as that affects mkcal
            // calculations for when the incidence was added and modified in the db.
            if (origCreated != dest->created()) {
                dest->setCreated(origCreated);
            }
            if (origLastModified != dest->lastModified()) {
                dest->setLastModified(origLastModified);
            }
        }

        void prepareImportedIncidence(KCalCore::Incidence::Ptr incidence, bool printDebug)
        {
            if (incidence->type() != KCalCore::IncidenceBase::TypeEvent) {
                qWarning() << "unable to handle imported non-event incidence; skipping";
                return;
            }
            KCalCore::Event::Ptr event = incidence.staticCast<KCalCore::Event>();

            if (event->allDay()) {
                KDateTime dtStart = event->dtStart();
                KDateTime dtEnd = event->dtEnd();

                // calendar requires all-day events to have times in order to appear correctly
                if (dtStart.isDateOnly()) {
                    dtStart.setTime(QTime(0, 0, 0, 0));
                    event->setDtStart(dtStart);
                    LOG_DEBUG("Added time to DTSTART, now" << dtStart.toString() << "for" << incidence->uid());
                }
                if (dtEnd.isValid() && dtEnd.isDateOnly()) {
                    dtEnd.setTime(QTime(0, 0, 0, 0));
                    event->setDtEnd(dtEnd);
                    LOG_DEBUG("Added time to DTEND, now" << dtEnd.toString() << "for" << incidence->uid());
                }

                // calendar processing requires all-day events to have a dtEnd
                if (!dtEnd.isValid()) {
                    LOG_DEBUG("Adding DTEND to" << incidence->uid() << "as" << dtStart.toString());
                    event->setDtEnd(dtStart);
                }

                // setting dtStart/End changes the allDay value, so ensure it is still set to true
                event->setAllDay(true);
            }
        }

        KCalCore::Incidence::Ptr incidenceToExport(KCalCore::Incidence::Ptr sourceIncidence, bool printDebug)
        {
            if (sourceIncidence->type() != KCalCore::IncidenceBase::TypeEvent) {
                LOG_DEBUG("Incidence not an event; cannot create exportable version");
                return sourceIncidence;
            }

            KCalCore::Incidence::Ptr incidence = QSharedPointer<KCalCore::Incidence>(sourceIncidence->clone());
            KCalCore::Event::Ptr event = incidence.staticCast<KCalCore::Event>();
            bool eventIsAllDay = event->allDay();
            if (eventIsAllDay) {
                if (event->dtStart() == event->dtEnd()) {
                    // A single-day all-day event was received without a DTEND, and it is still a single-day
                    // all-day event, so remove the DTEND before upsyncing.
                    LOG_DEBUG("Removing DTEND from" << incidence->uid());
                    event->setDtEnd(KDateTime());
                } else {
                    KDateTime dt;
                    if (event->hasEndDate()) {
                        // Event::dtEnd() is inclusive, but DTEND in iCalendar format is exclusive.
                        dt = KDateTime(event->dtEnd().addDays(1).date(), event->dtEnd().timeSpec());
                        LOG_DEBUG("Adding +1 day to DTEND to make exclusive DTEND for" << incidence->uid() << ":" << dt.toString());
                    } else {
                        // No DTEND exists in event, but it's all day.  Set to DTSTART+1 to make exclusive DTEND.
                        dt = KDateTime(event->dtStart().addDays(1).date(), event->dtStart().timeSpec());
                        LOG_DEBUG("Setting DTEND to DTSTART+1 for" << incidence->uid() << ":" << dt.toString());
                    }
                    dt.setDateOnly(true);
                    LOG_DEBUG("Stripping time from date-only DTEND:" << dt.toString());
                    event->setDtEnd(dt);
                }
            }

            if (event->dtStart().isDateOnly()) {
                KDateTime dt = KDateTime(event->dtStart().date(), event->dtStart().timeSpec());
                dt.setDateOnly(true);
                event->setDtStart(dt);
                LOG_DEBUG("Stripping time from date-only DTSTART:" << dt.toString());
            }

            // setting dtStart/End changes the allDay value, so ensure it is still set to true if needed.
            if (eventIsAllDay) {
                event->setAllDay(true);
            }

            // The default storage implementation applies the organizer as an attendee by default.
            // Undo this as it turns the incidence into a scheduled event requiring acceptance/rejection/etc.
            const KCalCore::Person::Ptr organizer = event->organizer();
            if (organizer) {
                Q_FOREACH (const KCalCore::Attendee::Ptr &attendee, event->attendees()) {
                    if (attendee->email() == organizer->email() && attendee->fullName() == organizer->fullName()) {
                        LOG_DEBUG("Discarding organizer as attendee" << attendee->fullName());
                        event->deleteAttendee(attendee);
                    } else {
                        LOG_DEBUG("Not discarding attendee:" << attendee->fullName() << attendee->email() << ": not organizer:" << organizer->fullName() << organizer->email());
                    }
                }
            }

            return event;
        }
    }

    QString constructExportIcs(mKCal::ExtendedCalendar::Ptr calendar, KCalCore::Incidence::List incidencesToExport, bool printDebug)
    {
        // create an in-memory calendar
        // add to it the required incidences (ie, check if has recurrenceId -> load parent and all instances; etc)
        // for each of those, we need to do the IncidenceToExport() modifications first
        // then, export from that calendar to .ics file.
        KCalCore::MemoryCalendar::Ptr memoryCalendar(new KCalCore::MemoryCalendar(KDateTime::UTC));
        Q_FOREACH (KCalCore::Incidence::Ptr toExport, incidencesToExport) {
            if (toExport->hasRecurrenceId() || toExport->recurs()) {
                KCalCore::Incidence::Ptr recurringIncidence = toExport->hasRecurrenceId()
                                                        ? calendar->incidence(toExport->uid(), KDateTime())
                                                        : toExport;
                KCalCore::Incidence::List instances = calendar->instances(recurringIncidence);
                KCalCore::Incidence::Ptr exportableIncidence = IncidenceHandler::incidenceToExport(recurringIncidence, printDebug);

                // remove EXDATE values from the recurring incidence which correspond to the persistent occurrences (instances)
                Q_FOREACH (KCalCore::Incidence::Ptr instance, instances) {
                    QList<KDateTime> exDateTimes = exportableIncidence->recurrence()->exDateTimes();
                    exDateTimes.removeAll(instance->recurrenceId());
                    exportableIncidence->recurrence()->setExDateTimes(exDateTimes);
                }

                // store the base recurring event into the in-memory calendar
                memoryCalendar->addIncidence(exportableIncidence);

                // now create the persistent occurrences in the in-memory calendar
                Q_FOREACH (KCalCore::Incidence::Ptr instance, instances) {
                    // We cannot call dissociateSingleOccurrence() on the MemoryCalendar
                    // as that's an mKCal specific function.
                    // We cannot call dissociateOccurrence() because that function
                    // takes only a QDate instead of a KDateTime recurrenceId.
                    // Thus, we need to manually create an exception occurrence.
                    KCalCore::Incidence::Ptr exportableOccurrence(exportableIncidence->clone());
                    exportableOccurrence->setCreated(instance->created());
                    exportableOccurrence->setRevision(instance->revision());
                    exportableOccurrence->clearRecurrence();
                    exportableOccurrence->setRecurrenceId(instance->recurrenceId());
                    exportableOccurrence->setDtStart(instance->recurrenceId());

                    // add it, and then update it in-memory.
                    memoryCalendar->addIncidence(exportableOccurrence);
                    exportableOccurrence = memoryCalendar->incidence(instance->uid(), instance->recurrenceId());
                    exportableOccurrence->startUpdates();
                    IncidenceHandler::copyIncidenceProperties(exportableOccurrence, IncidenceHandler::incidenceToExport(instance, printDebug));
                    exportableOccurrence->endUpdates();
                }
            } else {
                KCalCore::Incidence::Ptr exportableIncidence = IncidenceHandler::incidenceToExport(toExport, printDebug);
                memoryCalendar->addIncidence(exportableIncidence);
            }
        }

        KCalCore::ICalFormat icalFormat;
        return icalFormat.toString(memoryCalendar, QString(), false);
    }

    QString constructExportIcs(const QString &notebookUid, const QString &incidenceUid, const KDateTime &recurrenceId, bool printDebug)
    {
        // if notebookUid empty, we fall back to the default notebook.
        // if incidenceUid is empty, we load all incidences from the notebook.
        mKCal::ExtendedCalendar::Ptr calendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::UTC()));
        mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
        storage->open();
        storage->load();
        mKCal::Notebook::Ptr notebook = notebookUid.isEmpty() ? defaultLocalCalendarNotebook(storage) : storage->notebook(notebookUid);
        if (!notebook) {
            qWarning() << "No default notebook exists or invalid notebook uid specified:" << notebookUid;
            storage->close();
            return QString();
        }

        KCalCore::Incidence::List incidencesToExport;
        if (incidenceUid.isEmpty()) {
            storage->loadNotebookIncidences(notebook->uid());
            storage->allIncidences(&incidencesToExport, notebook->uid());
        } else {
            storage->load(incidenceUid);
            incidencesToExport << calendar->incidence(incidenceUid, recurrenceId);
        }

        QString retn = constructExportIcs(calendar, incidencesToExport, printDebug);
        storage->close();
        return retn;
    }

    bool updateIncidence(mKCal::ExtendedCalendar::Ptr calendar, mKCal::Notebook::Ptr notebook, KCalCore::Incidence::Ptr incidence, bool *criticalError, bool printDebug)
    {
        if (incidence.isNull()) {
            return false;
        }

        KCalCore::Incidence::Ptr storedIncidence;
        switch (incidence->type()) {
        case KCalCore::IncidenceBase::TypeEvent:
            storedIncidence = calendar->event(incidence->uid(), incidence->hasRecurrenceId() ? incidence->recurrenceId() : KDateTime());
            break;
        case KCalCore::IncidenceBase::TypeTodo:
            storedIncidence = calendar->todo(incidence->uid());
            break;
        case KCalCore::IncidenceBase::TypeJournal:
            storedIncidence = calendar->journal(incidence->uid());
            break;
        case KCalCore::IncidenceBase::TypeFreeBusy:
        case KCalCore::IncidenceBase::TypeUnknown:
            qWarning() << "Unsupported incidence type:" << incidence->type();
            return false;
        }
        if (storedIncidence) {
            if (incidence->status() == KCalCore::Incidence::StatusCanceled
                    || incidence->customStatus().compare(QStringLiteral("CANCELLED"), Qt::CaseInsensitive) == 0) {
                LOG_DEBUG("Deleting cancelled event:" << storedIncidence->uid() << storedIncidence->recurrenceId().toString());
                if (!calendar->deleteIncidence(storedIncidence)) {
                    qWarning() << "Error removing cancelled occurrence:" << storedIncidence->uid() << storedIncidence->recurrenceId().toString();
                    return false;
                }
            } else {
                LOG_DEBUG("Updating existing event:" << storedIncidence->uid() << storedIncidence->recurrenceId().toString());
                storedIncidence->startUpdates();
                IncidenceHandler::prepareImportedIncidence(incidence, printDebug);
                IncidenceHandler::copyIncidenceProperties(storedIncidence, incidence);

                // if this incidence is a recurring incidence, we should get all persistent occurrences
                // and add them back as EXDATEs.  This is because mkcal expects that dissociated
                // single instances will correspond to an EXDATE, but most sync servers do not (and
                // so will not include the RECURRENCE-ID values as EXDATEs of the parent).
                if (storedIncidence->recurs()) {
                    KCalCore::Incidence::List instances = calendar->instances(incidence);
                    Q_FOREACH (KCalCore::Incidence::Ptr instance, instances) {
                        if (instance->hasRecurrenceId()) {
                            storedIncidence->recurrence()->addExDateTime(instance->recurrenceId());
                        }
                    }
                }
                storedIncidence->endUpdates();
            }
        } else {
            // the new incidence will be either a new persistent occurrence, or a new base-series (or new non-recurring).
            LOG_DEBUG("Have new incidence:" << incidence->uid() << incidence->recurrenceId().toString());
            KCalCore::Incidence::Ptr occurrence;
            if (incidence->hasRecurrenceId()) {
                // no dissociated occurrence exists already (ie, it's not an update), so create a new one.
                // need to detach, and then copy the properties into the detached occurrence.
                KCalCore::Incidence::Ptr recurringIncidence = calendar->event(incidence->uid(), KDateTime());
                if (recurringIncidence.isNull()) {
                    qWarning() << "error: parent recurring incidence could not be retrieved:" << incidence->uid();
                    return false;
                }
                occurrence = calendar->dissociateSingleOccurrence(recurringIncidence, incidence->recurrenceId(), incidence->recurrenceId().timeSpec());
                if (occurrence.isNull()) {
                    qWarning() << "error: could not dissociate occurrence from recurring event:" << incidence->uid() << incidence->recurrenceId().toString();
                    return false;
                }

                IncidenceHandler::prepareImportedIncidence(incidence, printDebug);
                IncidenceHandler::copyIncidenceProperties(occurrence, incidence);
                if (!calendar->addEvent(occurrence.staticCast<KCalCore::Event>(), notebook->uid())) {
                    qWarning() << "error: could not add dissociated occurrence to calendar";
                    return false;
                }
                LOG_DEBUG("Added new occurrence incidence:" << occurrence->uid() << occurrence->recurrenceId().toString());
            } else {
                // just a new event without needing detach.
                IncidenceHandler::prepareImportedIncidence(incidence, printDebug);
                bool added = false;
                switch (incidence->type()) {
                case KCalCore::IncidenceBase::TypeEvent:
                    added = calendar->addEvent(incidence.staticCast<KCalCore::Event>(), notebook->uid());
                    break;
                case KCalCore::IncidenceBase::TypeTodo:
                    added = calendar->addTodo(incidence.staticCast<KCalCore::Todo>(), notebook->uid());
                    break;
                case KCalCore::IncidenceBase::TypeJournal:
                    added = calendar->addJournal(incidence.staticCast<KCalCore::Journal>(), notebook->uid());
                    break;
                case KCalCore::IncidenceBase::TypeFreeBusy:
                case KCalCore::IncidenceBase::TypeUnknown:
                    qWarning() << "Unsupported incidence type:" << incidence->type();
                    return false;
                }
                if (added) {
                    LOG_DEBUG("Added new incidence:" << incidence->uid() << incidence->recurrenceId().toString());
                } else {
                    qWarning() << "Unable to add incidence" << incidence->uid() << incidence->recurrenceId().toString() << "to notebook" << notebook->uid();
                    *criticalError = true;
                    return false;
                }
            }
        }
        return true;
    }

    bool importIcsData(const QString &icsData, const QString &notebookUid, bool destructiveImport, bool printDebug)
    {
        KCalCore::ICalFormat iCalFormat;
        KCalCore::MemoryCalendar::Ptr cal(new KCalCore::MemoryCalendar(KDateTime::UTC));
        if (!iCalFormat.fromString(cal, icsData)) {
            qWarning() << "unable to parse iCal data";
        }

        // Reorganize the list of imported incidences into lists of incidences segregated by UID.
        QHash<QString, KCalCore::Incidence::List> uidIncidences;
        KCalCore::Incidence::List importedIncidences = cal->incidences();
        Q_FOREACH (KCalCore::Incidence::Ptr imported, importedIncidences) {
            IncidenceHandler::prepareImportedIncidence(imported, printDebug);
            uidIncidences[imported->uid()] << imported;
        }

        // Now save the imported incidences into the calendar.
        // Note that the import may specify updates to existing events, so
        // we will need to compare the imported incidences with the
        // existing incidences, by UID.
        mKCal::ExtendedCalendar::Ptr calendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::UTC()));
        mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
        storage->open();
        storage->load();
        mKCal::Notebook::Ptr notebook = notebookUid.isEmpty() ? defaultLocalCalendarNotebook(storage) : storage->notebook(notebookUid);
        if (!notebook) {
            qWarning() << "No default notebook exists or invalid notebook uid specified:" << notebookUid;
            storage->close();
            return false;
        }
        KCalCore::Incidence::List notebookIncidences;
        storage->loadNotebookIncidences(notebook->uid());
        storage->allIncidences(&notebookIncidences, notebook->uid());

        if (destructiveImport) {
            // Any incidences which don't exist in the import list should be deleted.
            Q_FOREACH (KCalCore::Incidence::Ptr possiblyDoomed, notebookIncidences) {
                if (!uidIncidences.contains(possiblyDoomed->uid())) {
                    // no incidence or series with this UID exists in the import list.
                    LOG_DEBUG("Removing rolled-back incidence:" << possiblyDoomed->uid() << possiblyDoomed->recurrenceId().toString());
                    if (!calendar->deleteIncidence(possiblyDoomed)) {
                        qWarning() << "Error removing rolled-back incidence:" << possiblyDoomed->uid() << possiblyDoomed->recurrenceId().toString();
                        storage->close();
                        return false;
                    }
                } // no need to remove rolled-back persistent occurrences here; we do that later.
            }
        }

        Q_FOREACH (const QString &uid, uidIncidences.keys()) {
            // deal with every incidence or series from the import list.
            KCalCore::Incidence::List incidences(uidIncidences[uid]);
            // find the recurring incidence (parent) in the import list, and save it.
            // alternatively, it may be a non-recurring base incidence.
            bool criticalError = false;
            int parentIndex = -1;
            for (int i = 0; i < incidences.size(); ++i) {
                if (!incidences[i]->hasRecurrenceId()) {
                    parentIndex = i;
                    break;
                }
            }

            if (parentIndex == -1) {
                LOG_DEBUG("No parent or base incidence in incidence list, performing direct updates to persistent occurrences");
                for (int i = 0; i < incidences.size(); ++i) {
                    KCalCore::Incidence::Ptr importInstance = incidences[i];
                    updateIncidence(calendar, notebook, importInstance, &criticalError, printDebug);
                    if (criticalError) {
                        qWarning() << "Error saving updated persistent occurrence:" << importInstance->uid() << importInstance->recurrenceId().toString();
                        storage->close();
                        return false;
                    }
                }
            } else {
                // if there was a parent / base incidence, then we need to compare local/import lists.
                // load the local (persistent) occurrences of the series.  Later we will update or remove them as required.
                KCalCore::Incidence::Ptr localBaseIncidence = calendar->incidence(uid, KDateTime());
                KCalCore::Incidence::List localInstances;
                if (!localBaseIncidence.isNull() && localBaseIncidence->recurs()) {
                    localInstances = calendar->instances(localBaseIncidence);
                }

                // first save the added/updated base incidence
                LOG_DEBUG("Saving the added/updated base incidence before saving persistent exceptions:" << incidences[parentIndex]->uid());
                KCalCore::Incidence::Ptr updatedBaseIncidence = incidences[parentIndex];
                updateIncidence(calendar, notebook, updatedBaseIncidence, &criticalError, printDebug); // update the base incidence first.
                if (criticalError) {
                    qWarning() << "Error saving base incidence:" << updatedBaseIncidence->uid();
                    storage->close();
                    return false;
                }

                // update persistent exceptions which are in the import list.
                QList<KDateTime> importRecurrenceIds;
                for (int i = 0; i < incidences.size(); ++i) {
                    if (i == parentIndex) {
                        continue; // already handled this one.
                    }

                    LOG_DEBUG("Now saving a persistent exception:" << incidences[i]->recurrenceId().toString());
                    KCalCore::Incidence::Ptr importInstance = incidences[i];
                    importRecurrenceIds.append(importInstance->recurrenceId());
                    updateIncidence(calendar, notebook, importInstance, &criticalError, printDebug);
                    if (criticalError) {
                        qWarning() << "Error saving updated persistent occurrence:" << importInstance->uid() << importInstance->recurrenceId().toString();
                        storage->close();
                        return false;
                    }
                }

                if (destructiveImport) {
                    // remove persistent exceptions which are not in the import list.
                    for (int i = 0; i < localInstances.size(); ++i) {
                        KCalCore::Incidence::Ptr localInstance = localInstances[i];
                        if (!importRecurrenceIds.contains(localInstance->recurrenceId())) {
                            LOG_DEBUG("Removing rolled-back persistent occurrence:" << localInstance->uid() << localInstance->recurrenceId().toString());
                            if (!calendar->deleteIncidence(localInstance)) {
                                qWarning() << "Error removing rolled-back persistent occurrence:" << localInstance->uid() << localInstance->recurrenceId().toString();
                                storage->close();
                                return false;
                            }
                        }
                    }
                }
            }
        }

        storage->save();
        storage->close();
        return true;
    }
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString usage = QStringLiteral("usage: icalconverter import|export filename [-d|destructive] [-v|verbose]\n"
                                   "Examples:\n"
                                   "To import the ICS data found in backup.ics:\n"
                                   "icalconverter import backup.ics\n"
                                   "To export the calendar to newBackup.ics:\n"
                                   "icalconverter export newBackup.ics\n"
                                   "Note: if the -d or destructive argument is provided, local calendar data will be removed prior to import.\n"
                                   "Note: if the -v or verbose argument is provided, extra debugging will be printed.\n\n");

    QStringList args = app.arguments();
    if (args.size() < 3 || args.size() > 5
            || (args[1] != QStringLiteral("import") && args[1] != QStringLiteral("export"))
            || (args.size() == 4 && (args[3] != QStringLiteral("-d") && args[3] != QStringLiteral("destructive"))
                                 && (args[3] != QStringLiteral("-v") && args[3] != QStringLiteral("verbose")))
            || (args.size() == 5 && ((args[3] != QStringLiteral("-d") && args[3] != QStringLiteral("destructive"))
                                    || (args[4] != QStringLiteral("-v") && args[4] != QStringLiteral("verbose"))))) {
        qWarning() << usage;
    } else {
        // parse arguments
        bool verbose = false, destructive = false;
        if (args.size() == 4) {
            if (args[3] == QStringLiteral("-d") || args[3] != QStringLiteral("destructive")) {
                destructive = true;
            } else {
                verbose = true;
            }
        } else if (args.size() == 5) {
            destructive = true;
            verbose = true;
        }

        // perform required operation
        if (args[1] == QStringLiteral("import")) {
            if (!QFile::exists(args[2])) {
                qWarning() << "no such file exists:" << args[2] << "; cannot import.";
            } else {
                QFile importFile(args[2]);
                if (importFile.open(QIODevice::ReadOnly)) {
                    QString fileData = QString::fromUtf8(importFile.readAll());
                    if (NemoCalendarImportExport::importIcsData(fileData, QString(), destructive, verbose)) {
                        qDebug() << "Successfully imported:" << args[2];
                        return 0;
                    }
                    qWarning() << "Failed to import:" << args[2];
                } else {
                    qWarning() << "Unable to open:" << args[2] << "for import.";
                }
            }
        } else { // "export"
            QString exportIcsData = NemoCalendarImportExport::constructExportIcs(QString(), QString(), KDateTime(), verbose);
            if (exportIcsData.isEmpty()) {
                qWarning() << "No data to export!";
                return 0;
            }
            QFile exportFile(args[2]);
            if (exportFile.open(QIODevice::WriteOnly)) {
                QByteArray ba = exportIcsData.toUtf8();
                qint64 bytesRemaining = ba.size();
                while (bytesRemaining) {
                    qint64 count = exportFile.write(ba, bytesRemaining);
                    if (count == -1) {
                        qWarning() << "Error while writing export data to:" << args[2];
                        return 1;
                    } else {
                        bytesRemaining -= count;
                    }
                }
                qDebug() << "Successfully wrote:" << ba.size() << "bytes of export data to:" << args[2];
                return 0;
            } else {
                qWarning() << "Unable to open:" << args[2] << "for export.";
            }
        }
    }

    return 1;
}
