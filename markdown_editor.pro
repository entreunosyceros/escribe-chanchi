QT += core widgets gui printsupport network

CONFIG += c++11

TARGET = markdown_editor
TEMPLATE = app

SOURCES += main.cpp \
           markdow_editor.cpp \
           markdown_formatter.cpp \
           template_manager.cpp

HEADERS += markdow_editor.h \
           markdown_formatter.h \
           template_manager.h

LIBS += -lcmark
