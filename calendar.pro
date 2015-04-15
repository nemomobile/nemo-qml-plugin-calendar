TEMPLATE = subdirs
SUBDIRS = src tests lightweight

tests.depends = src

OTHER_FILES += rpm/* qmldir
