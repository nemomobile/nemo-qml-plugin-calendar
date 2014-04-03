TEMPLATE = subdirs
SUBDIRS = \
    tst_calendarmanager \
    tst_calendarevent

tests_xml.path = /opt/tests/nemo-qml-plugins-qt5/calendar
tests_xml.files = tests.xml
INSTALLS += tests_xml

OTHER_FILES += tests.xml
