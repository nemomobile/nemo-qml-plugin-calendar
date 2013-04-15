TARGET = nemocalendar
PLUGIN_IMPORT_PATH = org/nemomobile/calendar

TEMPLATE = lib
CONFIG += qt plugin hide_symbols
QT += declarative

target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$[QT_INSTALL_IMPORTS]/$$$$PLUGIN_IMPORT_PATH
INSTALLS += qmldir

CONFIG += link_pkgconfig
PKGCONFIG += libkcalcoren libmkcal

SOURCES += \
    plugin.cpp \
    calendarabstractmodel.cpp \
    calendarevent.cpp \
    calendaragendamodel.cpp \
    calendardb.cpp \
    calendareventcache.cpp

HEADERS += \
    calendarevent.h \
    calendarabstractmodel.h \
    calendaragendamodel.h \
    calendardb.h \
    calendareventcache.h

MOC_DIR = $$PWD/.moc
OBJECTS_DIR = $$PWD/.obj
