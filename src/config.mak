# -*- makefile -*-
# --------------------------------------------------------------------
#
# Ipe configuration for Unix
#
# *** This File is NOT USED on MAC OS X ***
#
# ------------------------------------------------------------------
# Include and linking options for libraries
# ------------------------------------------------------------------
#
# We just query "pkg-config" for the correct flags.  If this doesn't
# work on your system, enter the correct linker flags and directories
# directly.
#
# The name of the Lua package (it could be "lua", "lua52", or "lua5.2")
# Lua 5.3 would work as well.
#
LUA_PACKAGE   ?= lua5.2
#
ZLIB_CFLAGS   ?=
ZLIB_LIBS     ?= -lz
JPEG_CFLAGS   ?=
JPEG_LIBS     ?= -ljpeg
PNG_CFLAGS    ?= $(shell pkg-config --cflags libpng)
PNG_LIBS      ?= $(shell pkg-config --libs libpng)
FREETYPE_CFLAGS ?= $(shell pkg-config --cflags freetype2)
FREETYPE_LIBS ?= $(shell pkg-config --libs freetype2)
CAIRO_CFLAGS  ?= $(shell pkg-config --cflags cairo)
CAIRO_LIBS    ?= $(shell pkg-config --libs cairo)
LUA_CFLAGS    ?= $(shell pkg-config --cflags $(LUA_PACKAGE))
LUA_LIBS      ?= $(shell pkg-config --libs $(LUA_PACKAGE))
QT_CFLAGS     ?= $(shell pkg-config --cflags QtGui QtCore)
QT_LIBS	      ?= $(shell pkg-config --libs QtGui QtCore)
#
# Library needed to use dlopen/dlsym/dlclose calls
#
DL_LIBS       ?= -ldl
#
# MOC is the Qt meta-object compiler.
# Make sure it's the right one for Qt5.
MOC	      ?= moc
#
# --------------------------------------------------------------------
#
# The C++ compiler
# I'm testing with g++ and clang++.
#
CXX = g++
#
# Special compilation flags for compiling shared libraries
# 64-bit Linux requires shared libraries to be compiled as
# position independent code, that is -fpic or -fPIC
# Qt5 seems to require -fPIC
DLL_CFLAGS = -fPIC
#
# --------------------------------------------------------------------
#
# Installing Ipe:
#
IPEVERS = 7.2.5
#
# IPEPREFIX is the global prefix for the Ipe directory structure, which
# you can override individually for any of the specific directories.
# You could choose "/usr/local" or "/opt/ipe7", or
# even "/usr", or "$(HOME)/ipe7" if you have to install in your home
# directory.
#
# If you are installing Ipe in a networked environment, keep in mind
# that executables, ipelets, and Ipe library are machine-dependent,
# while the documentation and fonts can be shared.
#
#IPEPREFIX  := /usr/local
#IPEPREFIX  := /usr
#IPEPREFIX  := /opt/ipe7
IPEPREFIX  := $(DESTDIR)
#
#ifeq "$(IPEPREFIX)" ""
#$(error You need to specify IPEPREFIX!)
#endif
#
# Where Ipe executables will be installed ('ipe', 'ipetoipe' etc)
IPEBINDIR  = $(IPEPREFIX)/bin
#
# Where the Ipe libraries will be installed ('libipe.so' etc.)
IPELIBDIR  = $(IPEPREFIX)/lib
#
# Where the header files for Ipelib will be installed:
IPEHEADERDIR = $(IPEPREFIX)/include
#
# Where Ipelets will be installed:
IPELETDIR = $(IPEPREFIX)/lib/ipe/$(IPEVERS)/ipelets
#
# Where Lua code will be installed
# (This is the part of the Ipe program written in the Lua language)
IPELUADIR = $(IPEPREFIX)/share/ipe/$(IPEVERS)/lua
#
# Directory where Ipe will look for scripts
# (standard scripts will also be installed here)
IPESCRIPTDIR = $(IPEPREFIX)/share/ipe/$(IPEVERS)/scripts
#
# Directory where Ipe will look for style files
# (standard Ipe styles will also be installed here)
IPESTYLEDIR = $(IPEPREFIX)/share/ipe/$(IPEVERS)/styles
#
# IPEICONDIR contains the icons used in the Ipe user interface
#
IPEICONDIR = $(IPEPREFIX)/share/ipe/$(IPEVERS)/icons
#
# IPEDOCDIR contains the Ipe documentation (mostly html files)
#
IPEDOCDIR = $(IPEPREFIX)/share/ipe/$(IPEVERS)/doc
#
# The Ipe manual pages are installed into IPEMANDIR
#
IPEMANDIR = $(IPEPREFIX)/share/man/man1
#
# --------------------------------------------------------------------
