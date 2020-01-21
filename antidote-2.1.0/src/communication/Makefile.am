SUBDIRS = parser plugin agent common manager

INCLUDES =  -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/src -I$(top_srcdir)/src

noinst_LTLIBRARIES = libcom.la

libcom_la_SOURCES = communication.c \
                   fsm.c 

noinst_HEADERS = communication.h \
                 fsm.h