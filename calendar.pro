TEMPLATE = subdirs
SUBDIRS = src tests lightweight tools

tests.depends = src
tools.depends = src

OTHER_FILES += rpm/*
