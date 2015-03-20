#include "eventdata.h"

QDBusArgument &operator<<(QDBusArgument &argument, const EventData &eventData)
{
    argument.beginStructure();
    argument << eventData.calendarUid
             << eventData.uniqueId
             << eventData.recurrenceId
             << eventData.startTime
             << eventData.endTime
             << eventData.allDay
             << eventData.color
             << eventData.displayLabel
             << eventData.description
             << eventData.location;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, EventData &eventData)
{
    argument.beginStructure();
    argument >> eventData.calendarUid
             >> eventData.uniqueId
             >> eventData.recurrenceId
             >> eventData.startTime
             >> eventData.endTime
             >> eventData.allDay
             >> eventData.color
             >> eventData.displayLabel
             >> eventData.description
             >> eventData.location;
    argument.endStructure();
    return argument;
}
