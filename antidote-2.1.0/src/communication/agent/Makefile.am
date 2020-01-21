noinst_LTLIBRARIES = libagent.la

INCLUDES =  -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/src -I$(top_srcdir)/src

libagent_la_SOURCES = agent_ops.c \
					agent_association.c \
					agent_operating.c \
					agent_disassociating.c \
					agent_configuring.c \
					agent_fsm.c

noinst_HEADERS = agent_ops.h \
					agent_association.h \
					agent_operating.h \
					agent_disassociating.h \
					agent_configuring.h \
					agent_fsm.h