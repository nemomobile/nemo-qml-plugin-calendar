#ifndef CALENDARNOTEBOOKQUERY_H
#define CALENDARNOTEBOOKQUERY_H

#include <QObject>
#include "calendardata.h"

class NemoCalendarNotebookQuery : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString targetUid READ targetUid WRITE setTargetUid NOTIFY targetUidChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString description READ description NOTIFY descriptionChanged)
    Q_PROPERTY(QString color READ color NOTIFY colorChanged)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY isDefaultChanged)
    Q_PROPERTY(bool localCalendar READ localCalendar NOTIFY localCalendarChanged)
    Q_PROPERTY(bool isReadOnly READ isReadOnly NOTIFY isReadOnlyChanged)

public:
    explicit NemoCalendarNotebookQuery(QObject *parent = 0);
    ~NemoCalendarNotebookQuery();

    QString targetUid() const;
    void setTargetUid(const QString &target);

    bool isValid() const;
    QString name() const;
    QString description() const;
    QString color() const;
    bool isDefault() const;
    bool localCalendar() const;
    bool isReadOnly() const;

signals:
    void targetUidChanged();
    void isValidChanged();
    void nameChanged();
    void descriptionChanged();
    void colorChanged();
    void isDefaultChanged();
    void localCalendarChanged();
    void isReadOnlyChanged();

private slots:
    void updateData();

private:
    NemoCalendarData::Notebook m_notebook;
    QString m_targetUid;
    bool m_isValid;
};

#endif
