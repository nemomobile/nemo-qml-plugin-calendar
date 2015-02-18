#ifndef EVENTDATA_H
#define EVENTDATA_H

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtDBus/QDBusMetaType>

struct EventData {
    QString displayLabel;
    QString description;
    QString startTime;
    QString endTime;
    QString recurrenceId;
    bool allDay;
    QString location;
    QString calendarUid;
    QString uniqueId;
    QString color;
};
Q_DECLARE_METATYPE(EventData)

QDBusArgument &operator<<(QDBusArgument &argument, const EventData &eventData);
const QDBusArgument &operator>>(const QDBusArgument &argument, EventData &eventData);

typedef QList<EventData> EventDataList;
Q_DECLARE_METATYPE(EventDataList)

inline void registerDataTypes() {
    qDBusRegisterMetaType<EventData>();
    qDBusRegisterMetaType<EventDataList>();
}

#endif // EVENTDATA_H
