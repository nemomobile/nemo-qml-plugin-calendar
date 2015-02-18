#include "eventdata.h"

QDBusArgument &operator<<(QDBusArgument &argument, const EventData &eventData)
{
    argument.beginStructure();
    argument << eventData.displayLabel << eventData.description << eventData.startTime
             << eventData.endTime << eventData.recurrenceId << eventData.allDay
             << eventData.location << eventData.calendarUid << eventData.uniqueId
             << eventData.color;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, EventData &eventData)
{
    argument.beginStructure();
    argument >> eventData.displayLabel >> eventData.description >> eventData.startTime
             >> eventData.endTime >> eventData.recurrenceId >> eventData.allDay
             >> eventData.location >> eventData.calendarUid >> eventData.uniqueId
             >> eventData.color;
    argument.endStructure();
    return argument;
}
