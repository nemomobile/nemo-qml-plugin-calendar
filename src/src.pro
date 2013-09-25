TARGET = nemocalendar
PLUGIN_IMPORT_PATH = org/nemomobile/calendar

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

equals(QT_MAJOR_VERSION, 4) {
    QT += declarative
    target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
    PKGCONFIG += libkcalcoren libmkcal libical
}

equals(QT_MAJOR_VERSION, 5) {
    QT += qml
    target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
    PKGCONFIG += libkcalcoren-qt5 libmkcal-qt5 libical

    SOURCES += \
        calendarapi.cpp \
        calendareventquery.cpp \
        calendarnotebookmodel.cpp \

    HEADERS += \
        calendarapi.cpp \
        calendarapi.h \
        calendareventquery.h \
        calendarnotebookmodel.h \

    DEFINES += NEMO_USE_QT5
}

QT -= gui

INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

CONFIG += link_pkgconfig

SOURCES += \
    plugin.cpp \
    calendarevent.cpp \
    calendaragendamodel.cpp \
    calendardb.cpp \
    calendareventcache.cpp \

HEADERS += \
    calendarevent.h \
    calendaragendamodel.h \
    calendardb.h \
    calendareventcache.h \

MOC_DIR = $$PWD/.moc
OBJECTS_DIR = $$PWD/.obj
