TEMPLATE = subdirs
SUBDIRS = \
    tst_calendarmanager

tests_xml.path = /opt/tests/nemo-qml-plugins-qt5/calendar
tests_xml.files = tests.xml
INSTALLS += tests_xml

OTHER_FILES += tests.xml
