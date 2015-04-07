TEMPLATE = app
TARGET = calendardataservice
target.path = /usr/bin

QT += qml dbus
QT -= gui

CONFIG += link_pkgconfig
PKGCONFIG += libkcalcoren-qt5 libmkcal-qt5 libical accounts-qt5

HEADERS += \
    calendardataservice.h \
    calendardataserviceadaptor.h \
    ../common/eventdata.h \
    ../../src/calendaragendamodel.h \
    ../../src/calendarmanager.h \
    ../../src/calendarworker.h \
    ../../src/calendareventoccurrence.h \
    ../../src/calendarevent.h \
    ../../src/calendarchangeinformation.h \
    ../../src/calendareventquery.h \
    ../../src/calendarutils.h

SOURCES += \
    calendardataservice.cpp \
    calendardataserviceadaptor.cpp \
    ../common/eventdata.cpp \
    ../../src/calendaragendamodel.cpp \
    ../../src/calendarmanager.cpp \
    ../../src/calendarworker.cpp \
    ../../src/calendareventoccurrence.cpp \
    ../../src/calendarevent.cpp \
    ../../src/calendarchangeinformation.cpp \
    ../../src/calendareventquery.cpp \
    ../../src/calendarutils.cpp \
    main.cpp

dbus_service.path = /usr/share/dbus-1/services/
dbus_service.files = org.nemomobile.calendardataservice.service

INSTALLS += target dbus_service

OTHER_FILES += *.service *.xml
