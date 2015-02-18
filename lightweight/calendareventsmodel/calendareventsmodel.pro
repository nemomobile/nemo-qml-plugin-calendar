TARGET = nemocalendarwidget
PLUGIN_IMPORT_PATH = org/nemomobile/calendar/lightweight

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

QT += qml dbus
QT -= gui

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

isEmpty(SRCDIR) SRCDIR = "."
SOURCES += \
    calendardataserviceproxy.cpp \
    calendareventsmodel.cpp \
    ../common/eventdata.cpp \
    plugin.cpp

HEADERS += \
    calendardataserviceproxy.h \
    calendareventsmodel.h \
    ../common/eventdata.h

MOC_DIR = $$PWD/.moc
OBJECTS_DIR = $$PWD/.obj
