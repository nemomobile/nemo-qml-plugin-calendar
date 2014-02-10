TARGET = nemocalendar
PLUGIN_IMPORT_PATH = org/nemomobile/calendar

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

QT += qml
QT -= gui

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
PKGCONFIG += libkcalcoren-qt5 libmkcal-qt5 libical

INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

CONFIG += link_pkgconfig

SOURCES += \
    plugin.cpp \
    calendarevent.cpp \
    calendareventoccurrence.cpp \
    calendaragendamodel.cpp \
    calendardb.cpp \
    calendareventcache.cpp \
    calendarapi.cpp \
    calendareventquery.cpp \
    calendarnotebookmodel.cpp \

HEADERS += \
    calendarevent.h \
    calendareventoccurrence.h \
    calendaragendamodel.h \
    calendardb.h \
    calendareventcache.h \
    calendarapi.h \
    calendareventquery.h \
    calendarnotebookmodel.h \

MOC_DIR = $$PWD/.moc
OBJECTS_DIR = $$PWD/.obj
