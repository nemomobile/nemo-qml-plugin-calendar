#ifndef EVENTDATA_H
#define EVENTDATA_H

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtDBus/QDBusMetaType>

struct EventData {
    QString calendarUid;
    QString uniqueId;
    QString recurrenceId;
    QString startTime;
    QString endTime;
    bool allDay;
    QString color;
    QString displayLabel;
    QString description;
    QString location;

};
Q_DECLARE_METATYPE(EventData)

QDBusArgument &operator<<(QDBusArgument &argument, const EventData &eventData);
const QDBusArgument &operator>>(const QDBusArgument &argument, EventData &eventData);

typedef QList<EventData> EventDataList;
Q_DECLARE_METATYPE(EventDataList)

inline void registerCalendarDataServiceTypes() {
    qDBusRegisterMetaType<EventData>();
    qDBusRegisterMetaType<EventDataList>();
}

#endif // EVENTDATA_H
