TARGET = nemocalendar
PLUGIN_IMPORT_PATH = org/nemomobile/calendar

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

QT += qml concurrent
QT -= gui

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
PKGCONFIG += libkcalcoren-qt5 libmkcal-qt5 libical accounts-qt5

INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

CONFIG += link_pkgconfig

isEmpty(SRCDIR) SRCDIR = "."

SOURCES += \
    $$SRCDIR/plugin.cpp \
    $$SRCDIR/calendarevent.cpp \
    $$SRCDIR/calendareventoccurrence.cpp \
    $$SRCDIR/calendaragendamodel.cpp \
    $$SRCDIR/calendarapi.cpp \
    $$SRCDIR/calendareventquery.cpp \
    $$SRCDIR/calendarnotebookmodel.cpp \
    $$SRCDIR/calendarmanager.cpp \
    $$SRCDIR/calendarworker.cpp \
    $$SRCDIR/calendarnotebookquery.cpp \
    $$SRCDIR/calendareventmodification.cpp \
    $$SRCDIR/calendarchangeinformation.cpp \
    $$SRCDIR/calendarutils.cpp \
    $$SRCDIR/calendarimportmodel.cpp \
    $$SRCDIR/calendarimportevent.cpp

HEADERS += \
    $$SRCDIR/calendarevent.h \
    $$SRCDIR/calendareventoccurrence.h \
    $$SRCDIR/calendaragendamodel.h \
    $$SRCDIR/calendarapi.h \
    $$SRCDIR/calendareventquery.h \
    $$SRCDIR/calendarnotebookmodel.h \
    $$SRCDIR/calendarmanager.h \
    $$SRCDIR/calendarworker.h \
    $$SRCDIR/calendardata.h \
    $$SRCDIR/calendarnotebookquery.h \
    $$SRCDIR/calendareventmodification.h \
    $$SRCDIR/calendarchangeinformation.h \
    $$SRCDIR/calendarutils.h \
    $$SRCDIR/calendarimportmodel.h \
    $$SRCDIR/calendarimportevent.h

MOC_DIR = $$PWD/.moc
OBJECTS_DIR = $$PWD/.obj
