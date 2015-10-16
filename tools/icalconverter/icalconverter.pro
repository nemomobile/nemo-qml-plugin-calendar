TEMPLATE=app
TARGET=icalconverter
QT-=gui
CONFIG += link_pkgconfig
PKGCONFIG += libkcalcoren-qt5 libmkcal-qt5
QMAKE_CXXFLAGS += -fPIE -fvisibility=hidden -fvisibility-inlines-hidden
SOURCES+=main.cpp

target.path = $$INSTALL_ROOT/usr/bin/
INSTALLS+=target
