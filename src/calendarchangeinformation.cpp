#include "calendarchangeinformation.h"
#include <QDebug>

NemoCalendarChangeInformation::NemoCalendarChangeInformation(QObject *parent) :
    QObject(parent), m_pending(true)
{
}

NemoCalendarChangeInformation::~NemoCalendarChangeInformation()
{
}

void NemoCalendarChangeInformation::setInformation(const QString &uniqueId, const KDateTime &recurrenceId)
{
    m_uniqueId = uniqueId;
    m_recurrenceId = recurrenceId;
    m_pending = false;

    emit uniqueIdChanged();
    emit recurrenceIdChanged();
    emit pendingChanged();
}

bool NemoCalendarChangeInformation::pending()
{
    return m_pending;
}

QString NemoCalendarChangeInformation::uniqueId()
{
    return m_uniqueId;
}

QString NemoCalendarChangeInformation::recurrenceId()
{
    return m_recurrenceId.toString();
}
