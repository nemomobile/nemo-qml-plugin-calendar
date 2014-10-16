#ifndef CALENDARCHANGEINFORMATION_H
#define CALENDARCHANGEINFORMATION_H

#include <QObject>
#include <QString>

#include <KDateTime>

class NemoCalendarChangeInformation : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool pending READ pending NOTIFY pendingChanged)
    Q_PROPERTY(QString uniqueId READ uniqueId NOTIFY uniqueIdChanged)
    Q_PROPERTY(QString recurrenceId READ recurrenceId NOTIFY recurrenceIdChanged)

public:
    explicit NemoCalendarChangeInformation(QObject *parent = 0);
    virtual ~NemoCalendarChangeInformation();

    void setInformation(const QString &uniqueId, const KDateTime &recurrenceId);
    bool pending();
    QString uniqueId();
    QString recurrenceId();

signals:
    void pendingChanged();
    void uniqueIdChanged();
    void recurrenceIdChanged();

private:
    bool m_pending;
    QString m_uniqueId;
    KDateTime m_recurrenceId;
};

#endif
