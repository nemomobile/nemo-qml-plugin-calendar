#include "calendarnotebookquery.h"
#include "calendarmanager.h"

NemoCalendarNotebookQuery::NemoCalendarNotebookQuery(QObject *parent) :
    QObject(parent),
    m_isValid(false)
{
    connect(NemoCalendarManager::instance(), SIGNAL(notebooksChanged(QList<NemoCalendarData::Notebook>)),
            this, SLOT(updateData()));
}

NemoCalendarNotebookQuery::~NemoCalendarNotebookQuery()
{
}

QString NemoCalendarNotebookQuery::targetUid() const
{
    return m_targetUid;
}

void NemoCalendarNotebookQuery::setTargetUid(const QString &target)
{
    if (target != m_targetUid) {
        m_targetUid = target;
        updateData();
        emit targetUidChanged();
    }
}

bool NemoCalendarNotebookQuery::isValid() const
{
    return m_isValid;
}

QString NemoCalendarNotebookQuery::name() const
{
    return m_notebook.name;
}

QString NemoCalendarNotebookQuery::description() const
{
    return m_notebook.description;
}

QString NemoCalendarNotebookQuery::color() const
{
    return m_notebook.color;
}

bool NemoCalendarNotebookQuery::isDefault() const
{
    return m_notebook.isDefault;
}

bool NemoCalendarNotebookQuery::localCalendar() const
{
    return m_notebook.localCalendar;
}

bool NemoCalendarNotebookQuery::isReadOnly() const
{
    return m_notebook.readOnly;
}

void NemoCalendarNotebookQuery::updateData()
{
    // fetch new info and signal changes
    QList<NemoCalendarData::Notebook> notebooks = NemoCalendarManager::instance()->notebooks();

    NemoCalendarData::Notebook notebook;
    bool found = false;

    for (int i = 0; i < notebooks.length(); ++i) {
        NemoCalendarData::Notebook current = notebooks.at(i);
        if (current.uid == m_targetUid) {
            notebook = current;
            found = true;
            break;
        }
    }

    bool nameUpdated = (notebook.name != m_notebook.name);
    bool descriptionUpdated = (notebook.description != m_notebook.description);
    bool colorUpdated = (notebook.color != m_notebook.color);
    bool isDefaultUpdated = (notebook.isDefault != m_notebook.isDefault);
    bool localCalendarUpdated = (notebook.localCalendar != m_notebook.localCalendar);
    bool isReadOnlyUpdated = (notebook.readOnly != m_notebook.readOnly);

    m_notebook = notebook;

    if (nameUpdated)
        emit nameChanged();
    if (descriptionUpdated)
        emit descriptionChanged();
    if (colorUpdated)
        emit colorChanged();
    if (isDefaultUpdated)
        emit isDefaultChanged();
    if (localCalendarUpdated)
        emit localCalendarChanged();
    if (isReadOnlyUpdated)
        emit isReadOnlyChanged();

    if (m_isValid != found) {
        m_isValid = found;
        emit isValidChanged();
    }
}
