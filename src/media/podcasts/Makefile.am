#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

ext_LTLIBRARIES = libgrlpodcasts.la

libgrlpodcasts_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GRLNET_CFLAGS)	\
	$(XML_CFLAGS)		\
	$(SQLITE_CFLAGS) 	\
	$(GMIME_CFLAGS)

libgrlpodcasts_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(GRLNET_LIBS)		\
	$(XML_LIBS)		\
	$(SQLITE_LIBS) 		\
	$(GMIME_LIBS)

libgrlpodcasts_la_LDFLAGS = \
	-no-undefined	    \
	-module		    \
	-avoid-version

libgrlpodcasts_la_SOURCES = grl-podcasts.c grl-podcasts.h

extdir				= $(GRL_PLUGINS_DIR)
podcastsxmldir		= $(GRL_PLUGINS_DIR)
podcastsxml_DATA	= $(PODCASTS_PLUGIN_ID).xml

EXTRA_DIST = $(podcastsxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
