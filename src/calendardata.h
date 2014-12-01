#ifndef NEMOCALENDARDATA_H
#define NEMOCALENDARDATA_H

#include <QString>

// KCalCore
#include <attendee.h>
#include <KDateTime>

#include "calendarevent.h"

namespace NemoCalendarData {

struct EventOccurrence {
    QString eventUid;
    KDateTime recurrenceId;
    QDateTime startTime;
    QDateTime endTime;

    QString getId() const
    {
        return QString("%1-%2").arg(eventUid).arg(startTime.toMSecsSinceEpoch());
    }
};

struct Event {
    QString displayLabel;
    QString description;
    KDateTime startTime;
    KDateTime endTime;
    bool allDay;
    NemoCalendarEvent::Recur recur;
    QDate recurEndDate;
    NemoCalendarEvent::Reminder reminder;
    QString uniqueId;
    KDateTime recurrenceId;
    bool readonly;
    QString location;
    QString calendarUid;

    bool operator==(const Event& other) const
    {
        return uniqueId == other.uniqueId;
    }

    bool isValid() const
    {
        return !uniqueId.isEmpty();
    }
};

struct Notebook {
    QString name;
    QString uid;
    QString description;
    QString color;
    bool isDefault;
    bool readOnly;
    bool localCalendar;
    bool excluded;

    Notebook() : isDefault(false), readOnly(false), localCalendar(false), excluded(false) { }

    bool operator==(const Notebook other) const
    {
        return uid == other.uid && name == other.name && description == other.description &&
                color == other.color && isDefault == other.isDefault && readOnly == other.readOnly &&
                localCalendar == other.localCalendar && excluded == other.excluded;
    }

    bool operator!=(const Notebook other) const
    {
        return !operator==(other);
    }
};

typedef QPair<QDate,QDate> Range;

struct Attendee {
    bool isOrganizer;
    QString name;
    QString email;
    KCalCore::Attendee::Role participationRole;
};

}
#endif // NEMOCALENDARDATA_H
