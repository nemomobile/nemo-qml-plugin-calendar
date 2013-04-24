TARGET = nemocalendar
PLUGIN_IMPORT_PATH = org/nemomobile/calendar

TEMPLATE = lib
CONFIG += qt plugin hide_symbols
equals(QT_MAJOR_VERSION, 4): QT += declarative
equals(QT_MAJOR_VERSION, 5): QT += qml

QT -= gui

equals(QT_MAJOR_VERSION, 4): target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
equals(QT_MAJOR_VERSION, 5): target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

CONFIG += link_pkgconfig
equals(QT_MAJOR_VERSION, 4): PKGCONFIG += libkcalcoren libmkcal
equals(QT_MAJOR_VERSION, 5): PKGCONFIG += libkcalcoren-qt5 libmkcal-qt5

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
